import { describe, expect, it } from 'vitest';
import { createNode } from '../../model/ScenarioNode';
import { serializeScenario } from '../serializer';

describe('ordenacao: PluginLoader sempre primeiro entre os irmaos de uma namedList', () => {
  it('reordena mesmo quando o usuario editou o PluginLoader por ultimo', () => {
    const station = createNode('ClockStation');

    const btBehaviorUser = createNode('SimAgent', 'agent1');
    // node fake so pra ocupar o slot -- nao precisa ser semanticamente completo pro teste de ordem
    const pluginLoader = createNode('PluginLoader', 'plugins');
    pluginLoader.slots.modules = { kind: 'positionalList', items: [] };

    station.slots.components = {
      kind: 'namedList',
      entries: [
        { key: 'agent1', value: { kind: 'form', node: btBehaviorUser } },
        { key: 'plugins', value: { kind: 'form', node: pluginLoader } },
      ],
    };

    const text = serializeScenario(station);
    const pluginsIdx = text.indexOf('plugins:');
    const agentIdx = text.indexOf('agent1:');
    expect(pluginsIdx).toBeGreaterThan(-1);
    expect(agentIdx).toBeGreaterThan(-1);
    expect(pluginsIdx).toBeLessThan(agentIdx);
  });
});
