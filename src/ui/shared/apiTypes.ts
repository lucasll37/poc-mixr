// Contratos das rotas /api, compartilhados entre web/ (Vite/Bundler resolution,
// sem sufixo de import) e server/ (NodeNext, importado como "../shared/apiTypes.js").
// So tipos -- nenhum valor/runtime aqui.

export interface AircraftModelInfo {
  /** nome da pasta em models/plugins/data/flight/jsbsim/aircraft/, ex: "c310", "aim1" */
  model: string;
  /** caminho relativo a raiz do repo, ex: "./dist/share/mixr-plugins/flight/jsbsim/" */
  rootDir: string;
}

export interface TerrainTileInfo {
  /** nome do tile, ex: "S23W043" (sem extensao) */
  name: string;
  resolution: 'SRTM1' | 'SRTM3';
  gridSize: 1201 | 3601;
  bounds: { swLat: number; swLon: number; neLat: number; neLon: number };
  /** true se o .hgt.gz de origem e um tile real (nao sintetico) -- ver shared/data/terrain/srtm/README.md */
  real: boolean;
}

export interface ContourFeatureProperties {
  elevationM: number;
}

export interface ContourGeoJson {
  type: 'FeatureCollection';
  features: Array<{
    type: 'Feature';
    properties: ContourFeatureProperties;
    geometry: { type: 'LineString'; coordinates: Array<[number, number]> }; // [lon, lat]
  }>;
}

export interface ScenarioFileInfo {
  /** caminho relativo a raiz do repo, ex: "src/poc/single-thread/configs/scenario.epp.in" */
  path: string;
  /** subprojeto de origem, so para agrupar na UI */
  group: 'single-thread' | 'multi-thread' | 'bandit-dis' | 'app' | 'outro';
  sizeBytes: number;
}

export interface ScenarioFileContent {
  path: string;
  text: string;
}

export interface BehaviorTreeInfo {
  path: string;
  xml: string;
}

export interface ApiErrorBody {
  error: string;
}
