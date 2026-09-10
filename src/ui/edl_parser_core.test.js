#!/usr/bin/env node
// Testes de unidade PUROS de src/ui/edl_parser_core.js -- sem Babel, sem
// React, sem DOM. Mesmo estilo (e mesmo executor manual, sem framework) de
// src/ui/edl_builder.test.js -- roda como um segundo passo do "3/5" de
// src/ui/scripts/build.js.
//
// Uso: node src/ui/edl_parser_core.test.js
"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const core = require("./edl_builder_core.js");
const parser = require("./edl_parser_core.js");

let failures = 0;
function test(name, fn) {
  try {
    fn();
    console.log(`ok - ${name}`);
  } catch (err) {
    failures++;
    console.log(`FALHOU - ${name}`);
    console.log(`  ${err.message}`);
  }
}

/* ------------------------- catálogo sintético p/ testes ------------------ */
// Pequeno de proposito, so o suficiente pra exercitar cada regra de
// classificacao/serializacao sem depender do catalogo real (esse ganha a
// varredura de regressao contra arquivos REAIS, mais abaixo).

const CATALOG = [
  { class: "Aircraft", factory: "Aircraft", baseClass: "Player", chain: ["Aircraft", "Player", "Object"], origin: "models",
    slots: [
      { name: "type", declaredIn: "Aircraft", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "debugLevel", declaredIn: "Aircraft", comment: "", acceptsNumber: true, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "navMode", declaredIn: "Aircraft", comment: "", acceptsNumber: false, acceptsBoolean: true, acceptsText: false, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "initAlt", declaredIn: "Aircraft", comment: "", acceptsNumber: true, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: false, unitFamilies: [{ family: "Distance", options: ["Meters", "Feet"] }], objectTypes: [], isReference: false },
      { name: "components", declaredIn: "Component", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: true, unitFamilies: [], objectTypes: ["Component"], isReference: false },
    ] },
  { class: "JSBSimModel", factory: "JSBSimModel", baseClass: "DynamicsModel", chain: ["JSBSimModel", "DynamicsModel", "Component", "Object"], origin: "models",
    slots: [
      { name: "model", declaredIn: "JSBSimModel", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
    ] },
  // 'Seconds' nao declara slot nenhum -- so existe aqui pra exercitar o
  // fallback "classe cuja cadeia passa por Number e' wrapper de unidade"
  // (assignSlot(), edl_parser_core.js), que funciona mesmo com o SLOT
  // desconhecido (unitOptions vazio), so' precisa da classe WRAPPER estar
  // catalogada.
  { class: "Seconds", factory: "Seconds", baseClass: "Time", chain: ["Seconds", "Time", "Number", "Object"], origin: "base", slots: [] },
];
const BY_FACTORY = core.buildCatalogIndex(CATALOG);

/* ------------------------------- tokenizer -------------------------------- */

test("tokenize: numero HEX ('0x1F') vira NUM -- faltava no conversor antigo", () => {
  const toks = parser.tokenize("debugLevel: 0x1F");
  const numTok = toks.find((t) => t.t === "NUM");
  assert.ok(numTok, "deveria ter reconhecido um token NUM");
  assert.strictEqual(numTok.v, "0x1F");
});

test("tokenize: float com ponto NA FRENTE ('.5') e ponto ATRAS ('5.') -- as tres formas da gramatica real", () => {
  let toks = parser.tokenize("gain: .5");
  assert.strictEqual(toks[toks.length - 1].t, "NUM");
  assert.strictEqual(toks[toks.length - 1].v, ".5");
  toks = parser.tokenize("gain: 5.");
  assert.strictEqual(toks[toks.length - 1].t, "NUM");
  assert.strictEqual(toks[toks.length - 1].v, "5.");
  toks = parser.tokenize("gain: 5e-3");
  assert.strictEqual(toks[toks.length - 1].t, "NUM");
  assert.strictEqual(toks[toks.length - 1].v, "5e-3");
});

test("tokenize: string alternativa '<...>' vira STR -- faltava inteiramente no conversor antigo", () => {
  const toks = parser.tokenize("cor: <ff00ff>");
  const strTok = toks.find((t) => t.t === "STR");
  assert.ok(strTok, "deveria ter reconhecido um token STR");
  assert.strictEqual(strTok.v, "ff00ff");
  assert.strictEqual(strTok.raw, "<ff00ff>", "raw deveria preservar os delimitadores originais");
});

test("tokenize: string entre aspas NAO interpreta escape -- '\\X' vira DOIS caracteres, gramatica real nao processa nenhum", () => {
  // ACHADO POR AUDITORIA (nao redescobrir): edl_scanner.l:223-232 so' copia
  // o conteudo entre aspas byte a byte (utStrcpy sobre yytext+1); o '\\.' no
  // PADRAO existe so' pra o lexer nao terminar a string cedo demais num '\"'
  // embutido -- a ACAO nao descarta o backslash. Um valor como '"a\\"b"' na
  // fonte tem de carregar como os 4 caracteres 'a', '\\', '"', 'b', nao 'a"b'
  // (3 caracteres, escape estilo C que este scanner nao tem).
  const toks = parser.tokenize('caminho: "C:\\\\data"');
  const strTok = toks.find((t) => t.t === "STR");
  assert.strictEqual(strTok.v, "C:\\\\data", "os DOIS backslashes da fonte tem que sobreviver, nao virar um so'");

  const toks2 = parser.tokenize('nome: "a\\"b"');
  const strTok2 = toks2.find((t) => t.t === "STR");
  assert.strictEqual(strTok2.v, 'a\\"b', "backslash + aspas embutida sobrevivem os DOIS, o valor real tem 4 caracteres");
});

test("scalarToSlotValue (via parseEdlDocument): 'TRUE'/'FALSE' maiusculo tambem vira booleano -- gramatica real tem as duas regras", () => {
  // ACHADO POR AUDITORIA (nao redescobrir): edl_scanner.l:119-140 declara
  // 'true'/'TRUE'/'false'/'FALSE' como quatro regras distintas -- so'
  // 'true'/'false' eram reconhecidas aqui. Sem a forma maiuscula, o valor
  // caia no ramo "text" (campo de texto na UI, nao o toggle) e reexportava
  // sempre em minusculo, divergindo do '.edl' carregado.
  const { tree: t1 } = parser.parseEdlDocument("( Aircraft navMode: TRUE )", BY_FACTORY);
  assert.deepStrictEqual(t1.slotValues.navMode, { kind: "boolean", value: true });
  const { tree: t2 } = parser.parseEdlDocument("( Aircraft navMode: FALSE )", BY_FACTORY);
  assert.deepStrictEqual(t2.slotValues.navMode, { kind: "boolean", value: false });
  // 'True'/'False' (caixa mista) NAO existe na gramatica real -- continua
  // caindo em "text", como ja documentado para o destaque de sintaxe.
  const { tree: t3 } = parser.parseEdlDocument("( Aircraft navMode: True )", BY_FACTORY);
  assert.strictEqual(t3.slotValues.navMode.kind, "text");
});

test("tokenize: virgula e' espaco em branco puro (gramatica real: '[ ,\\t\\v\\f]'), nao separador", () => {
  const shape = (toks) => toks.map((t) => t.t + (t.v !== undefined ? ":" + t.v : ""));
  const a = parser.tokenize("( Aircraft type: C310 debugLevel: 1 )");
  const b = parser.tokenize("( Aircraft, type: C310, debugLevel: 1, )");
  assert.deepStrictEqual(shape(b), shape(a));
});

test("tokenize: cada token carrega o NUMERO DE LINHA em que aparece", () => {
  const toks = parser.tokenize("( Aircraft\n   type: C310\n)");
  const typeTok = toks.find((t) => t.v === "type");
  assert.strictEqual(typeTok.line, 2);
});

test("parseForm: '(' sem fechamento aponta a linha de ABERTURA no erro (nao so' uma Error crua)", () => {
  assert.throws(() => {
    const toks = parser.tokenize("( Aircraft\n   type: C310\n");
    parser.parseForm(toks, { i: 0 });
  }, /linha 1/);
});

/* ------------------- fidelidade sem perda (fabrica/slot desconhecido) ----- */

test("classe desconhecida: valores de slot viram 'raw' e reexportam EXATOS, sem reinterpretar aspas/tipo", () => {
  const text = '( SomeWipClass numero: 42 nome: "com espaco" ident: falcon1 )';
  const { tree, warnings, errors } = parser.parseEdlDocument(text, BY_FACTORY);
  assert.strictEqual(errors.length, 0);
  assert.ok(tree);
  assert.strictEqual(tree.slotValues.numero.kind, "raw");
  assert.strictEqual(tree.slotValues.numero.raw, "42");
  assert.strictEqual(tree.slotValues.ident.raw, "falcon1");
  assert.ok(warnings.some((w) => w.code === "unknown-factory"), "deveria avisar sobre a fabrica desconhecida");

  const out = core.projectToEdl(tree, BY_FACTORY);
  // "42" tem que sair NU (nao '"42"') -- e' a corrupcao que 'raw' existe
  // pra evitar: kind:"text" forcaria aspas de volta via LOOKS_LIKE_NUMBER_RE/
  // serializeTextLiteral, trocando Identifier/Number por String no lado MIXR.
  assert.ok(out.includes("numero: 42"), out);
  assert.ok(!out.includes('numero: "42"'), out);
  assert.ok(out.includes('nome: "com espaco"'), out);
  assert.ok(out.includes("ident: falcon1"), out);
});

test("classe CONHECIDA com um slot a MAIS que o catalogo nao lista: preservado, nao descartado na reexportacao", () => {
  const text = "( Aircraft type: C310 futureSlot: 7 )";
  const { tree, warnings, errors } = parser.parseEdlDocument(text, BY_FACTORY);
  assert.strictEqual(errors.length, 0);
  assert.ok(warnings.some((w) => w.code === "unknown-slot" && w.message.includes("futureSlot")));
  const out = core.projectToEdl(tree, BY_FACTORY);
  assert.ok(out.includes("type: C310"), out);
  assert.ok(out.includes("futureSlot: 7"), out);
});

test("slot desconhecido: forma NUA e forma de LISTA para o MESMO nome de slot reexportam CADA UMA na sua propria forma (nunca adivinhado pela contagem de itens)", () => {
  // As duas formas NAO sao equivalentes no lado C++ (PairStream vs. objeto
  // direto) -- e' exatamente por isso que node.unknownSlotForms existe: sem
  // ele, as duas fixtures abaixo reexportariam IDENTICAS (a mesma
  // ambiguidade que motivou o design), corrompendo uma das duas formas.
  const single = "( SomeWipClass extra: ( JSBSimModel model: A4 ) )";
  const asList = "( SomeWipClass extra: { um: ( JSBSimModel model: A4 ) } )";

  const rSingle = parser.parseEdlDocument(single, BY_FACTORY);
  const rList = parser.parseEdlDocument(asList, BY_FACTORY);
  assert.strictEqual(rSingle.errors.length, 0);
  assert.strictEqual(rList.errors.length, 0);
  assert.strictEqual(rSingle.tree.unknownSlotForms.extra, "single");
  assert.strictEqual(rList.tree.unknownSlotForms.extra, "list");

  const outSingle = core.projectToEdl(rSingle.tree, BY_FACTORY);
  const outList = core.projectToEdl(rList.tree, BY_FACTORY);
  assert.ok(outSingle.includes("extra: ( JSBSimModel"), outSingle);
  assert.ok(!outSingle.includes("extra: {"), outSingle);
  assert.ok(outList.includes("extra: {"), outList);
  assert.ok(outList.includes("um: ( JSBSimModel"), outList);
});

test("unidade '( Seconds 0.1 )' num slot DESCONHECIDO ainda e' reconhecida via cadeia-de-heranca-passa-por-Number (nao vira filho fantasma)", () => {
  const { tree, errors } = parser.parseEdlDocument("( SomeWipClass hold: ( Seconds 0.1 ) )", BY_FACTORY);
  assert.strictEqual(errors.length, 0);
  assert.strictEqual(tree.slotValues.hold.kind, "unit");
  assert.strictEqual(tree.slotValues.hold.unit, "Seconds");
  assert.strictEqual(tree.slotValues.hold.value, "0.1");
  assert.strictEqual(tree.children.hold, undefined, "nao deveria ter virado um FILHO de classe 'Seconds'");
});

/* --------------------------------- @include: ------------------------------- */

test("'@include:frag@' vira erro DURO nomeando o fragmento -- nunca substituido por vazio em silencio", () => {
  const { tree, errors } = parser.parseEdlDocument("( Station\n   x: @include:frag@\n)", BY_FACTORY);
  assert.strictEqual(tree, null);
  assert.strictEqual(errors.length, 1);
  assert.strictEqual(errors[0].code, "include-unresolved");
  assert.ok(errors[0].message.includes("frag"), errors[0].message);
});

test("'@include:' malformado (sem '@' de fechamento na MESMA linha) tambem vira erro duro, nao um parse silenciosamente corrompido", () => {
  // Sem esta deteccao, '@include:frag' tokenizaria como SLOT_ID '@include:'
  // seguido do valor 'frag' -- um slot fantasma, nao uma falha visivel.
  const { tree, errors } = parser.parseEdlDocument("( Station\n   x: @include:frag\n)", BY_FACTORY);
  assert.strictEqual(tree, null);
  assert.strictEqual(errors.length, 1);
  assert.strictEqual(errors[0].code, "include-unresolved");
});

/* ---------------------------------- @TOKEN@ -------------------------------- */

// A CARGA nao avisa mais nada sobre '@TOKEN@' -- quem aponta cada
// ocorrencia e' core.collectOpenIssues() (aba "Abertos"), sobre a arvore
// VIVA, entao corrigir o campo derruba a contagem sem recarregar. Aqui se
// afirma so' o que e' responsabilidade DESTE modulo: o token atravessa a
// carga e a reexportacao sem ser adivinhado nem corrompido.
test("'@TOKEN@' (fora de '@include:') carrega LITERAL e sobrevive ao round-trip -- nunca adivinha um valor", () => {
  const text = "( Aircraft type: C310 debugLevel: @NUM_TC_THREADS@ )";
  const { tree, warnings, errors } = parser.parseEdlDocument(text, BY_FACTORY);
  assert.strictEqual(errors.length, 0);
  assert.ok(tree);
  assert.ok(!warnings.some((w) => w.code === "token-placeholder"),
    "aviso de carga saiu de proposito -- a aba Abertos cobre isso ao vivo");
  const out = core.projectToEdl(tree, BY_FACTORY);
  assert.ok(out.includes("debugLevel: @NUM_TC_THREADS@"), out);
  // E o coletor da aba Abertos VE esse mesmo token na arvore carregada.
  const iss = core.collectOpenIssues(tree);
  assert.strictEqual(iss.length, 1);
  assert.strictEqual(iss[0].token, "NUM_TC_THREADS");
  assert.strictEqual(iss[0].slotName, "debugLevel");
});

/* ---------------------------------- ascii ---------------------------------- */

test("linha com caractere fora de ASCII gera aviso 'non-ascii' com o numero da linha, mas NAO bloqueia a carga", () => {
  const text = '( Aircraft\n   type: C310 // está acentuado\n)';
  const { tree, warnings, errors } = parser.parseEdlDocument(text, BY_FACTORY);
  assert.strictEqual(errors.length, 0);
  assert.ok(tree);
  assert.ok(warnings.some((w) => w.code === "non-ascii" && w.line === 2));
});

/* -------------------------------- multi-root -------------------------------- */

test("conteudo sobrando apos a primeira forma raiz vira aviso 'multi-root' -- o conversor antigo ignorava isso em silencio", () => {
  const text = "( Aircraft type: C310 ) ( Aircraft type: C999 )";
  const { tree, warnings } = parser.parseEdlDocument(text, BY_FACTORY);
  assert.ok(tree);
  assert.ok(warnings.some((w) => w.code === "multi-root"));
});

/* ----------------------- integração com o catálogo REAL --------------------- */
// Varre TODO '.edl'/'.edl.in' real rastreado sob sandbox/, src/poc/, src/rl/
// (descoberta por caminho, nao lista fixa -- mesmo espirito de
// tests/guard/check_falcons_estrutura.sh) e afirma ZERO avisos de
// fabrica/slot desconhecido contra o catalogo REAL atual: todo arquivo
// destes e' conhecido-bom hoje, entao isto vira uma rede de regressao
// permanente e sem manutencao -- qualquer classe/slot que o catalogo pare
// de reconhecer no futuro quebra este teste na hora.

const CATALOG_PATH = path.join(__dirname, "edl_catalog.generated.json");
const REPO_ROOT = path.resolve(__dirname, "..", "..");

function findEdlFiles(dir) {
  const out = [];
  (function walk(d) {
    let entries;
    try { entries = fs.readdirSync(d, { withFileTypes: true }); } catch { return; }
    for (const e of entries) {
      const p = path.join(d, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.isFile() && (p.endsWith(".edl") || p.endsWith(".edl.in"))) out.push(p);
    }
  })(dir);
  return out;
}

if (fs.existsSync(CATALOG_PATH)) {
  const REAL_CATALOG = JSON.parse(fs.readFileSync(CATALOG_PATH, "utf8"));
  const REAL_BY_FACTORY = core.buildCatalogIndex(REAL_CATALOG);

  const sweepDirs = ["sandbox", "src/poc", "src/rl"].map((d) => path.join(REPO_ROOT, d));
  const files = [].concat(...sweepDirs.map(findEdlFiles));

  test(`varredura de regressao: ${files.length} arquivo(s) .edl/.edl.in reais nao geram aviso de fabrica/slot desconhecido`, () => {
    assert.ok(files.length > 0, "deveria ter encontrado pelo menos um .edl real sob sandbox/src/poc/src/rl para varrer");
    const offenders = [];
    files.forEach((file) => {
      const text = fs.readFileSync(file, "utf8");
      // '@include:' nao e' suportado pelo carregador interativo (ver
      // parseEdlDocument) -- fora do escopo desta varredura. Nenhum arquivo
      // rastreado usa isso hoje (confirmado por grep antes de escrever este
      // teste); se algum passar a usar, cai aqui em vez de falhar
      // silenciosamente contra uma arvore vazia.
      if (text.includes("@include:")) return;
      const rel = path.relative(REPO_ROOT, file);
      const { warnings, errors } = parser.parseEdlDocument(text, REAL_BY_FACTORY);
      if (errors.length) {
        offenders.push(`${rel}: ERRO -- ${errors.map((e) => e.message).join("; ")}`);
        return;
      }
      const bad = warnings.filter((w) => w.code === "unknown-factory" || w.code === "unknown-slot");
      if (bad.length) offenders.push(`${rel}: ${bad.map((w) => w.message).join("; ")}`);
    });
    assert.deepStrictEqual(offenders, [], "\n" + offenders.join("\n"));
  });
} else {
  console.log("aviso - catalogo real ausente (edl_catalog.generated.json) -- pulando varredura de regressao. Rode 'node src/ui/scripts/build.js' primeiro.");
}

/* --------------------------------------------------------------------------- */

if (failures > 0) {
  console.log(`\n${failures} teste(s) falharam`);
  process.exit(1);
}
console.log("\nOK");
