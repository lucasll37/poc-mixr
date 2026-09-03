import { useEffect } from 'react';
import type maplibregl from 'maplibre-gl';
import { api } from '../../api/client';

/**
 * Adiciona/remove, por tile, uma fonte GeoJSON de curvas de nivel + uma layer
 * de linha estilizada por elevacao. Cada tile e uma fonte INDEPENDENTE (nao
 * funde grids de resolucao diferente, 3601 vs 1201, num so) -- pequena
 * descontinuidade visual na fronteira e aceitavel, mesma limitacao ja
 * existente no `app` (TUI).
 */
export function useContourLayer(map: maplibregl.Map | null, tileNames: string[], visible: boolean) {
  useEffect(() => {
    if (!map || !visible || tileNames.length === 0) return;

    let cancelled = false;
    const addedIds: string[] = [];

    (async () => {
      for (const tileName of tileNames) {
        try {
          const geojson = await api.getContours(tileName);
          if (cancelled) return;
          const sourceId = `contour-src-${tileName}`;
          const layerId = `contour-layer-${tileName}`;
          if (map.getSource(sourceId)) continue;
          map.addSource(sourceId, { type: 'geojson', data: geojson as GeoJSON.FeatureCollection });
          map.addLayer({
            id: layerId,
            type: 'line',
            source: sourceId,
            paint: {
              'line-color': '#8a5a2b',
              'line-width': 1,
              'line-opacity': 0.6,
            },
          });
          addedIds.push(sourceId, layerId);
        } catch {
          // tile sem contorno disponivel (ex: fora de shared/data/terrain/srtm/) -- degrada em silencio, nao trava o mapa
        }
      }
    })();

    return () => {
      cancelled = true;
      for (const tileName of tileNames) {
        const layerId = `contour-layer-${tileName}`;
        const sourceId = `contour-src-${tileName}`;
        if (map.getLayer(layerId)) map.removeLayer(layerId);
        if (map.getSource(sourceId)) map.removeSource(sourceId);
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [map, tileNames.join(','), visible]);
}
