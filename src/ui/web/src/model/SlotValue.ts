// Uniao discriminada dos valores que um slot EDL pode assumir. Ver
// src/ui/README.md ("Modelo de dados") para o raciocinio completo: a forma
// sintatica real de um '{ }' (lista vs mapa) e ambigua no texto -- quem
// desambigua e sempre o SlotSchema do slot que esta sendo preenchido, nunca
// o parser sozinho.
import type { ScenarioNode } from './ScenarioNode';

export type SlotValue =
  | { kind: 'number'; value: number }
  | { kind: 'unitNumber'; unit: string; value: number } // ( Meters 1200 )
  | { kind: 'string'; value: string } // "C310"
  | { kind: 'identifier'; value: string } // blue, horizontal, air
  | { kind: 'boolean'; value: boolean }
  | { kind: 'rawVector'; values: number[] } // [ 1 2 225 1 99 0 0 ]
  | { kind: 'playerRef'; playerName: string } // ownship: falcon1 / actorPlayerName: falcon1
  | { kind: 'positionalList'; items: SlotItem[] } // { (A...) (B...) } ou { a b c }
  | { kind: 'namedList'; entries: NamedEntry[] } // { plugins: (...) dynamicsModel: (...) }
  | { kind: 'form'; node: ScenarioNode }; // terrain: ( SrtmHgtFile ... )

/** Item de uma positionalList: pode ser um sub-form ou um literal solto (identifier/string/number). */
export type SlotItem =
  | { kind: 'node'; node: ScenarioNode }
  | { kind: 'identifier'; value: string }
  | { kind: 'string'; value: string }
  | { kind: 'number'; value: number };

export interface NamedEntry {
  key: string;
  value: SlotValue;
}

export function isFormValue(v: SlotValue): v is Extract<SlotValue, { kind: 'form' }> {
  return v.kind === 'form';
}
