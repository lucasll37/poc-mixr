import { Router } from 'express';
import { readFileSync, readdirSync, statSync, existsSync } from 'node:fs';
import path from 'node:path';
import { PATHS, toRepoRelative } from '../repoPaths.js';
import type { ScenarioFileInfo } from '../../../shared/apiTypes.js';

export const scenariosRouter = Router();

// Fragmentos de template (@include:nome@), hoje so em app/configs/fragments/
// -- ver app/src/app/ScenarioTemplate.cpp e edl/preprocessor.ts do frontend.
const FRAGMENTS_DIR = path.join(PATHS.root, 'app', 'configs', 'fragments');

function listScenarioFiles(): ScenarioFileInfo[] {
  const out: ScenarioFileInfo[] = [];
  for (const { group, dir } of PATHS.scenarioDirs) {
    let entries: string[];
    try {
      entries = readdirSync(dir);
    } catch {
      continue;
    }
    for (const name of entries) {
      if (!name.endsWith('.epp') && !name.endsWith('.epp.in')) continue;
      const abs = path.join(dir, name);
      let sizeBytes = 0;
      try {
        sizeBytes = statSync(abs).size;
      } catch {
        continue;
      }
      out.push({ path: toRepoRelative(abs), group, sizeBytes });
    }
  }
  return out.sort((a, b) => a.path.localeCompare(b.path));
}

// GET /api/scenarios -- lista real dos .epp/.epp.in existentes no repo.
scenariosRouter.get('/', (_req, res) => {
  res.json(listScenarioFiles());
});

// GET /api/scenarios/content?path=<caminho relativo ao repo>
// Restrito a caminhos que aparecem em listScenarioFiles() (nunca leitura
// arbitraria de arquivo, mesmo sendo um backend so-leitura).
scenariosRouter.get('/content', (req, res) => {
  const requested = String(req.query.path ?? '');
  const known = listScenarioFiles().find((f) => f.path === requested);
  if (!known) {
    res.status(404).json({ error: `cenario desconhecido: ${requested}` });
    return;
  }
  const abs = path.join(PATHS.root, requested.replace(/^\.\//, ''));
  const text = readFileSync(abs, 'utf8');
  res.json({ path: requested, text });
});

// GET /api/scenarios/fragment?name=tacview_recorder.epp.frag
// So basename (sem separador de diretorio) dentro de app/configs/fragments/
// -- restricao deliberada contra path traversal, mesmo sendo so-leitura.
scenariosRouter.get('/fragment', (req, res) => {
  const name = String(req.query.name ?? '');
  if (!name || name.includes('/') || name.includes('\\') || name.includes('..')) {
    res.status(400).json({ error: `nome de fragmento invalido: ${name}` });
    return;
  }
  const abs = path.join(FRAGMENTS_DIR, name);
  if (!existsSync(abs)) {
    res.status(404).json({ error: `fragmento desconhecido: ${name}` });
    return;
  }
  const text = readFileSync(abs, 'utf8');
  res.json({ path: toRepoRelative(abs), text });
});
