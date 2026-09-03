import type { SlotValue } from './SlotValue';

/**
 * Um form EDL: ( FactoryName slot: valor ... ). Filhos NAO sao um array
 * solto -- eles so existem atraves de slots do tipo 'namedList'/
 * 'positionalList'/'form' (ver SlotValue.ts). Isso evita reintroduzir, na
 * estrutura de dados, a mesma ambiguidade nomeado/posicional que o catalogo
 * de schema (schema/catalog.ts) ja resolve por fora.
 */
export interface ScenarioNode {
  /** uuid interno, so para a UI -- nunca serializado */
  id: string;
  /** nome EXATO da fabrica EDL, ex: "Aircraft", "BtBehavior", "SrtmHgtFile" */
  factoryName: string;
  /** a chave textual do form quando ele e um item de uma namedList, ex: "falcon1", "agent1" */
  formName?: string;
  slots: Record<string, SlotValue>;
  /**
   * true quando factoryName nao esta no catalogo de schema -- anotado depois
   * do parse (ver edl/parser.ts::annotateUnknown). O parser em si NAO
   * depende do catalogo pra resolver a arvore (a forma nomeada/posicional de
   * um '{ }' e sempre derivavel da propria estrutura -- ver o comentario em
   * edl/parser.ts), entao um node "unknown" ja vem com os slots corretamente
   * tipados; so fica bloqueado para EDICAO ESTRUTURAL guiada por schema na UI
   * (ainda e visualizavel e reserializavel sem perda).
   */
  unknown?: boolean;
}

let nextId = 0;
export function makeNodeId(): string {
  nextId += 1;
  return `n${nextId}-${Date.now().toString(36)}`;
}

export function createNode(factoryName: string, formName?: string): ScenarioNode {
  return { id: makeNodeId(), factoryName, formName, slots: {} };
}
