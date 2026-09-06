#!/usr/bin/env node
"use strict";
/*
 * Converte um .edl real (ja expandido -- sem @token@/@include:@ sobrando)
 * para o formato de PROJETO do editor grafico ({id,factory,slotValues,
 * children} -- ver src/ui/edl_builder_core.js). Nao e' um parser fiel a
 * gramatica bison/flex real (edl_parser.y) -- e' um parser recursivo-
 * descendente ESTRUTURAL, no mesmo espirito de tools/edl_lint.py, so que
 * em vez de so validar, CONSTROI a arvore.
 *
 * Reaproveita as MESMAS funcoes de src/ui/edl_builder_core.js
 * (makeNode/makeTextLeaf/resetIdCounter) -- a arvore produzida usa
 * exatamente o modelo de dados que a propria UI usaria arrastando as
 * classes uma a uma, sem reimplementar esse modelo aqui.
 *
 * Uso:
 *   node src/ui/edl_to_ui_project.js <arquivo.edl ou .edl.in> > saida.json
 *
 * Existe para gerar o cenario DEFAULT que a ferramenta carrega ao abrir
 * (ver src/ui/README.md e o Makefile, alvo `edl-default-scenario`) --
 * roda-se OFFLINE, uma vez por atualizacao do cenario de referencia
 * (src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in -- o
 * TEMPLATE, nao o '.generated.edl' -- ver a nota abaixo), nao a cada build.
 *
 * ATENCAO -- rode contra o '.edl.in', nao o '.generated.edl': confirmado
 * lendo os dois lado a lado que o 'scenario.generated.edl' committed desta
 * poc estava DESATUALIZADO em relacao ao template (provides: sem
 * 'ThreadTagProbe', type/model "A4" em vez de "C310", velocidades de
 * cruzeiro antigas) -- um artefato gerado, comitado uma vez, que nao
 * acompanhou edicoes seguintes do '.in'. Por isso este script aceita
 * '@TOKEN@'/'@include:frag@' (mesma mecanica de tools/edl_lint.py,
 * expandTemplates() abaixo) e roda direto contra o template, que e' a
 * fonte de verdade.
 *
 * Ambiguidade resolvida com o CATALOGO, nao so pela sintaxe: '( Nome ... )'
 * tanto serve pra um FILHO (classe MIXR) quanto pra um valor de UNIDADE
 * ('( Seconds 0.1 )') -- a diferenca so aparece checando se 'Nome' bate com
 * uma das unidades que o SLOT (no catalogo) aceita. Sem essa checagem, todo
 * '( Seconds 0.1 )' viraria um "filho" fantasma de classe 'Seconds'.
 */

const fs = require("fs");
const path = require("path");

const REPO_ROOT = path.resolve(__dirname, "..", "..");
const core = require(path.join(__dirname, "edl_builder_core.js"));
const CATALOG_PATH = path.join(REPO_ROOT, "src", "ui", "edl_catalog.generated.json");
const FRAGMENTS_DIR = path.join(REPO_ROOT, "app", "configs", "fragments");

// Mesma mecanica de tools/edl_lint.py (expand_templates()): '@include:...@'
// puxa o fragmento de app/configs/fragments/, e qualquer '@TOKEN@' restante
// (ex.: '@NUM_TC_THREADS@') vira um numero neutro -- aqui "2", o mesmo valor
// que as pocs de producao usam por convencao (bandit/single-thread/
// multi-thread), nao "1" (o de edl_lint.py, que so' precisa nao quebrar
// sintaxe): o cenario default desta ferramenta pode ser reexportado e
// inspecionado por um humano, entao vale a pena o numero parecer plausivel.
function expandTemplates(text) {
  text = text.replace(/@include:([^@]+)@/g, (_, name) => {
    const frag = path.join(FRAGMENTS_DIR, name);
    return fs.existsSync(frag) ? fs.readFileSync(frag, "utf8") : "";
  });
  text = text.replace(/@([A-Za-z_][A-Za-z0-9_]*)@/g, "2");
  return text;
}

/* ------------------------------- tokenizer -------------------------------- */
// Mesmo charset de identificador nu do scanner real (edl_scanner.l:48),
// repetido aqui igual a tools/edl_lint.py (SLOT_KEY_RE) -- numero tem
// regra PROPRIA (com ponto decimal, que NAO esta no charset de
// identificador), entao um valor como "0.1" so tokeniza inteiro se a regra
// de numero for tentada e vencer por ter o casamento mais LONGO (o mesmo
// criterio de "maior casamento vence" que flex usa).
const NUM_RE = /^[+-]?\d+(\.\d+)?([eE][+-]?\d+)?/;
const BARE_RE = /^[a-zA-Z0-9~!@#$%^&*\-_+=<>?/]+/;

function matchBareOrNumber(s, i) {
  const rest = s.slice(i);
  const numM = rest.match(NUM_RE);
  const bareM = rest.match(BARE_RE);
  const numLen = numM ? numM[0].length : 0;
  const bareLen = bareM ? bareM[0].length : 0;
  if (numLen === 0 && bareLen === 0) return null;
  return numLen >= bareLen ? { text: numM[0], isNumber: true } : { text: bareM[0], isNumber: false };
}

function tokenize(text) {
  const tokens = [];
  let i = 0;
  const n = text.length;
  while (i < n) {
    const c = text[i];
    if (c === " " || c === "\t" || c === "\r" || c === "\n") { i++; continue; }
    if (c === "/" && text[i + 1] === "/") { while (i < n && text[i] !== "\n") i++; continue; }
    if ("(){}[]:".includes(c)) { tokens.push({ t: c }); i++; continue; }
    if (c === '"') {
      let j = i + 1, buf = "";
      while (j < n && text[j] !== '"') {
        if (text[j] === "\\" && j + 1 < n) { buf += text[j + 1]; j += 2; continue; }
        buf += text[j]; j++;
      }
      tokens.push({ t: "STR", v: buf });
      i = j + 1;
      continue;
    }
    const m = matchBareOrNumber(text, i);
    if (!m) { i++; continue; } // caractere fora de qualquer charset conhecido -- pula, melhor esforco
    tokens.push({ t: m.isNumber ? "NUM" : "ID", v: m.text });
    i += m.text.length;
  }
  return tokens;
}

/* ------------------------- parser (AST estrutural) ------------------------- */

function parseValue(toks, pos) {
  const tok = toks[pos.i];
  if (!tok) throw new Error("fim inesperado do arquivo");
  if (tok.t === "(") return parseForm(toks, pos);
  if (tok.t === "{") return parseList(toks, pos);
  if (tok.t === "[") return parseBracket(toks, pos);
  if (tok.t === "STR") { pos.i++; return { kind: "scalar", text: tok.v, quoted: true }; }
  if (tok.t === "ID" || tok.t === "NUM") { pos.i++; return { kind: "scalar", text: tok.v, quoted: false, isNumber: tok.t === "NUM" }; }
  throw new Error("token inesperado: " + JSON.stringify(tok));
}

// Uma entrada dentro de um form ou de uma lista: 'chave: valor' OU so
// 'valor' (posicional/anonimo) -- o lookahead (ID OU NUM seguido de ':')
// decide. Chave numerica existe de verdade (ex.: steerpoints numerados
// '1: ( Steerpoint ... )'), entao NUM tambem conta, nao so ID.
function parseEntry(toks, pos) {
  const tok = toks[pos.i];
  if ((tok.t === "ID" || tok.t === "NUM") && toks[pos.i + 1] && toks[pos.i + 1].t === ":") {
    const key = tok.v;
    pos.i += 2;
    return { key, value: parseValue(toks, pos) };
  }
  return { key: null, value: parseValue(toks, pos) };
}

function parseForm(toks, pos) {
  pos.i++; // (
  const nameTok = toks[pos.i];
  if (!nameTok || nameTok.t !== "ID") throw new Error("esperava nome de classe logo apos '('");
  pos.i++;
  const entries = [];
  while (toks[pos.i] && toks[pos.i].t !== ")") entries.push(parseEntry(toks, pos));
  if (!toks[pos.i]) throw new Error("'(' sem fechamento ')' correspondente");
  pos.i++; // )
  return { kind: "form", name: nameTok.v, entries };
}

function parseList(toks, pos) {
  pos.i++; // {
  const items = [];
  while (toks[pos.i] && toks[pos.i].t !== "}") items.push(parseEntry(toks, pos));
  if (!toks[pos.i]) throw new Error("'{' sem fechamento '}' correspondente");
  pos.i++;
  return { kind: "list", items };
}

function parseBracket(toks, pos) {
  pos.i++; // [
  const values = [];
  while (toks[pos.i] && toks[pos.i].t !== "]") { values.push(toks[pos.i].v); pos.i++; }
  if (!toks[pos.i]) throw new Error("'[' sem fechamento ']' correspondente");
  pos.i++;
  return { kind: "bracket", values };
}

/* ------------------------ AST -> arvore de projeto -------------------------- */

function scalarToSlotValue(value, slotDef) {
  if (!value.quoted && (value.text === "true" || value.text === "false") && slotDef && slotDef.acceptsBoolean) {
    return { kind: "boolean", value: value.text === "true" };
  }
  if (value.isNumber && slotDef && (slotDef.acceptsNumber || slotDef.unitFamilies.length)) {
    return { kind: slotDef.unitFamilies.length ? "unit" : "number", value: value.text, unit: "" };
  }
  return { kind: "text", value: value.text };
}

function astValueToChildNode(value, byFactory) {
  if (value.kind === "form") return astFormToNode(value, byFactory);
  if (value.kind === "scalar") return core.makeTextLeaf(value.text);
  if (value.kind === "bracket") {
    // Um item de lista que e' ele mesmo um vetor numerico cru -- o caso de
    // Table2/Table3.data ('data: { [ 1 2 3 ] [ 4 5 6 ] }', uma sublista por
    // ponto de y). O texto vira literalmente '[ 1 2 3 ]': serializeTextLiteral()
    // (edl_builder_core.js) reconhece essa forma e deixa passar SEM aspas --
    // ver RAW_VECTOR_RE la.
    return core.makeTextLeaf(`[ ${value.values.join(" ")} ]`);
  }
  // lista dentro de lista -- nao esperado nos cenarios reais deste
  // repositorio, mas nao trava: melhor esforco, vira um texto opaco.
  return core.makeTextLeaf(JSON.stringify(value));
}

function assignSlot(node, slotName, value, slotDef, byFactory) {
  if (value.kind === "bracket") {
    node.slotValues[slotName] = { kind: "vector", value: value.values.join(" ") };
    return;
  }
  if (value.kind === "list") {
    node.children[slotName] = value.items.map((it, idx) => ({
      key: it.key != null ? it.key : String(idx + 1),
      node: astValueToChildNode(it.value, byFactory),
    }));
    return;
  }
  if (value.kind === "form") {
    const unitOptions = slotDef ? slotDef.unitFamilies.flatMap((f) => f.options) : [];
    const single = value.entries.length === 1 && !value.entries[0].key && value.entries[0].value.kind === "scalar";
    // O catalogo por vezes NAO detecta a uniao de tipo de um slot com valor
    // de unidade (ex.: RfSystem.bandwidth: comentario diz "base::Number or
    // base::Frequency", mas o ON_SLOT so' registra 'Number' -- a segunda
    // opcao se perde na extracao por regex). Fallback: qualquer classe cuja
    // CADEIA DE HERANCA passa por 'Number' (Seconds/Degrees/KiloHertz/dB/...
    // -- confirmado no catalogo real) e' , por construcao, um WRAPPER de
    // unidade quando usada como '( Classe numeroSo )' -- mesmo raciocinio
    // permissivo ja' documentado em isCompatible().
    const wrapperEntry = byFactory[value.name];
    const looksLikeNumberWrapper = wrapperEntry && wrapperEntry.chain.includes("Number");
    if ((unitOptions.includes(value.name) || looksLikeNumberWrapper) && single) {
      node.slotValues[slotName] = { kind: "unit", value: value.entries[0].value.text, unit: value.name };
      return;
    }
    node.children[slotName] = [{ key: "1", node: astFormToNode(value, byFactory) }];
    return;
  }
  node.slotValues[slotName] = scalarToSlotValue(value, slotDef);
}

function astFormToNode(ast, byFactory) {
  const node = core.makeNode(ast.name);
  const entry = byFactory[ast.name];
  const slotsByName = {};
  if (entry) entry.slots.forEach((s) => { slotsByName[s.name] = s; });
  ast.entries.forEach((e) => {
    if (!e.key) return; // um form so tem entradas 'slot: valor' na gramatica real -- posicional aqui e' defensivo, ignora
    assignSlot(node, e.key, e.value, slotsByName[e.key], byFactory);
  });
  return node;
}

/* ----------------------------------- main ----------------------------------- */

function convert(text, byFactory) {
  core.resetIdCounter(1);
  const toks = tokenize(text);
  const pos = { i: 0 };
  while (toks[pos.i] && toks[pos.i].t !== "(") pos.i++; // pula qualquer coisa antes do primeiro form
  if (!toks[pos.i]) throw new Error("nenhum '(' encontrado -- arquivo vazio ou so comentarios?");
  const ast = parseForm(toks, pos);
  return astFormToNode(ast, byFactory);
}

function main() {
  const file = process.argv[2];
  if (!file) {
    console.error("uso: node src/ui/edl_to_ui_project.js <arquivo.edl>");
    process.exit(1);
  }
  if (!fs.existsSync(CATALOG_PATH)) {
    console.error(`catalogo nao encontrado em ${CATALOG_PATH} -- rode 'make edl-catalog' primeiro.`);
    process.exit(1);
  }
  const catalog = JSON.parse(fs.readFileSync(CATALOG_PATH, "utf8"));
  const byFactory = core.buildCatalogIndex(catalog);
  let text = fs.readFileSync(file, "utf8");
  if (text.includes("@")) text = expandTemplates(text);
  const tree = convert(text, byFactory);
  process.stdout.write(JSON.stringify(tree, null, 2) + "\n");
}

if (require.main === module) main();

module.exports = { tokenize, parseForm, convert };
