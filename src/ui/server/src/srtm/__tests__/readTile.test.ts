import { describe, expect, it } from 'vitest';
import { listAvailableTileNames, loadTile, parseSwCorner, VOID_THRESHOLD_M } from '../readTile.js';

describe('leitura de tiles SRTM reais (shared/data/terrain/srtm/)', () => {
  it('lista os 4 tiles versionados no repo', () => {
    const names = listAvailableTileNames().sort();
    expect(names).toEqual(['S22W043', 'S22W044', 'S23W043', 'S23W044']);
  });

  it('parseSwCorner segue a convencao USGS (ultimos 11 caracteres)', () => {
    expect(parseSwCorner('S23W043')).toEqual({ swLat: -23, swLon: -43 });
    expect(parseSwCorner('N10E020')).toEqual({ swLat: 10, swLon: 20 });
  });

  it('S23W043 (real, SRTM1) decodifica como grid 3601x3601', () => {
    const tile = loadTile('S23W043');
    expect(tile.gridSize).toBe(3601);
    expect(tile.resolution).toBe('SRTM1');
    expect(tile.elevationM.length).toBe(3601 * 3601);
  });

  it('os 3 tiles sinteticos decodificam como grid 1201x1201', () => {
    for (const name of ['S22W043', 'S23W044', 'S22W044']) {
      const tile = loadTile(name);
      expect(tile.gridSize).toBe(1201);
      expect(tile.resolution).toBe('SRTM3');
    }
  });

  it('nenhum ponto do tile real S23W043 fica abaixo do sentinela de void perto do centro do cenario', () => {
    const tile = loadTile('S23W043');
    // ponto de referencia dos cenarios (latitude/longitude do WorldModel): -22.25/-42.48,
    // no meio da celula -- ver CLAUDE.md e o comentario em scenario.epp.in.
    const midRow = Math.round(((tile.bounds.neLat - -22.25) / 1) * (tile.gridSize - 1));
    const midCol = Math.round(((-42.48 - tile.bounds.swLon) / 1) * (tile.gridSize - 1));
    const value = tile.elevationM[midRow * tile.gridSize + midCol]!;
    expect(value).toBeGreaterThan(VOID_THRESHOLD_M);
  });
});
