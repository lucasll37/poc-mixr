import { describe, expect, it } from 'vitest';
import { createNode } from '../../model/ScenarioNode';
import { serializeScenario } from '../serializer';
import { NonAsciiError } from '../ascii-guard';

describe('guarda ASCII do serializer', () => {
  it('recusa gerar EDL com caractere nao-ASCII num slot de string', () => {
    const node = createNode('Aircraft');
    node.slots.type = { kind: 'string', value: 'nao-e-so' };
    expect(() => serializeScenario(node)).not.toThrow(); // ainda ASCII (acentos removidos)

    const bad = createNode('Aircraft');
    bad.slots.type = { kind: 'string', value: 'não-é-só' };
    expect(() => serializeScenario(bad)).toThrow(NonAsciiError);
  });

  it('recusa nome de fabrica nao-ASCII', () => {
    const node = createNode('Aviãozinho');
    expect(() => serializeScenario(node)).toThrow(NonAsciiError);
  });
});
