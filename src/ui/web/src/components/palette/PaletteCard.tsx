import type { FactorySchema, InsertionRule } from '../../schema/catalog';
import { useScenarioStore } from '../../store/scenarioStore';
import { findNodeById } from '../../model/treeOps';
import { instantiateNode } from '../../model/instantiate';

function suggestKey(parentFactory: string, slot: string, existingCount: number, childFactory: string): string {
  if (slot === 'players') return `player${existingCount + 1}`;
  if (slot === 'stores') return String(existingCount + 1);
  if (parentFactory === 'Aircraft' && slot === 'components') {
    const conventional: Record<string, string> = {
      JSBSimModel: 'dynamicsModel',
      Autopilot: 'pilot',
      AlertDatalink: 'datalink',
      Gimbal: 'antennas',
      SensorMgr: 'sensors',
      OnboardComputer: 'obc',
      StoresMgr: 'stores',
      FlightAgentTC: 'agent',
    };
    return conventional[childFactory] ?? childFactory.toLowerCase();
  }
  if (slot === 'components') return `${childFactory.toLowerCase()}${existingCount + 1}`;
  return `${childFactory.toLowerCase()}${existingCount + 1}`;
}

export function PaletteCard({ schema }: { schema: FactorySchema }) {
  const document = useScenarioStore((s) => s.document);
  const selectedNodeId = useScenarioStore((s) => s.selectedNodeId);
  const setFormChild = useScenarioStore((s) => s.setFormChild);
  const insertNamed = useScenarioStore((s) => s.insertNamed);
  const insertPositional = useScenarioStore((s) => s.insertPositional);

  const selected = document && selectedNodeId ? findNodeById(document, selectedNodeId) : undefined;

  const matchingRule: InsertionRule | undefined = selected
    ? schema.insertionPoints.find((r) => r.parentFactory === selected.factoryName)
    : undefined;

  const disabled = !selected || !matchingRule;

  function handleClick() {
    if (!selected || !matchingRule) return;
    const child = instantiateNode(schema.factoryName);
    if (matchingRule.shape === 'form') {
      setFormChild(selected.id, matchingRule.parentSlot, child);
    } else if (matchingRule.shape === 'namedList') {
      const currentSlot = selected.slots[matchingRule.parentSlot];
      const count = currentSlot?.kind === 'namedList' ? currentSlot.entries.length : 0;
      const suggested = suggestKey(selected.factoryName, matchingRule.parentSlot, count, schema.factoryName);
      const key = window.prompt(`Nome/chave para este ${schema.label}:`, suggested) ?? suggested;
      insertNamed(selected.id, matchingRule.parentSlot, key, child);
    } else {
      insertPositional(selected.id, matchingRule.parentSlot, child);
    }
  }

  return (
    <button
      type="button"
      className="palette-card"
      disabled={disabled}
      title={
        disabled
          ? `selecione um node do tipo ${schema.insertionPoints.map((r) => r.parentFactory).join(' ou ')} para habilitar`
          : `inserir em ${matchingRule!.parentSlot}`
      }
      onClick={handleClick}
    >
      <span className="palette-card__label">{schema.label}</span>
      <span className="palette-card__factory">{schema.factoryName}</span>
      {schema.requiresPluginProvides && <span className="palette-card__badge">plugin</span>}
      {schema.requiresNativeFactories && <span className="palette-card__badge">fabrica nativa</span>}
    </button>
  );
}
