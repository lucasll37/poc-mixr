// Gera curvas de nivel (isolinhas) a partir de um tile SRTM decodificado, via
// marching squares. Roda no BACKEND (decisao do plano) -- o navegador nunca
// baixa o .hgt bruto.
//
// Simplificacao deliberada em relacao a um gerador de contorno "de livro":
// cada segmento de uma celula da grade e emitido como sua PROPRIA feature
// LineString de 2 pontos, em vez de encadear segmentos vizinhos numa
// polyline continua. Encadear exigiria costurar segmentos por proximidade de
// extremidade (grafo de adjacencia, cuidado com ciclos abertos/fechados) --
// complexidade real sem ganho visual: o MapLibre desenha um feixe de
// segmentos curtos com a MESMA aparencia de uma polyline longa, e o controle
// de payload fica so na densidade da subamostragem (targetSamples), nao no
// numero de features por polyline.
import type { ContourGeoJson } from '../../../shared/apiTypes.js';
import { VOID_THRESHOLD_M, type DecodedTile } from './readTile.js';

const ROUND_INTERVALS_M = [5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000];

/** Mesma heuristica de app/src/app/MapPanel.cpp::contourIntervalFor(): a primeira
 *  equidistancia "redonda" que cobre range/8 -- como um mapa topografico de papel
 *  escolhe a equidistancia pela escala, nao um numero fixo arbitrario. */
export function contourIntervalFor(minM: number, maxM: number): number {
  const range = Math.max(1, maxM - minM);
  const target = range / 8;
  for (const interval of ROUND_INTERVALS_M) {
    if (interval >= target) return interval;
  }
  return ROUND_INTERVALS_M[ROUND_INTERVALS_M.length - 1]!;
}

function rowColToLatLon(tile: DecodedTile, row: number, col: number): [lat: number, lon: number] {
  const { swLat, swLon, neLat, neLon } = tile.bounds;
  const fracLat = 1 - row / (tile.gridSize - 1); // row 0 = norte
  const fracLon = col / (tile.gridSize - 1); // col 0 = oeste
  return [swLat + fracLat * (neLat - swLat), swLon + fracLon * (neLon - swLon)];
}

function elevAt(tile: DecodedTile, row: number, col: number): number {
  return tile.elevationM[row * tile.gridSize + col]!;
}

function interp(
  tile: DecodedTile,
  rowA: number,
  colA: number,
  rowB: number,
  colB: number,
  level: number,
): [lon: number, lat: number] {
  const eA = elevAt(tile, rowA, colA);
  const eB = elevAt(tile, rowB, colB);
  const t = eB === eA ? 0.5 : (level - eA) / (eB - eA);
  const row = rowA + (rowB - rowA) * t;
  const col = colA + (colB - colA) * t;
  const [lat, lon] = rowColToLatLon(tile, row, col);
  return [lon, lat];
}

export interface ContourOptions {
  /** numero alvo de amostras por lado da grade (controla o custo/densidade) */
  targetSamples?: number;
  /** forca uma equidistancia especifica, em vez da heuristica automatica */
  intervalM?: number;
}

export function computeContours(tile: DecodedTile, opts: ContourOptions = {}): ContourGeoJson {
  const targetSamples = opts.targetSamples ?? 240;
  const step = Math.max(1, Math.floor((tile.gridSize - 1) / targetSamples));

  const rows: number[] = [];
  for (let r = 0; r < tile.gridSize; r += step) rows.push(r);
  if (rows[rows.length - 1] !== tile.gridSize - 1) rows.push(tile.gridSize - 1);
  const cols: number[] = [];
  for (let c = 0; c < tile.gridSize; c += step) cols.push(c);
  if (cols[cols.length - 1] !== tile.gridSize - 1) cols.push(tile.gridSize - 1);

  let minM = Infinity;
  let maxM = -Infinity;
  for (const r of rows) {
    for (const c of cols) {
      const e = elevAt(tile, r, c);
      if (e < VOID_THRESHOLD_M) continue;
      if (e < minM) minM = e;
      if (e > maxM) maxM = e;
    }
  }
  if (!Number.isFinite(minM) || !Number.isFinite(maxM)) {
    return { type: 'FeatureCollection', features: [] };
  }

  const interval = opts.intervalM ?? contourIntervalFor(minM, maxM);
  const firstLevel = Math.ceil(minM / interval) * interval;
  const levels: number[] = [];
  for (let level = firstLevel; level <= maxM; level += interval) levels.push(level);

  const features: ContourGeoJson['features'] = [];

  for (let ri = 0; ri < rows.length - 1; ri++) {
    for (let ci = 0; ci < cols.length - 1; ci++) {
      const r0 = rows[ri]!;
      const r1 = rows[ri + 1]!;
      const c0 = cols[ci]!;
      const c1 = cols[ci + 1]!;

      // corners: 00=topo-esquerda(norte-oeste) 10=topo-direita 01=baixo-esquerda 11=baixo-direita
      const e00 = elevAt(tile, r0, c0);
      const e10 = elevAt(tile, r0, c1);
      const e01 = elevAt(tile, r1, c0);
      const e11 = elevAt(tile, r1, c1);
      if (e00 < VOID_THRESHOLD_M || e10 < VOID_THRESHOLD_M || e01 < VOID_THRESHOLD_M || e11 < VOID_THRESHOLD_M) {
        continue; // celula toca area sem dado -- nao desenha contorno ali (honesto, nao inventa)
      }

      for (const level of levels) {
        const above = [e00 >= level, e10 >= level, e01 >= level, e11 >= level];
        const caseIndex = (above[0] ? 8 : 0) | (above[1] ? 4 : 0) | (above[3] ? 2 : 0) | (above[2] ? 1 : 0);
        if (caseIndex === 0 || caseIndex === 15) continue; // celula inteira de um so lado do nivel

        // pontos medios das 4 arestas da celula, calculados sob demanda
        const top = () => interp(tile, r0, c0, r0, c1, level); // aresta norte
        const bottom = () => interp(tile, r1, c0, r1, c1, level); // aresta sul
        const left = () => interp(tile, r0, c0, r1, c0, level); // aresta oeste
        const right = () => interp(tile, r0, c1, r1, c1, level); // aresta leste

        const segments: Array<[[number, number], [number, number]]> = [];
        switch (caseIndex) {
          case 1:
          case 14:
            segments.push([left(), bottom()]);
            break;
          case 2:
          case 13:
            segments.push([bottom(), right()]);
            break;
          case 3:
          case 12:
            segments.push([left(), right()]);
            break;
          case 4:
          case 11:
            segments.push([top(), right()]);
            break;
          case 5:
            segments.push([left(), top()], [bottom(), right()]);
            break;
          case 6:
          case 9:
            segments.push([top(), bottom()]);
            break;
          case 7:
          case 8:
            segments.push([left(), top()]);
            break;
          case 10:
            segments.push([top(), right()], [left(), bottom()]);
            break;
          default:
            break;
        }

        for (const [a, b] of segments) {
          features.push({
            type: 'Feature',
            properties: { elevationM: level },
            geometry: { type: 'LineString', coordinates: [a, b] },
          });
        }
      }
    }
  }

  return { type: 'FeatureCollection', features };
}
