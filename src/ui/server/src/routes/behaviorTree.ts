import { Router } from 'express';
import { readFileSync, existsSync } from 'node:fs';
import { PATHS, toRepoRelative } from '../repoPaths.js';

export const behaviorTreeRouter = Router();

// GET /api/behavior-tree?variant=default|missile
// So-leitura, so-referencia (a arvore de comportamento nao e editavel por
// este app -- ver plano, fase 5 "TreeViewer" e limite conhecido documentado
// no CLAUDE.md sobre correspondencia de rotulo/folha).
behaviorTreeRouter.get('/', (req, res) => {
  const variant = req.query.variant === 'missile' ? 'missile' : 'default';
  const abs = variant === 'missile' ? PATHS.behaviorTreeMissileXml : PATHS.behaviorTreeXml;
  if (!existsSync(abs)) {
    res.status(404).json({ error: `arvore de comportamento nao encontrada: ${toRepoRelative(abs)}` });
    return;
  }
  const xml = readFileSync(abs, 'utf8');
  res.json({ path: toRepoRelative(abs), xml });
});
