import type { ScenarioNode } from '../../model/ScenarioNode';
import type { SlotValue } from '../../model/SlotValue';
import { useScenarioStore } from '../../store/scenarioStore';
import { getFactorySchema, UNIT_FAMILIES } from '../../schema/catalog';
import { listPlayers } from '../../model/selectors';
import { NumberField, TextField, BoolField, UnitNumberField, VectorField, PlayerRefField } from './fields';

function ChildSummary({ label, node }: { label: string; node: ScenarioNode }) {
  const select = useScenarioStore((s) => s.select);
  const removeNode = useScenarioStore((s) => s.removeNode);
  return (
    <div className="child-summary">
      <button type="button" className="child-summary__link" onClick={() => select(node.id)}>
        {label}: {node.factoryName}
        {node.unknown ? ' (fora do catalogo)' : ''}
      </button>
      <button type="button" className="child-summary__remove" title="remover" onClick={() => removeNode(node.id)}>
        x
      </button>
    </div>
  );
}

function SlotRow({ node, slotKey, value }: { node: ScenarioNode; slotKey: string; value: SlotValue }) {
  const updateSlot = useScenarioStore((s) => s.updateSlot);
  const rootDoc = useScenarioStore((s) => s.document);
  const schema = getFactorySchema(node.factoryName);
  const slotSchema = schema?.slots.find((s) => s.name === slotKey);
  const set = (v: SlotValue) => updateSlot(node.id, slotKey, v);

  switch (value.kind) {
    case 'number':
      return <NumberField value={value.value} onChange={(v) => set({ kind: 'number', value: v })} />;
    case 'unitNumber': {
      const allowed = slotSchema?.allowedUnits ?? (slotSchema?.unitFamily ? UNIT_FAMILIES[slotSchema.unitFamily] : undefined);
      return (
        <UnitNumberField
          value={value.value}
          unit={value.unit}
          allowedUnits={allowed}
          onChange={(v, unit) => set({ kind: 'unitNumber', unit, value: v })}
        />
      );
    }
    case 'string':
      return <TextField value={value.value} onChange={(v) => set({ kind: 'string', value: v })} />;
    case 'identifier':
      return <TextField value={value.value} onChange={(v) => set({ kind: 'identifier', value: v })} />;
    case 'boolean':
      return <BoolField value={value.value} onChange={(v) => set({ kind: 'boolean', value: v })} />;
    case 'playerRef': {
      const players = rootDoc ? listPlayers(rootDoc).map((p) => p.key) : [];
      return <PlayerRefField value={value.playerName} players={players} onChange={(v) => set({ kind: 'playerRef', playerName: v })} />;
    }
    case 'rawVector':
      return <VectorField values={value.values} onChange={(v) => set({ kind: 'rawVector', values: v })} />;
    case 'form':
      return <ChildSummary label={slotKey} node={value.node} />;
    case 'namedList':
      return (
        <div className="list-summary">
          {value.entries.length === 0 && <span className="list-summary__empty">(vazio)</span>}
          {value.entries.map((entry, i) =>
            entry.value.kind === 'form' ? (
              <ChildSummary key={`${entry.key}-${i}`} label={entry.key} node={entry.value.node} />
            ) : (
              <div key={`${entry.key}-${i}`} className="list-summary__literal">
                {entry.key}: {'value' in entry.value ? String((entry.value as { value: unknown }).value) : '...'}
              </div>
            ),
          )}
        </div>
      );
    case 'positionalList':
      return (
        <div className="list-summary">
          {value.items.length === 0 && <span className="list-summary__empty">(vazio)</span>}
          {value.items.map((item, i) =>
            item.kind === 'node' ? (
              <ChildSummary key={i} label={`[${i}]`} node={item.node} />
            ) : (
              <div key={i} className="list-summary__literal">
                {item.kind === 'identifier' || item.kind === 'string' ? item.value : String(item.value)}
              </div>
            ),
          )}
        </div>
      );
    default:
      return null;
  }
}

export function SlotForm({ node }: { node: ScenarioNode }) {
  const schema = getFactorySchema(node.factoryName);
  const entries = Object.entries(node.slots);

  return (
    <div className="slot-form">
      <div className="slot-form__header">
        <h3>{node.formName ? `${node.formName} ` : ''}({node.factoryName})</h3>
        {schema ? <span className="slot-form__label">{schema.label}</span> : (
          <span className="slot-form__unknown">fora do catalogo -- visualizacao/reexportacao ok, edicao guiada limitada</span>
        )}
      </div>
      {schema?.requiresPluginProvides && (
        <p className="slot-form__note">
          requer PluginModule com <code>provides</code> incluindo <code>{node.factoryName}</code>
        </p>
      )}
      {schema?.requiresNativeFactories && (
        <p className="slot-form__note">
          requer a fabrica nativa encadeada no binario-alvo: <code>{schema.requiresNativeFactories.join(', ')}</code>
        </p>
      )}
      <table className="slot-form__table">
        <tbody>
          {entries.length === 0 && (
            <tr>
              <td colSpan={2} className="slot-form__empty">
                sem slots preenchidos
              </td>
            </tr>
          )}
          {entries.map(([key, value]) => {
            const slotSchema = schema?.slots.find((s) => s.name === key);
            return (
              <tr key={key}>
                <td className="slot-form__key">
                  {key}
                  {slotSchema?.required && <span className="slot-form__required">*</span>}
                  {slotSchema?.note && <div className="slot-form__hint">{slotSchema.note}</div>}
                </td>
                <td className="slot-form__value">
                  <SlotRow node={node} slotKey={key} value={value} />
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
