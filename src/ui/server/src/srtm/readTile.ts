// Leitura de tiles SRTM (.hgt.gz) -- espelha o que app/src/app/TerrainData.cpp e
// TerrainQuery.cpp ja fazem em C++ (ver CLAUDE.md, secao "Terreno" e "Decima
// primeira/segunda passada"), reimplementado em Node puro (zlib embutido) sem
// escrever o .hgt descomprimido em disco -- o backend e so-leitura por decisao
// de projeto, e decodificar em memoria evita duplicar 12-25 MB por tile a toa.
import { readFileSync, readdirSync } from 'node:fs';
import { gunzipSync } from 'node:zlib';
import path from 'node:path';
import { PATHS } from '../repoPaths.js';
import type { TerrainTileInfo } from '../../../shared/apiTypes.js';

// Tamanhos validos em bytes (SrtmHgtFile.cpp): SRTM1 = 3601x3601x2, SRTM3 = 1201x1201x2.
const SRTM1_BYTES = 25_934_402;
const SRTM3_BYTES = 2_884_802;

/** Sentinela de "void" (sem dado) do formato SRTM -- qualquer coisa abaixo disso e lixo, nao elevacao. */
export const VOID_THRESHOLD_M = -1000;

export interface DecodedTile {
  name: string;
  gridSize: 1201 | 3601;
  resolution: 'SRTM1' | 'SRTM3';
  bounds: { swLat: number; swLon: number; neLat: number; neLon: number };
  /** grid[row][col], row 0 = norte (lat mais alta), col 0 = oeste (lon mais baixa) -- convencao .hgt padrao */
  elevationM: Int16Array; // tamanho gridSize*gridSize, indexado [row*gridSize + col]
}

/**
 * Nome do tile lido pelos ultimos 11 caracteres do arquivo, convencao USGS:
 * "S23W043.hgt" = canto SW em lat -23, lon -43 (celula de 1x1 grau).
 * Mesma regra usada por SrtmHgtFile::determineSrtmInfo() do lado C++.
 */
export function parseSwCorner(tileName: string): { swLat: number; swLon: number } {
  const m = /^([NS])(\d{2})([EW])(\d{3})$/.exec(tileName);
  if (!m) throw new Error(`nome de tile fora do padrao USGS: ${tileName}`);
  const [, ns, latStr, ew, lonStr] = m;
  const lat = Number(latStr) * (ns === 'S' ? -1 : 1);
  const lon = Number(lonStr) * (ew === 'W' ? -1 : 1);
  return { swLat: lat, swLon: lon };
}

function decodeGzippedHgt(gz: Buffer, tileName: string): DecodedTile {
  const raw = gunzipSync(gz);
  let gridSize: 1201 | 3601;
  let resolution: 'SRTM1' | 'SRTM3';
  if (raw.length === SRTM1_BYTES) {
    gridSize = 3601;
    resolution = 'SRTM1';
  } else if (raw.length === SRTM3_BYTES) {
    gridSize = 1201;
    resolution = 'SRTM3';
  } else {
    throw new Error(
      `tamanho inesperado para ${tileName}.hgt: ${raw.length} bytes ` +
        `(esperado ${SRTM3_BYTES} para SRTM3 ou ${SRTM1_BYTES} para SRTM1)`,
    );
  }

  const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  const n = gridSize * gridSize;
  const elevationM = new Int16Array(n);
  for (let i = 0; i < n; i++) {
    elevationM[i] = view.getInt16(i * 2, false); // big-endian
  }

  const { swLat, swLon } = parseSwCorner(tileName);
  return {
    name: tileName,
    gridSize,
    resolution,
    bounds: { swLat, swLon, neLat: swLat + 1, neLon: swLon + 1 },
    elevationM,
  };
}

// Cache de processo -- decodificar um SRTM1 de 3601x3601 e caro o bastante pra
// nao repetir por requisicao (mesma estrategia de tileRepository() em
// app/src/app/TerrainQuery.cpp).
const tileCache = new Map<string, DecodedTile>();

export function listAvailableTileNames(): string[] {
  let entries: string[];
  try {
    entries = readdirSync(PATHS.terrainDir);
  } catch {
    return [];
  }
  return entries.filter((f) => f.endsWith('.hgt.gz')).map((f) => f.replace(/\.hgt\.gz$/, ''));
}

export function loadTile(tileName: string): DecodedTile {
  const cached = tileCache.get(tileName);
  if (cached) return cached;

  const gzPath = path.join(PATHS.terrainDir, `${tileName}.hgt.gz`);
  const gz = readFileSync(gzPath);
  const decoded = decodeGzippedHgt(gz, tileName);
  tileCache.set(tileName, decoded);
  return decoded;
}

/**
 * true se o tile veio de dado SRTM real (nao sintetico) -- hoje so S23W043,
 * conforme shared/data/terrain/srtm/README.md. Mantido como lista curta e
 * explicita em vez de tentar inferir "realidade" do conteudo.
 */
const REAL_TILE_NAMES = new Set(['S23W043']);

export function listTileInfos(): TerrainTileInfo[] {
  return listAvailableTileNames().map((name) => {
    const { swLat, swLon } = parseSwCorner(name);
    // Evita decodificar o grid inteiro so pra listar metadados -- resolucao/gridSize
    // sao inferidos pela tabela conhecida (todos os tiles do repo sao 1x1 grau, e a
    // unica ambiguidade real seria o tamanho em bytes, que so importa ao decodificar).
    const real = REAL_TILE_NAMES.has(name);
    return {
      name,
      resolution: real ? 'SRTM1' : 'SRTM3',
      gridSize: real ? 3601 : 1201,
      bounds: { swLat, swLon, neLat: swLat + 1, neLon: swLon + 1 },
      real,
    };
  });
}

/** Amostra a elevacao (metros) no lat/lon dado, ou null se fora do tile ou void. */
export function sampleElevation(tile: DecodedTile, lat: number, lon: number): number | null {
  const { swLat, swLon, neLat, neLon } = tile.bounds;
  if (lat < swLat || lat > neLat || lon < swLon || lon > neLon) return null;

  const fracLat = (lat - swLat) / (neLat - swLat); // 0 (sul) .. 1 (norte)
  const fracLon = (lon - swLon) / (neLon - swLon); // 0 (oeste) .. 1 (leste)
  const row = Math.round((1 - fracLat) * (tile.gridSize - 1)); // row 0 = norte
  const col = Math.round(fracLon * (tile.gridSize - 1));
  const value = tile.elevationM[row * tile.gridSize + col] ?? VOID_THRESHOLD_M - 1;
  if (value < VOID_THRESHOLD_M) return null;
  return value;
}
