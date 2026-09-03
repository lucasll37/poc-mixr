import { Router } from 'express';
import { readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import { PATHS } from '../repoPaths.js';
import type { AircraftModelInfo } from '../../../shared/apiTypes.js';

export const aircraftRouter = Router();

// rootDir: o caminho que os .epp REAIS usam em JSBSimModel.rootDir -- e o
// destino de instalacao (dist/share/mixr-plugins/flight/jsbsim/), nao a
// pasta de origem que este backend le (models/plugins/data/flight/jsbsim/) --
// ver CLAUDE.md, secao "models/plugins/" e "sync-plugins". Ler a lista de
// pastas da ORIGEM (sempre presente no git) mas emitir o caminho de DESTINO
// (o unico que faz sentido dentro de um .epp gerado).
const JSBSIM_ROOT_DIR_IN_EDL = './dist/share/mixr-plugins/flight/jsbsim/';

// GET /api/aircraft -- lista real de aeronaves JSBSim disponiveis, lida do
// disco (models/plugins/data/flight/jsbsim/aircraft/<nome>/) -- nunca uma
// lista hardcoded, mesmo que hoje sejam so 2 pastas (c310, aim1).
aircraftRouter.get('/', (_req, res) => {
  let entries: string[] = [];
  try {
    entries = readdirSync(PATHS.jsbsimAircraftDir).filter((name) => {
      try {
        return statSync(path.join(PATHS.jsbsimAircraftDir, name)).isDirectory();
      } catch {
        return false;
      }
    });
  } catch {
    entries = [];
  }

  const models: AircraftModelInfo[] = entries
    .sort()
    .map((model) => ({ model, rootDir: JSBSIM_ROOT_DIR_IN_EDL }));
  res.json(models);
});
