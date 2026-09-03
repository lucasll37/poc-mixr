// Unica fonte de caminhos absolutos para a raiz do repositorio -- evita que
// cada rota resolva 'cwd' a propria maneira (fragil: depende de onde o
// processo 'npm run dev'/'node server/dist/...' foi disparado).
import { fileURLToPath } from 'node:url';
import { existsSync } from 'node:fs';
import path from 'node:path';

const HERE = path.dirname(fileURLToPath(import.meta.url));

/**
 * Acha a raiz do repo subindo diretorios ate achar o CLAUDE.md -- NAO conta
 * um numero fixo de "..". O tsconfig.server.json usa rootDir:'.' (pra poder
 * compilar server/src/ E shared/ juntos), entao o build real sai em
 * server/dist/server/src/repoPaths.js (uma camada a mais que o .ts em
 * server/src/repoPaths.ts) -- um numero fixo de "..", certo em dev (tsx
 * roda o .ts direto), quebraria em producao. Subir ate o marcador funciona
 * nos dois casos sem precisar saber a profundidade de antemao.
 */
function findRepoRoot(startDir: string): string {
  let dir = startDir;
  for (let i = 0; i < 10; i++) {
    if (existsSync(path.join(dir, 'CLAUDE.md'))) return dir;
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  throw new Error(`nao achei a raiz do repo (CLAUDE.md) subindo a partir de ${startDir}`);
}

export const REPO_ROOT = findRepoRoot(HERE);

export const PATHS = {
  root: REPO_ROOT,
  jsbsimAircraftDir: path.join(REPO_ROOT, 'models', 'plugins', 'data', 'flight', 'jsbsim', 'aircraft'),
  terrainDir: path.join(REPO_ROOT, 'shared', 'data', 'terrain', 'srtm'),
  behaviorTreeXml: path.join(REPO_ROOT, 'models', 'flight', 'configs', 'flight_tree.xml'),
  behaviorTreeMissileXml: path.join(REPO_ROOT, 'models', 'flight', 'configs', 'flight_tree_missile_demo.xml'),
  scenarioDirs: [
    { group: 'single-thread' as const, dir: path.join(REPO_ROOT, 'src', 'poc', 'single-thread', 'configs') },
    { group: 'multi-thread' as const, dir: path.join(REPO_ROOT, 'src', 'poc', 'multi-thread', 'configs') },
    { group: 'bandit-dis' as const, dir: path.join(REPO_ROOT, 'src', 'poc', 'bandit-dis', 'configs') },
    { group: 'app' as const, dir: path.join(REPO_ROOT, 'app', 'configs') },
  ],
};

/** Converte um caminho absoluto dentro do repo para o formato "./a/b/c" usado nos .epp. */
export function toRepoRelative(absolutePath: string): string {
  const rel = path.relative(REPO_ROOT, absolutePath).split(path.sep).join('/');
  return rel.startsWith('.') ? rel : `./${rel}`;
}

/**
 * Resolve um caminho relativo ao repo (ex: "./models/flight/configs/flight_tree.xml",
 * como aparece dentro de um .epp) para um caminho absoluto, sem permitir escapar da
 * raiz do repo (o backend e so-leitura, mas nao deve ler arquivo nenhum fora daqui).
 */
export function resolveRepoRelative(relativePath: string): string {
  const cleaned = relativePath.replace(/^\.\//, '');
  const abs = path.resolve(REPO_ROOT, cleaned);
  if (!abs.startsWith(REPO_ROOT + path.sep) && abs !== REPO_ROOT) {
    throw new Error(`caminho fora da raiz do repo: ${relativePath}`);
  }
  return abs;
}
