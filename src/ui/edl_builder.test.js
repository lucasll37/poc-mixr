#!/usr/bin/env node
// Testes de unidade PUROS da logica de src/ui/edl_builder_core.js -- sem
// Babel, sem React, sem DOM. Mesmo estilo dos testes Python do resto do
// repositorio (sem framework, uma lista de casos, um contador de falhas).
//
// Uso: node src/ui/edl_builder.test.js
"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const core = require("./edl_builder_core.js");

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
// Pequeno, de proposito -- so o suficiente pra exercitar cada regra do
// classificador/serializador sem depender do catalogo real (esse ganha um
// teste de integracao a parte, mais abaixo).

const CATALOG = [
  { class: "Aircraft", factory: "Aircraft", baseClass: "Player", chain: ["Aircraft", "Player", "Object"], origin: "models",
    // primaryComponents com so' DOIS papeis (nao os 10 reais) -- suficiente
    // pra exercitar o mecanismo sem depender do catalogo real (esse ganha
    // teste de integracao a parte, mais abaixo).
    primaryComponents: [
      { role: "dynamicsModel", baseClass: "DynamicsModel" },
      { role: "pilot", baseClass: "Pilot" },
    ],
    slots: [
      { name: "type", declaredIn: "Aircraft", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "initAlt", declaredIn: "Aircraft", comment: "", acceptsNumber: true, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: false, unitFamilies: [{ family: "Distance", options: ["Meters", "Feet"] }], objectTypes: [], isReference: false },
      { name: "dynamicsModel", declaredIn: "Player", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: ["DynamicsModel"], isReference: false },
      { name: "select", declaredIn: "Player", comment: "", acceptsNumber: true, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "ranges", declaredIn: "Player", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: true, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "modes", declaredIn: "Player", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: true, unitFamilies: [], objectTypes: ["Aircraft"], isReference: false },
      { name: "trackManagerName", declaredIn: "Player", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: true },
      // 'components' generico -- o mesmo mecanismo que o Player de verdade
      // usa pra hospedar os itens que satisfazem primaryComponents acima
      // (ver o comentario de roleFillStatus() em edl_builder_core.js).
      { name: "components", declaredIn: "Component", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: true, unitFamilies: [], objectTypes: ["Component"], isReference: false },
    ] },
  // concrete:true -- usado pelos testes novos de isOfferable() (candidata
  // tipo-compativel E concreta, ver mais abaixo). As demais entradas deste
  // catalogo sintetico NAO ganham o campo (fica 'undefined' -- isOfferable
  // trata isso como nao-concreto, mesmo comportamento de um catalogo real
  // onde toda entrada TEM a chave): so' se acrescenta 'concrete' onde um
  // teste novo de fato precisa dele, os testes de isCompatible() acima
  // continuam intocados/alheios a essa chave.
  { class: "JSBSimModel", factory: "JSBSimModel", baseClass: "DynamicsModel", chain: ["JSBSimModel", "DynamicsModel", "Component", "Object"], origin: "models", concrete: true,
    slots: [
      { name: "model", declaredIn: "JSBSimModel", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
    ] },
  // Espelha o caso real (models::DynamicsModel, orfao conhecido): registrada
  // no catalogo (IMPLEMENT_ABSTRACT_SUBCLASS tambem entra em
  // build_factory_map()), tipo-compativel com qualquer slot que aceite
  // "DynamicsModel" (esta' na PROPRIA cadeia), mas sem despacho real --
  // concrete:false. So' existe pra exercitar isOfferable()/compatibleFactories().
  { class: "DynamicsModel", factory: "DynamicsModel", baseClass: "Component", chain: ["DynamicsModel", "Component", "Object"], origin: "models", concrete: false,
    slots: [] },
  { class: "Autopilot", factory: "Autopilot", baseClass: "Pilot", chain: ["Autopilot", "Pilot", "Component", "Object"], origin: "models",
    slots: [
      // acceptsChildList sem NENHUM objectType declarado -- o caso que
      // generate_edl_catalog.py cobre com LIST_SLOT_TYPE_OVERRIDES/
      // TEXT_ONLY_LIST_SLOTS (ex.: Station.networks, TacviewOutput.typeMap);
      // aqui simula um slot que NENHUMA das duas tabelas cobriu -- deve
      // recusar toda candidata, nao aceitar qualquer uma.
      { name: "wildcardList", declaredIn: "Autopilot", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: true, unitFamilies: [], objectTypes: [], isReference: false },
    ] },
  { class: "Station", factory: "Station", baseClass: "Component", chain: ["Station", "Component", "Object"], origin: "simulation", slots: [] },
  { class: "ClockStation", factory: "ClockStation", baseClass: "Station", chain: ["ClockStation", "Station", "Component", "Object"], origin: "libs", slots: [] },
];
const BY_FACTORY = core.buildCatalogIndex(CATALOG);

/* ----------------------------- modelo de dados ---------------------------- */

test("makeNode gera ids crescentes e distintos", () => {
  const a = core.makeNode("Aircraft");
  const b = core.makeNode("Aircraft");
  assert.notStrictEqual(a.id, b.id);
});

test("findNode acha na raiz e em profundidade", () => {
  const root = core.makeNode("Aircraft");
  const child = core.makeNode("JSBSimModel");
  root.children.dynamicsModel = [{ key: "1", node: child }];
  assert.strictEqual(core.findNode(root, root.id), root);
  assert.strictEqual(core.findNode(root, child.id), child);
  assert.strictEqual(core.findNode(root, 99999), null);
});

test("updateNode troca so o no visado, preservando o resto por referencia", () => {
  const root = core.makeNode("Aircraft");
  const child = core.makeNode("JSBSimModel");
  root.children.dynamicsModel = [{ key: "1", node: child }];
  const untouchedSlot = root.children.dynamicsModel;
  const updated = core.updateNode(root, child.id, (n) => ({ ...n, slotValues: { model: { kind: "text", value: "A4" } } }));
  assert.notStrictEqual(updated, root, "a raiz devia clonar (um descendente mudou)");
  assert.notStrictEqual(updated.children.dynamicsModel, untouchedSlot);
  assert.strictEqual(updated.children.dynamicsModel[0].node.slotValues.model.value, "A4");
  assert.strictEqual(root.children.dynamicsModel[0].node.slotValues.model, undefined, "original nao pode mudar (imutavel)");
});

test("updateNode e no-op estrutural quando o id nao existe (mesma referencia)", () => {
  const root = core.makeNode("Aircraft");
  const updated = core.updateNode(root, 999999, (n) => ({ ...n }));
  assert.strictEqual(updated, root);
});

test("removeNode remove um filho profundo sem afetar irmaos", () => {
  const root = core.makeNode("Aircraft");
  const a = core.makeNode("JSBSimModel");
  const b = core.makeNode("Aircraft");
  root.children.dynamicsModel = [{ key: "1", node: a }];
  root.children.modes = [{ key: "1", node: b }];
  const updated = core.removeNode(root, a.id);
  assert.strictEqual(updated.children.dynamicsModel.length, 0);
  assert.strictEqual(updated.children.modes.length, 1);
});

test("removeNode na propria raiz devolve null (chamador trata a parte)", () => {
  const root = core.makeNode("Aircraft");
  assert.strictEqual(core.removeNode(root, root.id), null);
});

/* ------------------------------ compatibilidade --------------------------- */

test("isCompatible aceita a classe exata declarada no slot", () => {
  const slot = BY_FACTORY.Aircraft.slots.find((s) => s.name === "dynamicsModel");
  assert.ok(!core.isCompatible("Aircraft", slot, BY_FACTORY), "Aircraft nao deveria servir em dynamicsModel");
  assert.ok(core.isCompatible("JSBSimModel", slot, BY_FACTORY), "JSBSimModel deveria servir em dynamicsModel");
});

test("isCompatible aceita subclasse (via cadeia de heranca), nao so a classe exata", () => {
  // 'modes' aceita objectTypes: ["Aircraft"] -- Aircraft serve nele mesmo (a
  // propria classe), simulando o padrao real de RfSensor.modes aceitando
  // RfSensor (ela mesma) como filho unico.
  const slot = BY_FACTORY.Aircraft.slots.find((s) => s.name === "modes");
  assert.ok(core.isCompatible("Aircraft", slot, BY_FACTORY));
  assert.ok(!core.isCompatible("Autopilot", slot, BY_FACTORY));
});

test("isCompatible com fabrica desconhecida nao explode, so devolve false", () => {
  const slot = BY_FACTORY.Aircraft.slots.find((s) => s.name === "dynamicsModel");
  assert.strictEqual(core.isCompatible("NadaAVerComIsso", slot, BY_FACTORY), false);
});

test("isCompatible: slot-lista SEM objectTypes declarado NAO aceita nenhuma candidata (sem fallback permissivo)", () => {
  // generate_edl_catalog.py cobre os casos reais conhecidos (Station.
  // networks -> AbstractNetIO, TacviewOutput.typeMap -> textOnly, etc, ver
  // LIST_SLOT_TYPE_OVERRIDES/TEXT_ONLY_LIST_SLOTS) -- o que sobra vazio
  // aqui, no NIVEL DE isCompatible(), e' porque generate_edl_catalog.py nao
  // sabe o tipo de verdade, e a resposta certa e' recusar toda classe (o
  // usuario so pode adicionar um item de TEXTO), nunca "aceitar qualquer
  // coisa" (o comportamento antigo, removido).
  const slot = BY_FACTORY.Autopilot.slots.find((s) => s.name === "wildcardList");
  assert.strictEqual(core.isCompatible("Aircraft", slot, BY_FACTORY), false);
  assert.strictEqual(core.isCompatible("JSBSimModel", slot, BY_FACTORY), false);
});

test("isCompatible: slot de objeto UNICO sem tipo tambem recusa toda candidata", () => {
  // Um slot que NAO aceita lista (acceptsChildList:false) e tambem nao
  // declara objectTypes e, por definicao, um slot so de folha (numero/
  // texto/etc) -- nunca deveria aparecer como "aceita filho" pra comecar
  // (isChildSlot() ja o excluiria), mas isCompatible() sozinho tem que
  // recusar candidatas nesse caso tambem.
  const slotSemLista = { acceptsChildList: false, objectTypes: [] };
  assert.strictEqual(core.isCompatible("Aircraft", slotSemLista, BY_FACTORY), false);
});

test("isCompatible: raiz restrita a Station serve para ClockStation (subclasse) mas nao para Aircraft", () => {
  const slotRaiz = { acceptsChildList: false, objectTypes: ["Station"] };
  assert.ok(core.isCompatible("Station", slotRaiz, BY_FACTORY));
  assert.ok(core.isCompatible("ClockStation", slotRaiz, BY_FACTORY), "ClockStation deriva de Station, deveria servir");
  assert.ok(!core.isCompatible("Aircraft", slotRaiz, BY_FACTORY), "Aircraft nao deriva de Station, nao deveria servir");
});

/* -------------------------------- isOfferable ------------------------------ */
// isOfferable() = isCompatible() E concrete:true -- so' usado nos lugares que
// OFERECEM uma classe ao usuario (paleta, dropdown AddViaSelect, drag-and-
// drop), nunca em isCompatible() em si (ver o comentario dela em
// edl_builder_core.js). "DynamicsModel" (no CATALOG sintetico acima) espelha
// o caso real de uma classe abstrata/orfa: tipo-compativel com o slot
// 'dynamicsModel' de Aircraft (esta' na propria cadeia de heranca), mas sem
// despacho real (concrete:false).

test("isOfferable: candidata tipo-compativel mas NAO concreta -- isCompatible aceita, isOfferable recusa", () => {
  const slot = BY_FACTORY.Aircraft.slots.find((s) => s.name === "dynamicsModel");
  assert.ok(core.isCompatible("DynamicsModel", slot, BY_FACTORY),
    "DynamicsModel esta na propria cadeia de heranca do slot -- isCompatible deveria aceitar");
  assert.strictEqual(core.isOfferable("DynamicsModel", slot, BY_FACTORY), false,
    "DynamicsModel nao e' concreta (concrete:false) -- isOfferable deveria recusar");
});

test("isOfferable: candidata tipo-compativel E concreta -- os dois aceitam", () => {
  const slot = BY_FACTORY.Aircraft.slots.find((s) => s.name === "dynamicsModel");
  assert.ok(core.isCompatible("JSBSimModel", slot, BY_FACTORY));
  assert.strictEqual(core.isOfferable("JSBSimModel", slot, BY_FACTORY), true,
    "JSBSimModel e' concreta (concrete:true) e tipo-compativel -- isOfferable deveria aceitar");
});

test("isOfferable: fabrica desconhecida ou sem campo 'concrete' nenhum nao explode, so' devolve false", () => {
  const slot = BY_FACTORY.Aircraft.slots.find((s) => s.name === "dynamicsModel");
  assert.strictEqual(core.isOfferable("NadaAVerComIsso", slot, BY_FACTORY), false);
  // Autopilot nao declara 'concrete' nenhum no fixture (undefined) -- trata
  // como nao-concreto, nunca lanca excecao.
  const slotWildcard = { acceptsChildList: false, objectTypes: ["Autopilot"] };
  assert.strictEqual(core.isOfferable("Autopilot", slotWildcard, BY_FACTORY), false);
});

test("compatibleFactories: nunca lista uma fabrica com concrete:false, mesmo quando isCompatible sozinha aceitaria", () => {
  const slot = BY_FACTORY.Aircraft.slots.find((s) => s.name === "dynamicsModel");
  const options = core.compatibleFactories(slot, CATALOG, BY_FACTORY);
  assert.ok(options.includes("JSBSimModel"), `esperava JSBSimModel em ${JSON.stringify(options)}`);
  assert.ok(!options.includes("DynamicsModel"),
    `DynamicsModel (concrete:false) nao deveria aparecer em compatibleFactories, veio ${JSON.stringify(options)}`);
});

/* --------------------- papéis primários / placeholders --------------------- */
// dynamicsModel/pilot/.../storesMgr NÃO são slots de verdade (ver o
// comentário de primaryRolesFor() em edl_builder_core.js) -- são resolvidos
// por TIPO dentro da lista genérica 'components'. Aqui a árvore de teste
// simula isso com o CATALOG sintético acima (só 2 papéis, não os 10 reais).

test("primaryRolesFor devolve os papeis declarados pelo catalogo", () => {
  const roles = core.primaryRolesFor("Aircraft", BY_FACTORY);
  assert.deepStrictEqual(roles.map((r) => r.role), ["dynamicsModel", "pilot"]);
});

test("primaryRolesFor devolve lista vazia pra classe sem primaryComponents (nao explode)", () => {
  assert.deepStrictEqual(core.primaryRolesFor("Station", BY_FACTORY), []);
  assert.deepStrictEqual(core.primaryRolesFor("NadaAVerComIsso", BY_FACTORY), []);
});

test("roleFillStatus acha o item cuja cadeia de heranca bate com o baseClass do papel", () => {
  const root = core.makeNode("Aircraft");
  const engine = core.makeNode("JSBSimModel");
  root.children.components = [{ key: "dynamicsModel", node: engine }];
  const role = core.primaryRolesFor("Aircraft", BY_FACTORY)[0]; // dynamicsModel
  const found = core.roleFillStatus(root, role, BY_FACTORY);
  assert.strictEqual(found && found.node, engine);
});

test("roleFillStatus devolve null quando nenhum item satisfaz o papel -- vira placeholder", () => {
  const root = core.makeNode("Aircraft");
  root.children.components = [{ key: "dynamicsModel", node: core.makeNode("JSBSimModel") }];
  const pilotRole = core.primaryRolesFor("Aircraft", BY_FACTORY)[1]; // pilot
  assert.strictEqual(core.roleFillStatus(root, pilotRole, BY_FACTORY), null);
});

test("roleFillStatus: primeiro item que casa vence, igual a findByType() real", () => {
  const root = core.makeNode("Aircraft");
  const first = core.makeNode("JSBSimModel");
  const second = core.makeNode("JSBSimModel");
  root.children.components = [{ key: "1", node: first }, { key: "2", node: second }];
  const role = core.primaryRolesFor("Aircraft", BY_FACTORY)[0];
  const found = core.roleFillStatus(root, role, BY_FACTORY);
  assert.strictEqual(found.node, first, "deveria devolver o PRIMEIRO candidato, nao o segundo");
});

test("roleFillStatus atravessa item de texto sem quebrar (children:{} generico, ver makeTextLeaf)", () => {
  const root = core.makeNode("Aircraft");
  root.children.components = [
    { key: "apelido", node: core.makeTextLeaf("nao e um componente de verdade") },
    { key: "dynamicsModel", node: core.makeNode("JSBSimModel") },
  ];
  const role = core.primaryRolesFor("Aircraft", BY_FACTORY)[0];
  const found = core.roleFillStatus(root, role, BY_FACTORY);
  assert.strictEqual(found.key, "dynamicsModel");
});

/* ---------------------------------- ascii ---------------------------------- */

test("isAscii aceita texto ASCII puro", () => {
  assert.ok(core.isAscii("Estacao normal, sem acento, 42"));
});

test("isAscii rejeita um unico caractere acentuado -- a mesma armadilha do parser real", () => {
  // Confirmado no CLAUDE.md: um SO acento em qualquer lugar do arquivo
  // derruba o parse inteiro do bison/flex deste fork, com "syntax error"
  // sem dizer o motivo -- por isso a exportacao tem de barrar isso ANTES
  // de escrever o arquivo, nao depois.
  assert.ok(!core.isAscii("estação"));
});

/* ------------------------------ texto literal ------------------------------ */

test("serializeTextLiteral emite identificador NU quando o charset permite", () => {
  assert.strictEqual(core.serializeTextLiteral("falcon1"), "falcon1");
  assert.strictEqual(core.serializeTextLiteral("C310"), "C310");
});

test("serializeTextLiteral poe aspas quando ha espaco ou caractere fora do charset", () => {
  assert.strictEqual(core.serializeTextLiteral("Air+FixedWing"), "Air+FixedWing", "'+' esta no charset do scanner, deveria continuar nu");
  assert.strictEqual(core.serializeTextLiteral("nome com espaco"), JSON.stringify("nome com espaco"));
});

test("serializeTextLiteral forca aspas em texto puramente numerico -- IDENT nu viraria NUMERO no scanner", () => {
  // '310' sem aspas tokenizaria como INTEGERconstant (base::Integer), nao
  // como IDENT (base::Identifier) -- errado para um slot de texto.
  assert.strictEqual(core.serializeTextLiteral("310"), JSON.stringify("310"));
  assert.strictEqual(core.serializeTextLiteral("3.5"), JSON.stringify("3.5"));
});

test("serializeTextLiteral deixa passar cru um VETOR com notacao cientifica/'.5'/'5.' -- caso Table2/Table3.data", () => {
  // ACHADO POR AUDITORIA (nao redescobrir): RAW_VECTOR_RE so' reconhecia
  // '\d+(\.\d+)?' -- notacao cientifica/'.5'/'5.' caiam no fallback de
  // JSON.stringify() e viravam base::String, quebrando Table2::loadData()
  // (espera um base::List numerico de verdade).
  assert.strictEqual(core.serializeTextLiteral("[ 1 2.5e-3 3 ]"), "[ 1 2.5e-3 3 ]");
  assert.strictEqual(core.serializeTextLiteral("[ .5 5. -1.2E+10 ]"), "[ .5 5. -1.2E+10 ]");
});

/* ------------------------------ valor de slot ------------------------------ */

test("serializeLeafValue: numero simples", () => {
  assert.strictEqual(core.serializeLeafValue({ acceptsNumber: true }, { kind: "number", value: 42 }), "42");
});

test("serializeLeafValue: booleano", () => {
  assert.strictEqual(core.serializeLeafValue({}, { kind: "boolean", value: true }), "true");
  assert.strictEqual(core.serializeLeafValue({}, { kind: "boolean", value: false }), "false");
});

test("serializeLeafValue: vetor numerico vira [ a b c ]", () => {
  assert.strictEqual(core.serializeLeafValue({}, { kind: "vector", value: "10 20  40\t80" }), "[ 10 20 40 80 ]");
});

test("serializeLeafValue: unidade escolhida vira objeto aninhado ( Unidade valor )", () => {
  assert.strictEqual(core.serializeLeafValue({}, { kind: "unit", value: 1750, unit: "Meters" }), "( Meters 1750 )");
});

test("serializeLeafValue: unidade 'sem unidade' vira numero cru", () => {
  assert.strictEqual(core.serializeLeafValue({ acceptsNumber: true }, { kind: "unit", value: 5, unit: "" }), "5");
});

test("serializeLeafValue: texto-ou-numero (Component.select) emite numero cru quando parece numero", () => {
  const slotDef = { acceptsNumber: true, acceptsText: true };
  assert.strictEqual(core.serializeLeafValue(slotDef, { kind: "text", value: "2" }), "2");
  assert.strictEqual(core.serializeLeafValue(slotDef, { kind: "text", value: "falcon2" }), "falcon2");
});

test("serializeLeafValue: texto-ou-numero reconhece notacao cientifica/'.5'/'5.' -- sem isso viraria base::String", () => {
  // ACHADO POR AUDITORIA (nao redescobrir): LOOKS_LIKE_NUMBER_RE so'
  // reconhecia '\d+(\.\d+)?'; um valor como "1.5e-3" num slot que ACEITA
  // numero caia no fallback de serializeTextLiteral() e saia ENTRE ASPAS --
  // trocando o tipo real do lado MIXR (Number esperado, String entregue).
  const slotDef = { acceptsNumber: true, acceptsText: true };
  assert.strictEqual(core.serializeLeafValue(slotDef, { kind: "text", value: "1.5e-3" }), "1.5e-3");
  assert.strictEqual(core.serializeLeafValue(slotDef, { kind: "text", value: ".5" }), ".5");
  assert.strictEqual(core.serializeLeafValue(slotDef, { kind: "text", value: "5." }), "5.");
  assert.strictEqual(core.serializeLeafValue(slotDef, { kind: "text", value: "-1.2E+10" }), "-1.2E+10");
});

test("serializeLeafValue: sem valor (undefined) devolve null -- slot fica de fora do .edl", () => {
  assert.strictEqual(core.serializeLeafValue({}, undefined), null);
});

test("serializeLeafValue: texto limpo de volta pra vazio ('' ou so' espaco) tambem devolve null -- limpar o campo REMOVE o slot, nao vira 'chave: \"\"'", () => {
  assert.strictEqual(core.serializeLeafValue({}, { kind: "text", value: "" }), null);
  assert.strictEqual(core.serializeLeafValue({}, { kind: "text", value: "   " }), null);
});

test("serializeLeafValue: vetor limpo de volta pra vazio tambem devolve null (nao vira '[  ]')", () => {
  assert.strictEqual(core.serializeLeafValue({}, { kind: "vector", value: "" }), null);
});

test("isEmptyLeafValue: numero 0 e booleano false NAO sao 'vazios' -- sao valores legitimos", () => {
  assert.strictEqual(core.isEmptyLeafValue({ kind: "number", value: 0 }), false);
  assert.strictEqual(core.isEmptyLeafValue({ kind: "boolean", value: false }), false);
});

test("serializeTextLiteral: barra invertida NAO dobra ao serializar -- o tokenizer real nunca desfaz o escape do JSON.stringify puro", () => {
  // ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): serializeTextLiteral()
  // usava JSON.stringify(v) sozinho, que ESCAPA barra invertida (\ -> \\).
  // Como o tokenizer (edl_scanner.l real, e tokenizeEdlText() aqui) nunca
  // interpreta \X como escape -- copia literal, byte a byte -- um ciclo
  // carregar+exportar DOBRAVA a contagem de barras de um valor com \ literal
  // (ex.: caminho estilo Windows). Nao-idempotente: 1 -> 3 -> 7 -> 15 barras
  // em ciclos repetidos.
  const original = "C:\\data\\mission.acmi"; // uma barra entre cada segmento
  const serializado = core.serializeTextLiteral(original);
  assert.strictEqual(serializado, '"C:\\data\\mission.acmi"');
});

test("serializeTextLiteral: idempotente em ciclos repetidos de serializar+desserializar", () => {
  let valor = "C:\\data\\mission.acmi";
  for (let i = 0; i < 5; i++) {
    const serial = core.serializeTextLiteral(valor);
    valor = serial.slice(1, -1); // o tokenizer real copia o conteudo entre aspas literal, sem reinterpretar
    assert.strictEqual(valor, "C:\\data\\mission.acmi", `divergiu no ciclo ${i + 1}`);
  }
});

test("serializeTextLiteral: aspa embutida continua escapada (nao mexeu no que ja funcionava)", () => {
  assert.strictEqual(core.serializeTextLiteral('diz "oi"'), '"diz \\"oi\\""');
});

test("projectToEdl: editar um campo e depois limpar de volta pra vazio nao deixa o slot no .edl exportado", () => {
  const n = core.makeNode("JSBSimModel");
  n.slotValues.model = { kind: "text", value: "A4" };
  let text = core.projectToEdl(n, BY_FACTORY);
  assert.ok(text.includes("model: A4"), text);
  // "limpar o campo" -- o widget continua mandando um valor (nao apaga a
  // chave do objeto), so' com texto vazio. E' esse valor vazio que
  // serializeLeafValue/isEmptyLeafValue tem que tratar como "sem valor".
  n.slotValues.model = { kind: "text", value: "" };
  text = core.projectToEdl(n, BY_FACTORY);
  assert.ok(!text.includes("model:"), text);
});

/* --------------------------------- projeto inteiro -------------------------- */

test("projectToEdl: um no folha sem filhos serializa so os slots preenchidos", () => {
  const n = core.makeNode("JSBSimModel");
  n.slotValues.model = { kind: "text", value: "A4" };
  const text = core.projectToEdl(n, BY_FACTORY);
  assert.strictEqual(text, "( JSBSimModel\n   model: A4\n) // JSBSimModel\n");
});

test("projectToEdl: slot de objeto UNICO serializa em forma NUA, sem chaves", () => {
  const root = core.makeNode("Aircraft");
  const engine = core.makeNode("JSBSimModel");
  engine.slotValues.model = { kind: "text", value: "A4" };
  root.children.dynamicsModel = [{ key: "1", node: engine }];
  const text = core.projectToEdl(root, BY_FACTORY);
  assert.ok(text.includes("dynamicsModel: ( JSBSimModel"), text);
  assert.ok(!text.includes("dynamicsModel: {"), "slot de objeto unico nao pode virar lista com chaves");
});

test("projectToEdl: slot de LISTA serializa com chaves e as chaves escolhidas pelo usuario", () => {
  const root = core.makeNode("Aircraft");
  const a = core.makeNode("Aircraft");
  const b = core.makeNode("Aircraft");
  root.children.modes = [{ key: "falcon1", node: a }, { key: "falcon2", node: b }];
  const text = core.projectToEdl(root, BY_FACTORY);
  assert.ok(text.includes("modes: {"), text);
  assert.ok(text.includes("falcon1: ( Aircraft"), text);
  assert.ok(text.includes("falcon2: ( Aircraft"), text);
});

test("projectToEdl: um SO item numa lista ainda sai com chaves (nunca vira forma nua)", () => {
  // RfSensor::setSlotModeSingle() (fonte real) converte a forma nua em
  // exatamente esta mesma estrutura por baixo -- as duas formas sao
  // semanticamente identicas, entao emitir sempre a de lista e seguro e
  // mais simples (sem alternar forma conforme o numero de itens).
  const root = core.makeNode("Aircraft");
  const a = core.makeNode("Aircraft");
  root.children.modes = [{ key: "1", node: a }];
  const text = core.projectToEdl(root, BY_FACTORY);
  assert.ok(text.includes("modes: {\n"), text);
  assert.ok(text.includes("1: ( Aircraft"), text);
});

test("projectToEdl: arvore vazia (null) serializa para string vazia", () => {
  assert.strictEqual(core.projectToEdl(null, BY_FACTORY), "");
});

/* ------------------------------ folha de texto ------------------------------ */
// TacviewOutput.modelMap/typeMap/colorMap (catalogo real) sao PairStream de
// VALOR ESCALAR -- cada item e um base::String, nao uma classe MIXR. Sem
// isso, o slot era um beco sem saida (nenhuma classe do catalogo "servia"
// como filho, ver o teste de isCompatible acima).

test("makeTextLeaf cria um no minimo, com id proprio e children vazio (generico o suficiente pra findNode/etc.)", () => {
  const leaf = core.makeTextLeaf("A-4E");
  assert.strictEqual(leaf.isText, true);
  assert.strictEqual(leaf.text, "A-4E");
  assert.deepStrictEqual(leaf.children, {});
  assert.ok(typeof leaf.id === "number");
});

test("projectToEdl: item de texto numa lista serializa 'chave: valor', nao 'chave: ( Classe ... )'", () => {
  const root = core.makeNode("Aircraft");
  root.children.modes = [
    { key: "falcon1", node: core.makeTextLeaf("A-4E") },
    { key: "falcon2", node: core.makeTextLeaf("nome com espaco") },
  ];
  const text = core.projectToEdl(root, BY_FACTORY);
  assert.ok(text.includes("falcon1: A-4E"), text);
  assert.ok(text.includes(`falcon2: ${JSON.stringify("nome com espaco")}`), text);
  assert.ok(!text.includes("( A-4E"), "item de texto nao pode virar uma forma ( Classe ... )");
});

test("findNode/removeNode atravessam um item de texto sem quebrar (children:{} generico)", () => {
  const root = core.makeNode("Aircraft");
  const leaf = core.makeTextLeaf("A-4E");
  root.children.modes = [{ key: "1", node: leaf }];
  assert.strictEqual(core.findNode(root, leaf.id), leaf);
  const removed = core.removeNode(root, leaf.id);
  assert.strictEqual(removed.children.modes.length, 0);
});

/* ----------------------- integração com o catálogo REAL --------------------- */
// Carrega src/ui/edl_catalog.generated.json (gerado por
// src/ui/scripts/generate_edl_catalog.py, via `make open-edl-builder`) e
// monta uma arvore minima real (Station -> ... ), pra pegar qualquer
// divergencia entre o catalogo de verdade e as regras acima que o catalogo
// sintetico, pequeno demais de proposito, nao exercitaria.

const CATALOG_PATH = path.join(__dirname, "edl_catalog.generated.json");
if (fs.existsSync(CATALOG_PATH)) {
  const REAL_CATALOG = JSON.parse(fs.readFileSync(CATALOG_PATH, "utf8"));
  const REAL_BY_FACTORY = core.buildCatalogIndex(REAL_CATALOG);

  test("integração: Aircraft real NÃO tem slot dedicado 'dynamicsModel' -- é 'components' genérico", () => {
    // Player::updateSystemPointers() acha os DEZ sistemas primarios
    // (DynamicsModel/Pilot/Navigation/Datalink/Radio/Gimbal/RfSensor/
    // IrSystem/OnboardComputer/StoresMgr) por findByType() dentro da lista
    // GENERICA 'components:' (herdada de Component) -- 'dynamicsModel:'/
    // 'pilot:' sao so CONVENCAO de nome de chave no .edl, nao slots
    // proprios; confirmado direto no catalogo real (grep na lista de
    // nomes de Aircraft: so 'components', nunca 'dynamicsModel').
    const names = REAL_BY_FACTORY.Aircraft.slots.map((s) => s.name);
    assert.ok(!names.includes("dynamicsModel"), "Aircraft nao deveria ter um slot proprio 'dynamicsModel'");
    const componentsSlot = REAL_BY_FACTORY.Aircraft.slots.find((s) => s.name === "components");
    assert.ok(componentsSlot, "Aircraft deveria herdar 'components' de Component");
    assert.ok(componentsSlot.acceptsChildList, "'components' deveria ser uma lista");
    assert.ok(core.isCompatible("JSBSimModel", componentsSlot, REAL_BY_FACTORY),
      "JSBSimModel deveria caber dentro de 'components' (aceita qualquer Component)");
  });

  test("integração: monta Aircraft com JSBSimModel/Autopilot dentro de 'components' e exporta .edl plausível", () => {
    const plane = core.makeNode("Aircraft");
    plane.slotValues.type = { kind: "text", value: "C310" };
    const engine = core.makeNode("JSBSimModel");
    engine.slotValues.model = { kind: "text", value: "A4" };
    const pilot = core.makeNode("Autopilot");
    // As chaves ('dynamicsModel'/'pilot') sao so as convencionais deste
    // repositorio -- o C++ acha os dois por TIPO, nao por este nome.
    plane.children.components = [
      { key: "dynamicsModel", node: engine },
      { key: "pilot", node: pilot },
    ];

    const text = core.projectToEdl(plane, REAL_BY_FACTORY);
    assert.ok(core.isAscii(text), "saida deveria ser ASCII puro");
    assert.ok(text.startsWith("( Aircraft"), text.slice(0, 40));
    assert.ok(text.includes("type: C310"), text);
    assert.ok(text.includes("components: {"), text);
    assert.ok(text.includes("dynamicsModel: ( JSBSimModel"), text);
    assert.ok(text.includes("model: A4"), text);
    assert.ok(text.includes("pilot: ( Autopilot"), text);

    // Os placeholders da arvore se apoiam em primaryRolesFor/roleFillStatus
    // sobre o CATALOGO REAL -- confirma que os dois papeis preenchidos
    // acima (dynamicsModel/pilot) sao encontrados, e que um papel ausente
    // (ex.: storesMgr, nao adicionado neste cenario minimo) vira null.
    const roles = core.primaryRolesFor("Aircraft", REAL_BY_FACTORY);
    assert.strictEqual(roles.length, 10, `Aircraft deveria herdar os 10 papeis de Player, achou ${roles.length}`);
    const dynRole = roles.find((r) => r.role === "dynamicsModel");
    const pilotRole = roles.find((r) => r.role === "pilot");
    const storesRole = roles.find((r) => r.role === "storesMgr");
    assert.strictEqual(core.roleFillStatus(plane, dynRole, REAL_BY_FACTORY).node, engine);
    assert.strictEqual(core.roleFillStatus(plane, pilotRole, REAL_BY_FACTORY).node, pilot);
    assert.strictEqual(core.roleFillStatus(plane, storesRole, REAL_BY_FACTORY), null,
      "storesMgr nao foi adicionado neste cenario minimo -- deveria virar placeholder, nao um falso-positivo");
  });

}

/* ------------------------- destaque de sintaxe .edl ------------------------ */
// Porta as regras de .vscode/extensions/edl/syntaxes/edl.tmLanguage.json --
// o invariante mais importante e' o ROUND-TRIP (nunca perder/duplicar um
// caractere só por colorir), testado explicitamente abaixo.

function joinTokens(tokens) {
  return tokens.map((t) => t.text).join("");
}
function clsOf(tokens, text) {
  const t = tokens.find((tk) => tk.text === text);
  return t ? t.cls : undefined;
}

test("tokenizeEdlText: round-trip -- concatenar os tokens reproduz o texto original", () => {
  const sample = '( ClockStation\n   tcRate: ( Hertz 50 )\n   // comentario\n   ownship: "bandit1"\n   ativo: true\n   cor: <ff00ff>\n   ranges: [ 1 2 3 ]\n) // ClockStation\n';
  const tokens = core.tokenizeEdlText(sample);
  assert.strictEqual(joinTokens(tokens), sample);
});

test("tokenizeEdlText: nome de classe logo apos '(' vira tok-class", () => {
  const tokens = core.tokenizeEdlText("( Aircraft type: C310 )");
  assert.strictEqual(clsOf(tokens, "Aircraft"), "tok-class");
});

test("tokenizeEdlText: nome de slot logo antes de ':' vira tok-slot", () => {
  const tokens = core.tokenizeEdlText("( Aircraft type: C310 )");
  assert.strictEqual(clsOf(tokens, "type"), "tok-slot");
});

test("tokenizeEdlText: comentario // vira tok-comment ate o fim da linha", () => {
  const tokens = core.tokenizeEdlText("( Aircraft ) // fim\n");
  assert.strictEqual(clsOf(tokens, "// fim"), "tok-comment");
});

test("tokenizeEdlText: string entre aspas vira tok-string", () => {
  const tokens = core.tokenizeEdlText('nome: "falcon 1"');
  assert.strictEqual(clsOf(tokens, '"falcon 1"'), "tok-string");
});

test("tokenizeEdlText: string entre < > (angled) tambem vira tok-string", () => {
  const tokens = core.tokenizeEdlText("cor: <ff00ff>");
  assert.strictEqual(clsOf(tokens, "<ff00ff>"), "tok-string");
});

test("tokenizeEdlText: true/false (so' minusculo ou MAIUSCULO) vira tok-bool", () => {
  const tokens = core.tokenizeEdlText("a: true b: FALSE");
  assert.strictEqual(clsOf(tokens, "true"), "tok-bool");
  assert.strictEqual(clsOf(tokens, "FALSE"), "tok-bool");
});

test("tokenizeEdlText: True/False (case mista) NAO e' booleano -- vira valor solto (regra da gramatica original)", () => {
  const tokens = core.tokenizeEdlText("a: True");
  assert.strictEqual(clsOf(tokens, "True"), "tok-value");
});

test("tokenizeEdlText: numero float e inteiro viram tok-num", () => {
  const tokens = core.tokenizeEdlText("x: 1750.5 y: 42");
  assert.strictEqual(clsOf(tokens, "1750.5"), "tok-num");
  assert.strictEqual(clsOf(tokens, "42"), "tok-num");
});

test("tokenizeEdlText: pontuacao de bloco/lista vira tok-punct", () => {
  const tokens = core.tokenizeEdlText("components: { 1: ( A ) } ranges: [ 1 2 ]");
  assert.strictEqual(clsOf(tokens, "{"), "tok-punct");
  assert.strictEqual(clsOf(tokens, "}"), "tok-punct");
  assert.strictEqual(clsOf(tokens, "["), "tok-punct");
  assert.strictEqual(clsOf(tokens, "]"), "tok-punct");
});

test("tokenizeEdlText: identificador solto (nem apos '(' nem antes de ':') vira tok-value", () => {
  const tokens = core.tokenizeEdlText("alvo: falcon1");
  assert.strictEqual(clsOf(tokens, "falcon1"), "tok-value");
});

test("tokenizeEdlText: round-trip tambem no .edl REAL do preset (se o catalogo existir)", () => {
  if (!fs.existsSync(CATALOG_PATH)) return;
  const REAL_CATALOG = JSON.parse(fs.readFileSync(CATALOG_PATH, "utf8"));
  const REAL_BY_FACTORY = core.buildCatalogIndex(REAL_CATALOG);
  const PRESET_PATH = path.join(__dirname, "preset.json");
  if (!fs.existsSync(PRESET_PATH)) return;
  const scenario = JSON.parse(fs.readFileSync(PRESET_PATH, "utf8"));
  const text = core.projectToEdl(scenario, REAL_BY_FACTORY);
  const tokens = core.tokenizeEdlText(text);
  assert.strictEqual(joinTokens(tokens), text, "round-trip deveria reproduzir o .edl real byte a byte");
});

/* ------------------------- mapa: posições georreferenciadas ------------- */

test("leafValueToMeters: unidade escolhida converte pro fator certo", () => {
  assert.strictEqual(core.leafValueToMeters({ kind: "unit", unit: "NauticalMiles", value: 5 }), 5 * 1852);
  assert.strictEqual(core.leafValueToMeters({ kind: "unit", unit: "KiloMeters", value: 2 }), 2000);
  assert.strictEqual(core.leafValueToMeters({ kind: "unit", unit: "Feet", value: 1000 }), 1000 * 0.3048);
});

test("leafValueToMeters: sem unidade escolhida, o numero cru JA e' em metros (convencao do slot Player)", () => {
  assert.strictEqual(core.leafValueToMeters({ kind: "unit", unit: "", value: 42 }), 42);
});

test("leafValueToMeters: slot-valor ausente ou unidade desconhecida devolve undefined, nao NaN", () => {
  assert.strictEqual(core.leafValueToMeters(undefined), undefined);
  assert.strictEqual(core.leafValueToMeters({ kind: "unit", unit: "NaoExiste", value: 5 }), undefined);
});

test("leafValueToMeters: campo numerico limpo ('') conta como 0 -- number/unit nao tem nocao de 'vazio' (mesma decisao ja registrada em isEmptyLeafValue)", () => {
  assert.strictEqual(core.leafValueToMeters({ kind: "unit", unit: "", value: "" }), 0);
});

test("extractPlacements: so pega nos cuja cadeia inclui Player, nao qualquer componente", () => {
  const station = core.makeNode("Aircraft");
  const dyn = core.makeNode("JSBSimModel");
  station.children.components = [{ key: "dynamicsModel", node: dyn }];
  const placements = core.extractPlacements(station, BY_FACTORY);
  assert.strictEqual(placements.length, 1);
  assert.strictEqual(placements[0].factory, "Aircraft");
});

test("extractPlacements: le initXPos/initYPos/initAlt convertidos pra metros", () => {
  const n = core.makeNode("Aircraft");
  n.slotValues.initXPos = { kind: "unit", unit: "NauticalMiles", value: 5 };
  n.slotValues.initYPos = { kind: "unit", unit: "NauticalMiles", value: -2 };
  n.slotValues.initAlt = { kind: "unit", unit: "Meters", value: 1750 };
  const [p] = core.extractPlacements(n, BY_FACTORY);
  assert.strictEqual(p.north, 5 * 1852);
  assert.strictEqual(p.east, -2 * 1852);
  assert.strictEqual(p.alt, 1750);
  assert.strictEqual(p.hasExplicitPosition, true);
});

test("extractPlacements: posicao ausente conta como 0 (aparece na origem, nao some do mapa)", () => {
  const n = core.makeNode("Aircraft");
  const [p] = core.extractPlacements(n, BY_FACTORY);
  assert.strictEqual(p.north, 0);
  assert.strictEqual(p.east, 0);
  assert.strictEqual(p.hasExplicitPosition, false);
});

test("extractPlacements: percorre players ANINHADOS (ex.: missil dentro de stores)", () => {
  const root = core.makeNode("Aircraft");
  const missile = core.makeNode("Aircraft"); // reaproveita a classe sintetica so pra testar aninhamento
  root.children.components = [{ key: "stores", node: missile }];
  const placements = core.extractPlacements(root, BY_FACTORY);
  assert.strictEqual(placements.length, 2);
});

test("extractPlacements: arvore vazia (null) devolve lista vazia", () => {
  assert.deepStrictEqual(core.extractPlacements(null, BY_FACTORY), []);
});

test("extractPlacements: rotulo usa a CHAVE do pai (convencao 'falcon1: (Aircraft...)'), nao um slot -- Player nao tem slot 'name'", () => {
  const root = core.makeNode("Aircraft");
  const child = core.makeNode("Aircraft");
  root.children.components = [{ key: "falcon1", node: child }];
  const placements = core.extractPlacements(root, BY_FACTORY);
  const rootPlacement = placements.find((p) => p.id === root.id);
  const childPlacement = placements.find((p) => p.id === child.id);
  assert.strictEqual(rootPlacement.label, "Aircraft", "raiz nao tem chave de pai -- cai no nome da fabrica");
  assert.strictEqual(childPlacement.label, "falcon1", "item de lista usa a chave que o pai deu a ele");
});

/* ------------------------------ itens em aberto ----------------------------- */
// NAO ha mais testes de nodePendencies/collectPendencies/emptyChildSlots:
// o conceito de "pendencia" (todo slot-filho declarado e vazio) foi
// REMOVIDO -- media o tamanho da arvore, nao o que falta nela. A nota com
// os numeros medidos esta em edl_builder_core.js, secao "itens em aberto".

test("collectOpenIssues: acha @TOKEN@ tanto em valor NU quanto entre aspas", () => {
  const root = core.makeNode("Aircraft");
  // 'raw' e' o que o parser produz pra slot sem slotDef; 'text' pra string.
  root.slotValues.numTcThreads = { kind: "raw", raw: "@NUM_TC_THREADS@" };
  root.slotValues.fileName = { kind: "text", value: "./data/mission_@RUN_ID@.acmi" };
  const iss = core.collectOpenIssues(root);
  assert.strictEqual(iss.length, 2);
  assert.deepStrictEqual(iss.map((i) => i.token).sort(), ["NUM_TC_THREADS", "RUN_ID"]);
  assert.ok(iss.every((i) => i.nodeId === root.id && i.factory === "Aircraft"));
});

// A varredura ANTIGA (edl_parser_core.js) so' olhava 'raw' e 'text' --
// um '( Meters @ALT@ )' passava mudo. Estes dois tipos de folha existem
// de verdade: assignSlot() dobra '( Meters 500 )' em kind:"unit".
test("collectOpenIssues: acha @TOKEN@ em folha de unidade, numero e vetor", () => {
  const root = core.makeNode("Aircraft");
  root.slotValues.initAlt = { kind: "unit", value: "@ALT@", unit: "Meters" };
  root.slotValues.id = { kind: "number", value: "@ID@", unit: "" };
  root.slotValues.feba = { kind: "vector", value: "@X@ @Y@" };
  const tokens = core.collectOpenIssues(root).map((i) => i.token).sort();
  assert.deepStrictEqual(tokens, ["ALT", "ID", "X", "Y"]);
});

test("collectOpenIssues: acha @TOKEN@ em item de TEXTO dentro de lista", () => {
  const root = core.makeNode("Aircraft");
  root.children.modes = [{ key: "1", node: core.makeTextLeaf("mission_@RUN_ID@") }];
  const iss = core.collectOpenIssues(root);
  assert.strictEqual(iss.length, 1);
  assert.strictEqual(iss[0].token, "RUN_ID");
  assert.strictEqual(iss[0].slotName, "modes");
});

test("collectOpenIssues: desce nos filhos e o rotulo vem da CHAVE do pai", () => {
  const root = core.makeNode("Aircraft");
  const child = core.makeNode("Aircraft");
  child.slotValues.fileName = { kind: "text", value: "@RUN_ID@" };
  root.children.modes = [{ key: "falcon2", node: child }];
  const iss = core.collectOpenIssues(root);
  assert.strictEqual(iss.length, 1);
  assert.strictEqual(iss[0].nodeId, child.id);
  assert.strictEqual(iss[0].label, "falcon2");
});

test("collectOpenIssues: raiz sem chave de pai cai no nome da fabrica", () => {
  const root = core.makeNode("Aircraft");
  root.slotValues.fileName = { kind: "text", value: "@RUN_ID@" };
  assert.strictEqual(core.collectOpenIssues(root)[0].label, "Aircraft");
});

// Duas ocorrencias do MESMO token contam DUAS vezes quando estao em slots
// diferentes -- cada uma e' um campo distinto a corrigir (no A4-6DOF sao
// exatamente os dois fileName com @RUN_ID@). O contador nao deduplica.
test("collectOpenIssues: o mesmo token em slots diferentes conta duas vezes", () => {
  const root = core.makeNode("Aircraft");
  root.slotValues.fileName = { kind: "text", value: "a_@RUN_ID@.acmi" };
  root.slotValues.description = { kind: "text", value: "b_@RUN_ID@.jsonl" };
  assert.strictEqual(core.collectOpenIssues(root).length, 2);
});

test("collectOpenIssues: duas ocorrencias no MESMO valor contam duas vezes", () => {
  const root = core.makeNode("Aircraft");
  root.slotValues.fileName = { kind: "text", value: "@A@/@B@" };
  assert.deepStrictEqual(core.collectOpenIssues(root).map((i) => i.token), ["A", "B"]);
});

test("collectOpenIssues: arvore sem placeholder, e arvore vazia, devolvem lista vazia", () => {
  const root = core.makeNode("Aircraft");
  root.slotValues.fileName = { kind: "text", value: "./data/mission.acmi" };
  root.slotValues.id = { kind: "number", value: "7", unit: "" };
  assert.deepStrictEqual(core.collectOpenIssues(root), []);
  assert.deepStrictEqual(core.collectOpenIssues(null), []);
});

// Um '@' solto NAO e' placeholder -- so' conta a forma fechada @NOME@,
// com o mesmo regex que o parser usa (fonte unica em edl_builder_core.js).
test("collectOpenIssues: '@' solto ou '@nome' sem fechar nao conta", () => {
  const root = core.makeNode("Aircraft");
  root.slotValues.fileName = { kind: "text", value: "user@host e @aberto" };
  assert.deepStrictEqual(core.collectOpenIssues(root), []);
});

test("findAncestorPath: caminho da raiz ate um no profundo, incluindo as duas pontas", () => {
  const root = core.makeNode("Aircraft");
  const mid = core.makeNode("Aircraft");
  const leaf = core.makeNode("JSBSimModel");
  root.children.modes = [{ key: "1", node: mid }];
  mid.children.components = [{ key: "dynamicsModel", node: leaf }];
  const path = core.findAncestorPath(root, leaf.id);
  assert.deepStrictEqual(path, [root.id, mid.id, leaf.id]);
});

test("findAncestorPath: id inexistente devolve null", () => {
  const root = core.makeNode("Aircraft");
  assert.strictEqual(core.findAncestorPath(root, 999999), null);
});

test("findAncestorPath: arvore vazia (null) devolve null", () => {
  assert.strictEqual(core.findAncestorPath(null, 1), null);
});

/* --------------------------------------------------------------------------- */

if (failures > 0) {
  console.log(`\n${failures} teste(s) falharam`);
  process.exit(1);
}
console.log("\nOK");
