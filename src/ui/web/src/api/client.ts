import type {
  AircraftModelInfo,
  BehaviorTreeInfo,
  ContourGeoJson,
  ScenarioFileContent,
  ScenarioFileInfo,
  TerrainTileInfo,
} from '../../../shared/apiTypes';

async function getJson<T>(url: string): Promise<T> {
  const res = await fetch(url);
  if (!res.ok) {
    const body = await res.json().catch(() => ({ error: res.statusText }));
    throw new Error(`GET ${url} -> ${res.status}: ${body.error ?? res.statusText}`);
  }
  return res.json() as Promise<T>;
}

export const api = {
  listScenarios: () => getJson<ScenarioFileInfo[]>('/api/scenarios'),
  getScenarioContent: (path: string) =>
    getJson<ScenarioFileContent>(`/api/scenarios/content?path=${encodeURIComponent(path)}`),
  getFragment: (name: string) => getJson<{ path: string; text: string }>(`/api/scenarios/fragment?name=${encodeURIComponent(name)}`),
  listAircraft: () => getJson<AircraftModelInfo[]>('/api/aircraft'),
  listTerrainTiles: () => getJson<TerrainTileInfo[]>('/api/terrain-tiles'),
  getContours: (tileName: string, opts?: { targetSamples?: number; intervalM?: number }) => {
    const params = new URLSearchParams();
    if (opts?.targetSamples) params.set('targetSamples', String(opts.targetSamples));
    if (opts?.intervalM) params.set('intervalM', String(opts.intervalM));
    const qs = params.toString();
    return getJson<ContourGeoJson>(`/api/terrain-tiles/${encodeURIComponent(tileName)}/contours${qs ? `?${qs}` : ''}`);
  },
  getBehaviorTree: (variant: 'default' | 'missile' = 'default') =>
    getJson<BehaviorTreeInfo>(`/api/behavior-tree?variant=${variant}`),
};
