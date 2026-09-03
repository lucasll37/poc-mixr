import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { parseScenario } from '../parser';
import { serializeScenario } from '../serializer';
import { preprocessIncludes } from '../preprocessor';
import type { ScenarioNode } from '../../model/ScenarioNode';

const HERE = path.dirname(fileURLToPath(import.meta.url));
// __tests__ -> edl -> src -> web -> ui -> src -> <raiz do repo>
const REPO_ROOT = path.resolve(HERE, '..', '..', '..', '..', '..', '..');

function read(relPath: string): string {
  return readFileSync(path.join(REPO_ROOT, relPath), 'utf8');
}

/** Compara duas ScenarioNode ignorando os "id" (uuid interno, nao semantico). */
function stripIds(node: ScenarioNode): unknown {
  return JSON.parse(JSON.stringify(node, (key, value) => (key === 'id' ? undefined : value)));
}

function expectRoundTrip(source: string) {
  const first = parseScenario(source);
  const text = serializeScenario(first);
  const second = parseScenario(text);
  expect(stripIds(second)).toEqual(stripIds(first));
}

describe('round-trip semantico contra os .epp/.epp.in REAIS do repo', () => {
  it('src/poc/single-thread/configs/scenario.epp.in', () => {
    expectRoundTrip(read('src/poc/single-thread/configs/scenario.epp.in'));
  });

  it('src/poc/single-thread/configs/scenario_missile_demo.epp.in', () => {
    expectRoundTrip(read('src/poc/single-thread/configs/scenario_missile_demo.epp.in'));
  });

  it('src/poc/multi-thread/configs/scenario.epp.in', () => {
    expectRoundTrip(read('src/poc/multi-thread/configs/scenario.epp.in'));
  });

  it('src/poc/bandit-dis/configs/scenario.epp', () => {
    expectRoundTrip(read('src/poc/bandit-dis/configs/scenario.epp'));
  });

  it('app/configs/scenario_intercept_missile.epp.in (com @include: resolvido)', () => {
    const raw = read('app/configs/scenario_intercept_missile.epp.in');
    const fragment = read('app/configs/fragments/tacview_recorder.epp.frag');
    const { text } = preprocessIncludes(raw, { 'tacview_recorder.epp.frag': fragment });
    expectRoundTrip(text);
  });

  it('preserva a unidade original de um slot ao reserializar sem edicao', () => {
    // Formatacao numerica exata (ex: "2.0" vs "2") NAO e uma garantia do
    // serializer -- so a UNIDADE escolhida no arquivo original e preservada
    // (ver plano, "Regras obrigatorias do serializer"). O valor numerico em
    // si e conferido pelo round-trip semantico acima, nao aqui.
    const source = read('src/poc/single-thread/configs/scenario.epp.in');
    const node = parseScenario(source);
    const text = serializeScenario(node);
    expect(text).toContain('( NauticalMiles 2 )');
    expect(text).toContain('( Meters 1200 )');
  });
});
