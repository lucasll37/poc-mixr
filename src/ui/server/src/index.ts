import express from 'express';
import path from 'node:path';
import { existsSync } from 'node:fs';
import { aircraftRouter } from './routes/aircraft.js';
import { terrainRouter } from './routes/terrain.js';
import { scenariosRouter } from './routes/scenarios.js';
import { behaviorTreeRouter } from './routes/behaviorTree.js';
import { REPO_ROOT } from './repoPaths.js';

const app = express();
const PORT = Number(process.env.PORT ?? 5175);

app.use('/api/aircraft', aircraftRouter);
app.use('/api/terrain-tiles', terrainRouter);
app.use('/api/scenarios', scenariosRouter);
app.use('/api/behavior-tree', behaviorTreeRouter);

// Em producao (npm run build && npm start), o mesmo processo serve os
// estaticos do front-end buildado -- em dev, o Vite (porta 5173) e quem
// serve a UI e faz proxy de /api pra ca (ver vite.config.ts).
const webDist = path.join(REPO_ROOT, 'src', 'ui', 'web', 'dist-app');
if (existsSync(webDist)) {
  app.use(express.static(webDist));
  app.get('*', (_req, res) => {
    res.sendFile(path.join(webDist, 'index.html'));
  });
}

app.listen(PORT, () => {
  // eslint-disable-next-line no-console
  console.log(`[poc-mixr-ui] backend so-leitura ouvindo em http://localhost:${PORT}`);
});
