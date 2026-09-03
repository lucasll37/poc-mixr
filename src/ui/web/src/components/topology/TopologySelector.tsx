import { useScenarioStore, type Topology } from '../../store/scenarioStore';
import { findNodeById } from '../../model/treeOps';
import { findPlayerKeyForNode } from '../../model/selectors';
import { instantiateNode } from '../../model/instantiate';

const OPTIONS: Array<{ value: Topology; label: string; hint: string }> = [
  { value: 'single-thread', label: 'single-thread', hint: 'SimAgent no nivel da Station' },
  { value: 'multi-thread', label: 'multi-thread', hint: 'FlightAgentTC dentro do Player' },
  { value: 'none', label: 'sem decisao', hint: 'so Autopilot/joystick (ex: bandit-dis)' },
];

/** Monta ( FlightState )/( UbfArbiter behaviors: {} ) vazios -- o usuario preenche
 *  BtBehavior/AltitudeSafetyBehavior depois pela paleta (aba "Decisao"), com o
 *  UbfArbiter ja selecionado. */
function buildEmptyDecisionPair() {
  const state = instantiateNode('FlightState');
  const arbiter = instantiateNode('UbfArbiter');
  arbiter.slots.behaviors = { kind: 'positionalList', items: [] };
  return { state, arbiter };
}

export function TopologySelector() {
  const document = useScenarioStore((s) => s.document);
  const topology = useScenarioStore((s) => s.topology);
  const setTopology = useScenarioStore((s) => s.setTopology);
  const selectedNodeId = useScenarioStore((s) => s.selectedNodeId);
  const insertNamed = useScenarioStore((s) => s.insertNamed);

  const selected = document && selectedNodeId ? findNodeById(document, selectedNodeId) : undefined;
  const isAircraft = selected?.factoryName === 'Aircraft';
  const canAddAgent = isAircraft && topology !== 'none' && document;

  function handleAddAgent() {
    if (!document || !selected) return;
    const { state, arbiter } = buildEmptyDecisionPair();

    if (topology === 'multi-thread') {
      // 'components' do Aircraft e uma namedList (dynamicsModel, pilot, ...) --
      // usar setFormChild aqui SUBSTITUIRIA a lista inteira por um unico form,
      // destruindo os componentes ja existentes do aviao. Tem que ser
      // insertNamed, com a chave "agent" (convencao observada em todo cenario
      // multi-thread real -- sempre o ULTIMO componente).
      const existing = selected.slots.components;
      if (existing?.kind === 'namedList' && existing.entries.some((e) => e.key === 'agent')) {
        window.alert('este aviao ja tem um agente ("agent") em components -- remova-o primeiro pela arvore/inspector.');
        return;
      }
      const agent = instantiateNode('FlightAgentTC');
      agent.slots.state = { kind: 'form', node: state };
      agent.slots.behavior = { kind: 'form', node: arbiter };
      insertNamed(selected.id, 'components', 'agent', agent);
      return;
    }

    // single-thread: SimAgent vive em components: da propria Station (o
    // document raiz), amarrado ao player por NOME -- confirmado lendo
    // src/poc/single-thread/configs/scenario.epp.in:36-82 (ver plano aprovado).
    const playerName = findPlayerKeyForNode(document, selected.id);
    if (!playerName) {
      window.alert('nao foi possivel achar o nome deste player em simulation.players -- salve o cenario com um nome valido antes.');
      return;
    }
    const existing = document.slots.components;
    const count = existing?.kind === 'namedList' ? existing.entries.filter((e) => e.key.startsWith('agent')).length : 0;
    const agent = instantiateNode('SimAgent');
    agent.slots.actorPlayerName = { kind: 'playerRef', playerName };
    agent.slots.state = { kind: 'form', node: state };
    agent.slots.behavior = { kind: 'form', node: arbiter };
    insertNamed(document.id, 'components', `agent${count + 1}`, agent);
  }

  return (
    <div className="topology-selector">
      <div className="topology-selector__options">
        {OPTIONS.map((o) => (
          <label key={o.value} className="topology-selector__option" title={o.hint}>
            <input type="radio" name="topology" checked={topology === o.value} onChange={() => setTopology(o.value)} />
            {o.label}
          </label>
        ))}
      </div>
      <button type="button" disabled={!canAddAgent} onClick={handleAddAgent} title={isAircraft ? '' : 'selecione um Aircraft'}>
        + agente de decisao
      </button>
    </div>
  );
}
