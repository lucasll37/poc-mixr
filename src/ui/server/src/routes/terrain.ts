import { Router } from 'express';
import { listTileInfos, loadTile } from '../srtm/readTile.js';
import { computeContours } from '../srtm/contours.js';

export const terrainRouter = Router();

// GET /api/terrain-tiles -- lista os tiles reais disponiveis em shared/data/terrain/srtm/.
terrainRouter.get('/', (_req, res) => {
  res.json(listTileInfos());
});

// GET /api/terrain-tiles/:name/contours?targetSamples=&intervalM=
terrainRouter.get('/:name/contours', (req, res) => {
  const { name } = req.params;
  const known = listTileInfos().find((t) => t.name === name);
  if (!known) {
    res.status(404).json({ error: `tile desconhecido: ${name}` });
    return;
  }

  const targetSamples = req.query.targetSamples ? Number(req.query.targetSamples) : undefined;
  const intervalM = req.query.intervalM ? Number(req.query.intervalM) : undefined;

  try {
    const tile = loadTile(name!);
    const geojson = computeContours(tile, { targetSamples, intervalM });
    res.json(geojson);
  } catch (err) {
    res.status(500).json({ error: err instanceof Error ? err.message : String(err) });
  }
});
