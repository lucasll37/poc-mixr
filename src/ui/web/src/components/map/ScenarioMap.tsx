import { useEffect, useRef, useState } from 'react';
import maplibregl from 'maplibre-gl';
import 'maplibre-gl/dist/maplibre-gl.css';
import { useScenarioStore } from '../../store/scenarioStore';
import { getReferenceLatLon, listPlayers } from '../../model/selectors';
import { getPlayerLatLon, withPlayerLatLon } from './playerPosition';
import { useContourLayer } from './ContourLayer';
import { api } from '../../api/client';
import type { TerrainTileInfo } from '../../../../shared/apiTypes';

// Base OSM publica -- so pra uso interno da PoC (baixo volume). Uso pesado ou
// producao de verdade deveria trocar por um tile server proprio -- ver README.md.
const OSM_STYLE: maplibregl.StyleSpecification = {
  version: 8,
  sources: {
    osm: {
      type: 'raster',
      tiles: ['https://tile.openstreetmap.org/{z}/{x}/{y}.png'],
      tileSize: 256,
      attribution: '&copy; OpenStreetMap contributors',
    },
  },
  layers: [{ id: 'osm', type: 'raster', source: 'osm' }],
};

export function ScenarioMap() {
  const containerRef = useRef<HTMLDivElement>(null);
  const mapRef = useRef<maplibregl.Map | null>(null);
  const markersRef = useRef<Map<string, maplibregl.Marker>>(new Map());
  const [mapReady, setMapReady] = useState(false);
  const [showTerrain, setShowTerrain] = useState(false);
  const [tiles, setTiles] = useState<TerrainTileInfo[]>([]);

  const scenarioDoc = useScenarioStore((s) => s.document);
  const selectedNodeId = useScenarioStore((s) => s.selectedNodeId);
  const select = useScenarioStore((s) => s.select);
  const updateSlot = useScenarioStore((s) => s.updateSlot);

  useEffect(() => {
    if (!containerRef.current || mapRef.current) return;
    const map = new maplibregl.Map({
      container: containerRef.current,
      style: OSM_STYLE,
      center: [-42.48, -22.25],
      zoom: 10,
    });
    map.addControl(new maplibregl.NavigationControl(), 'top-right');
    map.on('load', () => setMapReady(true));
    mapRef.current = map;
    return () => {
      map.remove();
      mapRef.current = null;
    };
  }, []);

  useEffect(() => {
    api.listTerrainTiles().then(setTiles).catch(() => setTiles([]));
  }, []);

  const ref = scenarioDoc ? getReferenceLatLon(scenarioDoc) : undefined;

  // recentraliza no ponto de referencia do cenario quando um novo documento e carregado
  useEffect(() => {
    if (mapReady && ref) mapRef.current?.jumpTo({ center: [ref.lon, ref.lat], zoom: 10 });
  }, [mapReady, ref?.lat, ref?.lon]);

  const tileNamesInRange = ref
    ? tiles.filter((t) => ref.lat >= t.bounds.swLat && ref.lat <= t.bounds.neLat && ref.lon >= t.bounds.swLon && ref.lon <= t.bounds.neLon).map((t) => t.name)
    : [];
  useContourLayer(mapReady ? mapRef.current : null, tileNamesInRange, showTerrain);

  // sincroniza os marcadores de player com o documento atual
  useEffect(() => {
    const map = mapRef.current;
    if (!map || !mapReady || !scenarioDoc || !ref) return;

    const players = listPlayers(scenarioDoc);
    const seen = new Set<string>();

    for (const player of players) {
      seen.add(player.node.id);
      const latLon = getPlayerLatLon(player.node, ref.lat, ref.lon);
      if (!latLon) continue;

      let marker = markersRef.current.get(player.node.id);
      if (!marker) {
        const el = window.document.createElement('div');
        el.className = 'scenario-map-marker';
        el.textContent = player.key;
        marker = new maplibregl.Marker({ element: el, draggable: true })
          .setLngLat([latLon.lon, latLon.lat])
          .addTo(map);
        // Marker (mapbox/maplibre) so emite eventos de DRAG via .on() -- 'click'
        // nao e um evento que o Marker dispara (Evented.on registraria um
        // listener que nunca roda); o jeito documentado e escutar no
        // elemento DOM da propria marker.
        el.addEventListener('click', () => select(player.node.id));
        marker.on('dragend', () => {
          const pos = marker!.getLngLat();
          const updates = withPlayerLatLon(player.node, pos.lat, pos.lng, ref.lat, ref.lon);
          for (const u of updates) updateSlot(player.node.id, u.slotKey, u.value);
        });
        markersRef.current.set(player.node.id, marker);
      } else {
        marker.setLngLat([latLon.lon, latLon.lat]);
      }
      const el = marker.getElement();
      el.classList.toggle('scenario-map-marker--selected', player.node.id === selectedNodeId);
    }

    for (const [id, marker] of markersRef.current) {
      if (!seen.has(id)) {
        marker.remove();
        markersRef.current.delete(id);
      }
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [scenarioDoc, mapReady, ref?.lat, ref?.lon, selectedNodeId]);

  return (
    <div className="scenario-map">
      <div className="scenario-map__toolbar">
        <label>
          <input type="checkbox" checked={showTerrain} onChange={(e) => setShowTerrain(e.target.checked)} />
          {' '}terreno (curvas de nivel)
        </label>
      </div>
      <div ref={containerRef} className="scenario-map__canvas" />
    </div>
  );
}
