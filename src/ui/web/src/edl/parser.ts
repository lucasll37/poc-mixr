// Parser recursivo-descendente EDL -> ScenarioNode/SlotValue.
//
// Achado de projeto (refina o plano original): resolver "{ } e lista ou
// mapa" NAO exige consultar o catalogo de schema em tempo de parse -- e
// sempre derivavel da propria sintaxe observada, item a item: cada item de
// um '{ }' ou tem um SLOT_ID na frente (chave) ou nao tem. "Todos com chave"
// vira namedList, "nenhum com chave" vira positionalList, "vazio" fica
// ambiguo so no vacuo (nao tem slot nenhum pra desambiguar, mas tambem nao
// tem informacao nenhuma pra perder -- os dois formatos serializam de volta
// como "{ }"). So "ALGUNS com chave, outros nao" no MESMO '{ }' e uma
// ambiguidade de verdade -- e nunca acontece nos .epp reais do repo, entao
// vira erro de parse (com localizacao) em vez de adivinhar.
//
// Isso significa que o parser funciona para QUALQUER factoryName, catalogado
// ou nao -- ele nao descarta nem "achata" nada. O catalogo de schema entra
// DEPOIS do parse, so pra anotar quais nodes sao "desconhecidos" (fora do
// catalogo -- ainda visualizaveis/reserializaveis, so sem edicao guiada por
// schema na UI) e pra dar significado semantico extra (unitFamily, playerRef,
// insertionPoints) usado pela paleta/inspector.
import { tokenize, type Token } from './lexer';
import type { ScenarioNode } from '../model/ScenarioNode';
import { createNode } from '../model/ScenarioNode';
import type { SlotValue, SlotItem } from '../model/SlotValue';
import { getFactorySchema } from '../schema/catalog';

export class EdlParseError extends Error {
  constructor(message: string, public readonly line: number, public readonly col: number) {
    super(`${message} (linha ${line}, coluna ${col})`);
    this.name = 'EdlParseError';
  }
}

interface RawArg {
  key?: string;
  value: RawValue;
}

type RawValue =
  | { kind: 'form'; factoryName: string; args: RawArg[]; line: number; col: number }
  | { kind: 'string'; value: string }
  | { kind: 'ident'; value: string }
  | { kind: 'number'; value: number }
  | { kind: 'bool'; value: boolean }
  | { kind: 'vector'; values: number[] }
  | { kind: 'pairstream'; items: RawArg[]; line: number; col: number };

class TokenCursor {
  private pos = 0;
  constructor(private readonly tokens: Token[]) {}

  peek(): Token {
    return this.tokens[this.pos]!;
  }

  next(): Token {
    const t = this.tokens[this.pos]!;
    if (t.kind !== 'eof') this.pos += 1;
    return t;
  }

  expect(kind: Token['kind']): Token {
    const t = this.peek();
    if (t.kind !== kind) {
      throw new EdlParseError(`esperado ${kind}, encontrado ${t.kind}${t.text ? ` ("${t.text}")` : ''}`, t.line, t.col);
    }
    return this.next();
  }
}

function parseForm(cur: TokenCursor): Extract<RawValue, { kind: 'form' }> {
  const open = cur.expect('lparen');
  const nameTok = cur.expect('ident');
  const args: RawArg[] = [];
  while (cur.peek().kind !== 'rparen') {
    if (cur.peek().kind === 'eof') {
      throw new EdlParseError('form nao fechado (esperava ")")', open.line, open.col);
    }
    args.push(parseArg(cur));
  }
  cur.expect('rparen');
  return { kind: 'form', factoryName: nameTok.text!, args, line: open.line, col: open.col };
}

function parseVector(cur: TokenCursor): Extract<RawValue, { kind: 'vector' }> {
  cur.expect('lbracket');
  const values: number[] = [];
  while (cur.peek().kind !== 'rbracket') {
    const t = cur.expect('number');
    values.push(t.num!);
  }
  cur.expect('rbracket');
  return { kind: 'vector', values };
}

function parsePairstream(cur: TokenCursor): Extract<RawValue, { kind: 'pairstream' }> {
  const open = cur.expect('lbrace');
  const items: RawArg[] = [];
  while (cur.peek().kind !== 'rbrace') {
    if (cur.peek().kind === 'eof') {
      throw new EdlParseError('"{" nao fechado (esperava "}")', open.line, open.col);
    }
    items.push(parseArg(cur));
  }
  cur.expect('rbrace');
  return { kind: 'pairstream', items, line: open.line, col: open.col };
}

function parseValue(cur: TokenCursor): RawValue {
  const t = cur.peek();
  switch (t.kind) {
    case 'lparen':
      return parseForm(cur);
    case 'lbrace':
      return parsePairstream(cur);
    case 'lbracket':
      return parseVector(cur);
    case 'string':
      cur.next();
      return { kind: 'string', value: t.text! };
    case 'ident':
      cur.next();
      return { kind: 'ident', value: t.text! };
    case 'number':
      cur.next();
      return { kind: 'number', value: t.num! };
    case 'bool':
      cur.next();
      return { kind: 'bool', value: t.bool! };
    default:
      throw new EdlParseError(`valor inesperado: ${t.kind}`, t.line, t.col);
  }
}

function parseArg(cur: TokenCursor): RawArg {
  const t = cur.peek();
  if (t.kind === 'slotId') {
    cur.next();
    return { key: t.text!, value: parseValue(cur) };
  }
  return { value: parseValue(cur) };
}

/** Forma de "unidade" do MIXR: ( Meters 1200 ), ( Degrees 90 ), ( dB 42 ) ...
 *  Reconhecida ESTRUTURALMENTE (1 arg unico, sem chave, numerico) -- nenhuma
 *  lista de nomes de unidade precisa ser mantida a parte. */
function isUnitWrapperForm(form: Extract<RawValue, { kind: 'form' }>): form is Extract<RawValue, { kind: 'form' }> {
  return form.args.length === 1 && form.args[0]!.key === undefined && form.args[0]!.value.kind === 'number';
}

function resolveNode(form: Extract<RawValue, { kind: 'form' }>, formName?: string): ScenarioNode {
  const node = createNode(form.factoryName, formName);
  let positionalCounter = 0;
  for (const arg of form.args) {
    if (arg.key !== undefined) {
      node.slots[arg.key] = resolveValue(arg.value);
    } else {
      // Nao deveria acontecer nos .epp reais (todo arg de um form top-level e
      // "chave: valor"), mas a gramatica permite -- preserva em vez de descartar.
      positionalCounter += 1;
      node.slots[`_positional_${positionalCounter}`] = resolveValue(arg.value);
    }
  }
  return node;
}

function resolveValue(raw: RawValue): SlotValue {
  switch (raw.kind) {
    case 'number':
      return { kind: 'number', value: raw.value };
    case 'string':
      return { kind: 'string', value: raw.value };
    case 'ident':
      return { kind: 'identifier', value: raw.value };
    case 'bool':
      return { kind: 'boolean', value: raw.value };
    case 'vector':
      return { kind: 'rawVector', values: raw.values };
    case 'form':
      if (isUnitWrapperForm(raw)) {
        const numArg = raw.args[0]!.value as Extract<RawValue, { kind: 'number' }>;
        return { kind: 'unitNumber', unit: raw.factoryName, value: numArg.value };
      }
      return { kind: 'form', node: resolveNode(raw) };
    case 'pairstream':
      return resolvePairstream(raw);
    default: {
      const exhaustive: never = raw;
      throw new Error(`valor RawValue nao tratado: ${JSON.stringify(exhaustive)}`);
    }
  }
}

/** Como resolveValue, mas repassa a chave do item de uma namedList como
 *  node.formName quando o valor e um form -- assim a UI mostra "falcon1
 *  (Aircraft)" sem precisar subir ate o pai pra achar o nome. */
function resolveNamedEntryValue(key: string, raw: RawValue): SlotValue {
  if (raw.kind === 'form' && !isUnitWrapperForm(raw)) {
    return { kind: 'form', node: resolveNode(raw, key) };
  }
  return resolveValue(raw);
}

function resolvePairstream(raw: Extract<RawValue, { kind: 'pairstream' }>): SlotValue {
  if (raw.items.length === 0) {
    return { kind: 'positionalList', items: [] };
  }
  const keyedCount = raw.items.filter((i) => i.key !== undefined).length;
  if (keyedCount === raw.items.length) {
    return {
      kind: 'namedList',
      entries: raw.items.map((i) => ({ key: i.key!, value: resolveNamedEntryValue(i.key!, i.value) })),
    };
  }
  if (keyedCount === 0) {
    return { kind: 'positionalList', items: raw.items.map((i) => resolveSlotItem(i.value)) };
  }
  throw new EdlParseError(
    '"{ }" mistura itens com e sem chave -- ambiguidade real, nao um caso visto nos .epp do repo',
    raw.line,
    raw.col,
  );
}

function resolveSlotItem(raw: RawValue): SlotItem {
  switch (raw.kind) {
    case 'form':
      return { kind: 'node', node: resolveNode(raw) };
    case 'ident':
      return { kind: 'identifier', value: raw.value };
    case 'string':
      return { kind: 'string', value: raw.value };
    case 'number':
      return { kind: 'number', value: raw.value };
    default:
      throw new Error(`item de lista posicional com forma inesperada: ${raw.kind}`);
  }
}

/** Marca node.unknown=true recursivamente para todo factoryName fora do catalogo. */
function annotateUnknown(node: ScenarioNode): void {
  node.unknown = getFactorySchema(node.factoryName) === undefined;
  for (const slot of Object.values(node.slots)) {
    annotateUnknownInValue(slot);
  }
}

function annotateUnknownInValue(value: SlotValue): void {
  switch (value.kind) {
    case 'form':
      annotateUnknown(value.node);
      break;
    case 'namedList':
      for (const entry of value.entries) annotateUnknownInValue(entry.value);
      break;
    case 'positionalList':
      for (const item of value.items) {
        if (item.kind === 'node') annotateUnknown(item.node);
      }
      break;
    default:
      break;
  }
}

/** Parseia um documento EDL completo (um unico form no topo). */
export function parseScenario(source: string): ScenarioNode {
  const tokens = tokenize(source);
  const cur = new TokenCursor(tokens);
  const root = parseForm(cur);
  const eof = cur.peek();
  if (eof.kind !== 'eof') {
    throw new EdlParseError(`conteudo inesperado apos o form principal: ${eof.kind}`, eof.line, eof.col);
  }
  const node = resolveNode(root);
  annotateUnknown(node);
  return node;
}
