// Atalhos de leitura sobre a arvore, usados pelo mapa/topbar -- nao mutam nada.
import type { ScenarioNode } from './ScenarioNode';
import type { SlotValue } from './SlotValue';

export function getSlotForm(node: ScenarioNode, slotKey: string): ScenarioNode | undefined {
  const v = node.slots[slotKey];
  return v?.kind === 'form' ? v.node : undefined;
}

export function getSlotNumber(node: ScenarioNode, slotKey: string): number | undefined {
  const v = node.slots[slotKey];
  if (!v) return undefined;
  if (v.kind === 'number') return v.value;
  if (v.kind === 'unitNumber') return v.value;
  return undefined;
}

export function getWorldModel(root: ScenarioNode): ScenarioNode | undefined {
  return getSlotForm(root, 'simulation');
}

export function getReferenceLatLon(root: ScenarioNode): { lat: number; lon: number } | undefined {
  const world = getWorldModel(root);
  if (!world) return undefined;
  const lat = getSlotNumber(world, 'latitude');
  const lon = getSlotNumber(world, 'longitude');
  if (lat === undefined || lon === undefined) return undefined;
  return { lat, lon };
}

export interface PlayerEntry {
  key: string; // "falcon1"
  node: ScenarioNode;
}

export function listPlayers(root: ScenarioNode): PlayerEntry[] {
  const world = getWorldModel(root);
  if (!world) return [];
  const playersSlot = world.slots.players;
  if (playersSlot?.kind !== 'namedList') return [];
  const out: PlayerEntry[] = [];
  for (const entry of playersSlot.entries) {
    if (entry.value.kind === 'form') out.push({ key: entry.key, node: entry.value.node });
  }
  return out;
}

/** Inferido a partir da estrutura -- usado ao IMPORTAR um cenario existente,
 *  pra pre-selecionar o seletor de topologia da UI de acordo com o que o
 *  arquivo ja usa (nao pergunta ao usuario algo que ja da pra ver no arquivo). */
export function inferTopology(root: ScenarioNode): 'single-thread' | 'multi-thread' | 'none' {
  let hasFlightAgentTC = false;
  let hasSimAgent = false;
  const walk = (node: ScenarioNode) => {
    if (node.factoryName === 'FlightAgentTC') hasFlightAgentTC = true;
    if (node.factoryName === 'SimAgent') hasSimAgent = true;
    for (const value of Object.values(node.slots)) {
      if (value.kind === 'form') {
        walk(value.node);
      } else if (value.kind === 'namedList') {
        for (const e of value.entries) {
          if (e.value.kind === 'form') walk(e.value.node);
        }
      } else if (value.kind === 'positionalList') {
        for (const i of value.items) {
          if (i.kind === 'node') walk(i.node);
        }
      }
    }
  };
  walk(root);
  if (hasFlightAgentTC) return 'multi-thread';
  if (hasSimAgent) return 'single-thread';
  return 'none';
}

/** Chave (nome) do player na lista players: da WorldModel, dado o id do node -- undefined se nao for um player. */
export function findPlayerKeyForNode(root: ScenarioNode, nodeId: string): string | undefined {
  return listPlayers(root).find((p) => p.node.id === nodeId)?.key;
}

/** Unidade original ('NauticalMiles'/'Meters'/'Feet') de um slot unitNumber, ou undefined se o slot nao existe/nao e unitNumber. */
export function getUnitOf(value: SlotValue | undefined): string | undefined {
  return value?.kind === 'unitNumber' ? value.unit : undefined;
}
