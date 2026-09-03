import { createNode } from './ScenarioNode';
import type { ScenarioNode } from './ScenarioNode';
import type { SlotValue } from './SlotValue';
import { getFactorySchema } from '../schema/catalog';

/** Cria um node novo, pre-preenchido com os defaults do catalogo (quando existem). */
export function instantiateNode(factoryName: string, formName?: string): ScenarioNode {
  const node = createNode(factoryName, formName);
  const schema = getFactorySchema(factoryName);
  if (!schema) return node;

  for (const slot of schema.slots) {
    if (slot.default === undefined) continue;
    const value = literalFromDefault(slot);
    if (value) node.slots[slot.name] = value;
  }
  return node;
}

function literalFromDefault(slot: { valueShape: string; default?: number | string | boolean; unitFamily?: string; allowedUnits?: string[] }): SlotValue | undefined {
  switch (slot.valueShape) {
    case 'number':
      return typeof slot.default === 'number' ? { kind: 'number', value: slot.default } : undefined;
    case 'unitNumber':
      return typeof slot.default === 'number'
        ? { kind: 'unitNumber', unit: slot.allowedUnits?.[0] ?? 'Meters', value: slot.default }
        : undefined;
    case 'string':
      return typeof slot.default === 'string' ? { kind: 'string', value: slot.default } : undefined;
    case 'identifier':
      return typeof slot.default === 'string' ? { kind: 'identifier', value: slot.default } : undefined;
    case 'boolean':
      return typeof slot.default === 'boolean' ? { kind: 'boolean', value: slot.default } : undefined;
    default:
      return undefined;
  }
}
