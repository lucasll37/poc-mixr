// Operacoes de navegacao/mutacao sobre a arvore ScenarioNode. Mutacao e
// sempre "clona a raiz inteira, muta a copia" -- simplicidade sobre
// micro-otimizacao de structural sharing, aceitavel para o tamanho de
// arvore de um editor de cenario (dezenas/centenas de nodes, nao milhares).
import type { ScenarioNode } from './ScenarioNode';
import type { SlotValue } from './SlotValue';

export function cloneTree(root: ScenarioNode): ScenarioNode {
  return structuredClone(root);
}

export function findNodeById(root: ScenarioNode, id: string): ScenarioNode | undefined {
  if (root.id === id) return root;
  for (const value of Object.values(root.slots)) {
    const found = findInValue(value, id);
    if (found) return found;
  }
  return undefined;
}

function findInValue(value: SlotValue, id: string): ScenarioNode | undefined {
  switch (value.kind) {
    case 'form':
      return findNodeById(value.node, id);
    case 'namedList':
      for (const entry of value.entries) {
        const found = findInValue(entry.value, id);
        if (found) return found;
      }
      return undefined;
    case 'positionalList':
      for (const item of value.items) {
        if (item.kind === 'node') {
          const found = findNodeById(item.node, id);
          if (found) return found;
        }
      }
      return undefined;
    default:
      return undefined;
  }
}

export interface ChildRef {
  /** rotulo pra exibir na arvore, ex: "falcon1", "dynamicsModel", "[0]" */
  label: string;
  slotKey: string;
  node: ScenarioNode;
}

/** Lista os filhos diretos de um node, junto do rotulo/slot onde cada um mora -- usado pela ScenarioTree. */
export function listChildren(node: ScenarioNode): ChildRef[] {
  const out: ChildRef[] = [];
  for (const [slotKey, value] of Object.entries(node.slots)) {
    if (value.kind === 'form') {
      out.push({ label: slotKey, slotKey, node: value.node });
    } else if (value.kind === 'namedList') {
      for (const entry of value.entries) {
        if (entry.value.kind === 'form') {
          out.push({ label: entry.key, slotKey, node: entry.value.node });
        }
      }
    } else if (value.kind === 'positionalList') {
      value.items.forEach((item, idx) => {
        if (item.kind === 'node') {
          out.push({ label: `[${idx}] ${item.node.factoryName}`, slotKey, node: item.node });
        }
      });
    }
  }
  return out;
}

export function updateSlotValue(root: ScenarioNode, nodeId: string, slotKey: string, value: SlotValue): ScenarioNode {
  const clone = cloneTree(root);
  const target = findNodeById(clone, nodeId);
  if (!target) throw new Error(`node nao encontrado: ${nodeId}`);
  target.slots[slotKey] = value;
  return clone;
}

export function setFormSlot(root: ScenarioNode, parentId: string, slotKey: string, child: ScenarioNode): ScenarioNode {
  const clone = cloneTree(root);
  const parent = findNodeById(clone, parentId);
  if (!parent) throw new Error(`node pai nao encontrado: ${parentId}`);
  parent.slots[slotKey] = { kind: 'form', node: child };
  return clone;
}

export function insertIntoNamedList(
  root: ScenarioNode,
  parentId: string,
  slotKey: string,
  key: string,
  child: ScenarioNode,
): ScenarioNode {
  const clone = cloneTree(root);
  const parent = findNodeById(clone, parentId);
  if (!parent) throw new Error(`node pai nao encontrado: ${parentId}`);
  const existing = parent.slots[slotKey];
  const entries = existing?.kind === 'namedList' ? existing.entries : [];
  parent.slots[slotKey] = {
    kind: 'namedList',
    entries: [...entries, { key, value: { kind: 'form', node: child } }],
  };
  return clone;
}

export function insertIntoPositionalList(
  root: ScenarioNode,
  parentId: string,
  slotKey: string,
  child: ScenarioNode,
): ScenarioNode {
  const clone = cloneTree(root);
  const parent = findNodeById(clone, parentId);
  if (!parent) throw new Error(`node pai nao encontrado: ${parentId}`);
  const existing = parent.slots[slotKey];
  const items = existing?.kind === 'positionalList' ? existing.items : [];
  parent.slots[slotKey] = { kind: 'positionalList', items: [...items, { kind: 'node', node: child }] };
  return clone;
}

/** Remove um node (por id) de onde quer que ele esteja -- slot 'form', item de namedList ou de positionalList. */
export function removeNodeById(root: ScenarioNode, targetId: string): ScenarioNode {
  const clone = cloneTree(root);
  removeFromNode(clone, targetId);
  return clone;
}

function removeFromNode(node: ScenarioNode, targetId: string): void {
  for (const [slotKey, value] of Object.entries(node.slots)) {
    if (value.kind === 'form') {
      if (value.node.id === targetId) {
        delete node.slots[slotKey];
      } else {
        removeFromNode(value.node, targetId);
      }
    } else if (value.kind === 'namedList') {
      const before = value.entries.length;
      value.entries = value.entries.filter((e) => !(e.value.kind === 'form' && e.value.node.id === targetId));
      if (value.entries.length === before) {
        for (const entry of value.entries) {
          if (entry.value.kind === 'form') removeFromNode(entry.value.node, targetId);
        }
      }
    } else if (value.kind === 'positionalList') {
      const before = value.items.length;
      value.items = value.items.filter((i) => !(i.kind === 'node' && i.node.id === targetId));
      if (value.items.length === before) {
        for (const item of value.items) {
          if (item.kind === 'node') removeFromNode(item.node, targetId);
        }
      }
    }
  }
}

/** Percorre a arvore inteira aplicando `fn` a cada node (pre-ordem). */
export function walkTree(root: ScenarioNode, fn: (node: ScenarioNode) => void): void {
  fn(root);
  for (const child of listChildren(root)) walkTree(child.node, fn);
}
