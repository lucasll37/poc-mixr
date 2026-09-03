// ScenarioNode/SlotValue -> texto EDL. Nao tenta reproduzir a formatacao
// exata do arquivo original (espacamento manual, comentarios) -- so
// legibilidade e VALIDADE sintatica (guarda ASCII, indentacao consistente).
// Ver README.md, "Limitacoes conhecidas".
import type { ScenarioNode } from '../model/ScenarioNode';
import type { SlotValue, SlotItem, NamedEntry } from '../model/SlotValue';
import { assertAscii } from './ascii-guard';
import { reorderPluginLoaderFirst } from './ordering';

const INDENT_UNIT = '   '; // 3 espacos, como os .epp reais do repo

function indent(depth: number): string {
  return INDENT_UNIT.repeat(depth);
}

function escapeEdlString(s: string): string {
  return s.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
}

function formToLines(node: ScenarioNode, depth: number, leadingText?: string): string[] {
  assertAscii(node.factoryName, `factoryName do node ${node.id}`);
  const lead = leadingText ?? indent(depth);
  const lines: string[] = [`${lead}( ${node.factoryName}`];
  for (const [key, value] of Object.entries(node.slots)) {
    lines.push(...slotToLines(key, value, depth + 1));
  }
  lines.push(`${indent(depth)})`);
  return lines;
}

function positionalItemToLines(item: SlotItem, depth: number): string[] {
  switch (item.kind) {
    case 'node':
      return formToLines(item.node, depth);
    case 'identifier':
      assertAscii(item.value, 'item de lista posicional (identifier)');
      return [`${indent(depth)}${item.value}`];
    case 'string':
      assertAscii(item.value, 'item de lista posicional (string)');
      return [`${indent(depth)}"${escapeEdlString(item.value)}"`];
    case 'number':
      return [`${indent(depth)}${item.value}`];
    default: {
      const exhaustive: never = item;
      throw new Error(`SlotItem nao tratado: ${JSON.stringify(exhaustive)}`);
    }
  }
}

function slotToLines(key: string, value: SlotValue, depth: number): string[] {
  assertAscii(key, `nome de slot "${key}"`);
  const prefix = `${indent(depth)}${key}: `;
  switch (value.kind) {
    case 'number':
      return [`${prefix}${value.value}`];
    case 'unitNumber':
      assertAscii(value.unit, `unidade do slot "${key}"`);
      return [`${prefix}( ${value.unit} ${value.value} )`];
    case 'string':
      assertAscii(value.value, `slot "${key}"`);
      return [`${prefix}"${escapeEdlString(value.value)}"`];
    case 'identifier':
      assertAscii(value.value, `slot "${key}"`);
      return [`${prefix}${value.value}`];
    case 'boolean':
      return [`${prefix}${value.value}`];
    case 'playerRef':
      assertAscii(value.playerName, `slot "${key}"`);
      return [`${prefix}${value.playerName}`];
    case 'rawVector':
      return [`${prefix}[ ${value.values.join(' ')} ]`];
    case 'form':
      return formToLines(value.node, depth, prefix);
    case 'namedList':
      return namedListToLines(key, value.entries, depth);
    case 'positionalList':
      return positionalListToLines(key, value.items, depth);
    default: {
      const exhaustive: never = value;
      throw new Error(`SlotValue nao tratado: ${JSON.stringify(exhaustive)}`);
    }
  }
}

function namedListToLines(key: string, entries: NamedEntry[], depth: number): string[] {
  const lines = [`${indent(depth)}${key}: {`];
  for (const entry of reorderPluginLoaderFirst(entries)) {
    lines.push(...slotToLines(entry.key, entry.value, depth + 1));
  }
  lines.push(`${indent(depth)}}`);
  return lines;
}

function positionalListToLines(key: string, items: SlotItem[], depth: number): string[] {
  const lines = [`${indent(depth)}${key}: {`];
  for (const item of items) {
    lines.push(...positionalItemToLines(item, depth + 1));
  }
  lines.push(`${indent(depth)}}`);
  return lines;
}

/** Serializa a arvore inteira, a partir do node raiz (ex: o ClockStation). */
export function serializeScenario(root: ScenarioNode): string {
  return formToLines(root, 0).join('\n') + '\n';
}
