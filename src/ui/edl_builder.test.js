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
    slots: [
      { name: "type", declaredIn: "Aircraft", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "initAlt", declaredIn: "Aircraft", comment: "", acceptsNumber: true, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: false, unitFamilies: [{ family: "Distance", options: ["Meters", "Feet"] }], objectTypes: [], isReference: false },
      { name: "dynamicsModel", declaredIn: "Player", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: ["DynamicsModel"], isReference: false },
      { name: "select", declaredIn: "Player", comment: "", acceptsNumber: true, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "ranges", declaredIn: "Player", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: true, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
      { name: "modes", declaredIn: "Player", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: true, unitFamilies: [], objectTypes: ["Aircraft"], isReference: false },
      { name: "trackManagerName", declaredIn: "Player", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: true },
    ] },
  { class: "JSBSimModel", factory: "JSBSimModel", baseClass: "DynamicsModel", chain: ["JSBSimModel", "DynamicsModel", "Object"], origin: "models",
    slots: [
      { name: "model", declaredIn: "JSBSimModel", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: true, acceptsVector: false, acceptsChildList: false, unitFamilies: [], objectTypes: [], isReference: false },
    ] },
  { class: "Autopilot", factory: "Autopilot", baseClass: "Pilot", chain: ["Autopilot", "Pilot", "Object"], origin: "models",
    slots: [
      // acceptsChildList sem NENHUM objectType declarado -- mesmo padrao real
      // de Station.networks/JoystickIoHandler.devices/UsbJoystick.adapters:
      // o tipo de cada ITEM so e checado por dentro do setter, nao aparece
      // no ON_SLOT. Simula isso pra testar o fallback permissivo.
      { name: "wildcardList", declaredIn: "Autopilot", comment: "", acceptsNumber: false, acceptsBoolean: false, acceptsText: false, acceptsVector: false, acceptsChildList: true, unitFamilies: [], objectTypes: [], isReference: false },
    ] },
  { class: "Station", factory: "Station", baseClass: "Component", chain: ["Station", "Component", "Object"], origin: "simulation", slots: [] },
  { class: "ClockStation", factory: "ClockStation", baseClass: "Station", chain: ["ClockStation", "Station", "Component", "Object"], origin: "shared", slots: [] },
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

test("isCompatible: slot-lista SEM objectTypes declarado aceita QUALQUER candidata (fallback permissivo)", () => {
  // Station.networks/JoystickIoHandler.devices/UsbJoystick.adapters no
  // catalogo real tem objectTypes:[] -- sem o fallback, NENHUMA classe
  // "serviria" ali, um beco sem saida na UI (confirmado rodando antes
  // deste fallback existir).
  const slot = BY_FACTORY.Autopilot.slots.find((s) => s.name === "wildcardList");
  assert.ok(core.isCompatible("Aircraft", slot, BY_FACTORY));
  assert.ok(core.isCompatible("JSBSimModel", slot, BY_FACTORY));
});

test("isCompatible: o fallback permissivo NAO se aplica a slot de objeto UNICO sem tipo (so a slot-lista)", () => {
  // Um slot que NAO aceita lista (acceptsChildList:false) e tambem nao
  // declara objectTypes e, por definicao, um slot so de folha (numero/
  // texto/etc) -- nunca deveria aparecer como "aceita filho" pra comecar
  // (isChildSlot() ja o excluiria), mas isCompatible() sozinho tem que
  // continuar recusando candidatas nesse caso, sem o fallback.
  const slotSemLista = { acceptsChildList: false, objectTypes: [] };
  assert.strictEqual(core.isCompatible("Aircraft", slotSemLista, BY_FACTORY), false);
});

test("isCompatible: raiz restrita a Station serve para ClockStation (subclasse) mas nao para Aircraft", () => {
  const slotRaiz = { acceptsChildList: false, objectTypes: ["Station"] };
  assert.ok(core.isCompatible("Station", slotRaiz, BY_FACTORY));
  assert.ok(core.isCompatible("ClockStation", slotRaiz, BY_FACTORY), "ClockStation deriva de Station, deveria servir");
  assert.ok(!core.isCompatible("Aircraft", slotRaiz, BY_FACTORY), "Aircraft nao deriva de Station, nao deveria servir");
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

test("serializeLeafValue: sem valor (undefined) devolve null -- slot fica de fora do .edl", () => {
  assert.strictEqual(core.serializeLeafValue({}, undefined), null);
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
// Carrega src/ui/edl_catalog.generated.json (gerado por `make edl-catalog`) e
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
  });

}

/* --------------------------------------------------------------------------- */

if (failures > 0) {
  console.log(`\n${failures} teste(s) falharam`);
  process.exit(1);
}
console.log("\nOK");
