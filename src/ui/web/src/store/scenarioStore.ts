import { create } from 'zustand';
import type { ScenarioNode } from '../model/ScenarioNode';
import {
  insertIntoNamedList,
  insertIntoPositionalList,
  removeNodeById,
  setFormSlot,
  updateSlotValue,
} from '../model/treeOps';
import type { SlotValue } from '../model/SlotValue';

export type Topology = 'single-thread' | 'multi-thread' | 'none';

interface ScenarioStoreState {
  document: ScenarioNode | null;
  sourcePath: string | null;
  topology: Topology;
  selectedNodeId: string | null;
  dirty: boolean;
  history: ScenarioNode[];

  loadDocument: (doc: ScenarioNode, sourcePath: string | null, topology: Topology) => void;
  select: (nodeId: string | null) => void;
  setTopology: (t: Topology) => void;
  updateSlot: (nodeId: string, slotKey: string, value: SlotValue) => void;
  setFormChild: (parentId: string, slotKey: string, child: ScenarioNode) => void;
  insertNamed: (parentId: string, slotKey: string, key: string, child: ScenarioNode) => void;
  insertPositional: (parentId: string, slotKey: string, child: ScenarioNode) => void;
  removeNode: (nodeId: string) => void;
  undo: () => void;
  reset: () => void;
}

const MAX_HISTORY = 50;

export const useScenarioStore = create<ScenarioStoreState>((set, get) => {
  function applyMutation(mutate: (doc: ScenarioNode) => ScenarioNode) {
    const { document, history } = get();
    if (!document) return;
    const next = mutate(document);
    set({
      document: next,
      dirty: true,
      history: [...history, document].slice(-MAX_HISTORY),
    });
  }

  return {
    document: null,
    sourcePath: null,
    topology: 'multi-thread',
    selectedNodeId: null,
    dirty: false,
    history: [],

    loadDocument: (doc, sourcePath, topology) =>
      set({ document: doc, sourcePath, topology, selectedNodeId: doc.id, dirty: false, history: [] }),

    select: (nodeId) => set({ selectedNodeId: nodeId }),

    setTopology: (t) => set({ topology: t }),

    updateSlot: (nodeId, slotKey, value) => applyMutation((doc) => updateSlotValue(doc, nodeId, slotKey, value)),

    setFormChild: (parentId, slotKey, child) => applyMutation((doc) => setFormSlot(doc, parentId, slotKey, child)),

    insertNamed: (parentId, slotKey, key, child) =>
      applyMutation((doc) => insertIntoNamedList(doc, parentId, slotKey, key, child)),

    insertPositional: (parentId, slotKey, child) =>
      applyMutation((doc) => insertIntoPositionalList(doc, parentId, slotKey, child)),

    removeNode: (nodeId) =>
      applyMutation((doc) => {
        const next = removeNodeById(doc, nodeId);
        return next;
      }),

    undo: () => {
      const { history } = get();
      if (history.length === 0) return;
      const previous = history[history.length - 1]!;
      set({ document: previous, history: history.slice(0, -1), dirty: true });
    },

    reset: () => set({ document: null, sourcePath: null, selectedNodeId: null, dirty: false, history: [] }),
  };
});
