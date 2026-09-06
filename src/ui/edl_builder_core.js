// Lógica PURA do editor gráfico de .edl (src/ui/edl_builder.jsx) -- sem React,
// sem JSX, sem `import`/`export` de verdade: é um arquivo <script> comum,
// concatenado por docs/compile.js na MESMA tag <script> do app transpilado
// (compartilham escopo global, sem precisar de bundler nem de resolução de
// módulo no navegador -- ver o comentário de `coreFile` em compile.js).
//
// Separado de edl_builder.jsx só para poder ser testado em Node puro, sem
// Babel/React (src/ui/edl_builder.test.js faz `require()` direto): tudo que
// não depende de DOM/React mora aqui.

/* ------------------------- catálogo: índice e regras -------------------- */

function buildCatalogIndex(catalog) {
  const byFactory = {};
  catalog.forEach((e) => { byFactory[e.factory] = e; });
  return byFactory;
}

const ORIGIN_ORDER = [
  "models", "base", "terrain", "interop/dis", "linkage", "recorder",
  "simulation", "shared",
];
function originLabel(o) {
  if (o.startsWith("plugin:")) return `plugin: ${o.slice(7)}`;
  return o;
}
function originRank(o) {
  const i = ORIGIN_ORDER.indexOf(o);
  return i === -1 ? ORIGIN_ORDER.length + (o.startsWith("plugin:") ? 0 : 1) : i;
}

// Uma classe candidata serve num slot quando algum dos "objectTypes"
// declarados pelo slot aparece na CADEIA DE HERANÇA da candidata -- exatamente
// o mesmo teste que dynamic_cast<ObjType*>(obj) faz em runtime (ON_SLOT,
// macros.hpp:318-324): o slot aceita a classe exata OU qualquer subclasse.
//
// EXCEÇÃO deliberada: um slot-lista (acceptsChildList) cujo ON_SLOT só
// declara 'base::PairStream' -- sem um segundo tipo de objeto único, como
// Component.components tem ('PairStream' + 'Component') -- não tem NENHUM
// tipo de item visível na própria assinatura (ex.: Station.networks,
// JoystickIoHandler.devices, UsbJoystick.adapters, DisNetIO.
// inputEntityTypes/outputEntityTypes: todos com objectTypes:[] no catálogo).
// A checagem de tipo de CADA ITEM ali é feita por dentro do setter em
// runtime (ex.: Station::setSlotNetworks() percorre o PairStream e tenta
// dynamic_cast<NetIO*> item a item) -- não há como extrair isso do ON_SLOT
// por regex. Bloquear tudo aqui (como a regra geral faria, já que
// objectTypes vazio nunca bate) transformava esses slots num beco sem
// saída -- nenhuma classe do catálogo "servia". Ficar permissivo (aceitar
// qualquer candidata) é o comportamento certo: o pior caso é o usuário
// arrastar algo que não serve ali, e 'make edl-check'/'edl-lint' (o
// oráculo real) pegam isso na hora de validar -- muito melhor que um slot
// que nunca aceita nada.
function isCompatible(candidateFactory, slot, byFactory) {
  const cand = byFactory[candidateFactory];
  if (!cand) return false;
  if (slot.acceptsChildList && slot.objectTypes.length === 0) return true;
  return slot.objectTypes.some((t) => cand.chain.includes(t));
}

function compatibleFactories(slot, catalog, byFactory) {
  return catalog
    .filter((e) => isCompatible(e.factory, slot, byFactory))
    .map((e) => e.factory)
    .sort();
}

// Slots com valor de FOLHA (texto/numero/booleano/vetor/unidade) -- vao no
// painel de propriedades. Slots com filho (objeto e/ou lista) -- vao na
// árvore outline. Um slot pode ser as DUAS coisas ao mesmo tempo (ex.:
// dis::NetIO.maxTimeDR aceita um numero-com-unidade OU uma tabela de
// excecoes por PairStream) -- por isso as duas listas não são mutuamente
// exclusivas.
function isLeafSlot(s) {
  return s.acceptsNumber || s.acceptsBoolean || s.acceptsText || s.acceptsVector || s.unitFamilies.length > 0;
}
function isChildSlot(s) {
  return s.acceptsChildList || s.objectTypes.length > 0;
}

function defaultKindFor(slotDef) {
  if (slotDef.unitFamilies.length) return "unit";
  if (slotDef.acceptsVector) return "vector";
  if (slotDef.acceptsBoolean && !slotDef.acceptsNumber && !slotDef.acceptsText) return "boolean";
  if (slotDef.acceptsNumber && !slotDef.acceptsText) return "number";
  return "text";
}

/* ------------------------------ modelo de dados -------------------------- */

let nextNodeId = 1;
function freshId() { return nextNodeId++; }
function resetIdCounter(n) { nextNodeId = n; }

function makeNode(factory) {
  return { id: freshId(), factory, slotValues: {}, children: {} };
}

// Um item de lista NEM SEMPRE é um objeto aninhado -- slots como
// TacviewOutput.modelMap/typeMap/colorMap são PairStream de VALOR ESCALAR
// (cada entrada é um base::String/Identifier, não uma classe MIXR: ver
// TacviewOutput::setSlotTypeMap(), que faz dynamic_cast<base::String*> em
// cada item, não dynamic_cast<Object*>). 'children: {}' aqui é só pra
// findNode()/updateNode()/removeNode()/maxId() (genéricos, percorrem
// node.children de QUALQUER nó) não precisarem de um caso especial.
function makeTextLeaf(text) {
  return { id: freshId(), isText: true, text: text || "", children: {} };
}

// Acha um nó pelo id, em qualquer profundidade (percorre TODOS os slots de
// filho de todo nó). Devolve null se não achar.
function findNode(root, id) {
  if (!root) return null;
  if (root.id === id) return root;
  for (const items of Object.values(root.children)) {
    for (const it of items) {
      const found = findNode(it.node, id);
      if (found) return found;
    }
  }
  return null;
}

// Clona a árvore trocando SÓ o nó de id `id` pelo resultado de `updater(nó)`
// -- o resto é reaproveitado (imutabilidade estrutural, sem deep-clone
// desnecessário).
function updateNode(root, id, updater) {
  if (!root) return root;
  if (root.id === id) return updater(root);
  let changed = false;
  const children = {};
  for (const [slotName, items] of Object.entries(root.children)) {
    const newItems = items.map((it) => {
      const newChild = updateNode(it.node, id, updater);
      if (newChild !== it.node) changed = true;
      return newChild === it.node ? it : { ...it, node: newChild };
    });
    children[slotName] = newItems;
  }
  return changed ? { ...root, children } : root;
}

function removeNode(root, id) {
  if (!root || root.id === id) return null;
  const children = {};
  for (const [slotName, items] of Object.entries(root.children)) {
    children[slotName] = items
      .filter((it) => it.node.id !== id)
      .map((it) => ({ ...it, node: removeNode(it.node, id) }));
  }
  return { ...root, children };
}

function maxId(node, acc) {
  acc = acc || 0;
  if (!node) return acc;
  acc = Math.max(acc, node.id || 0);
  for (const items of Object.values(node.children || {})) {
    for (const it of items) acc = Math.max(acc, maxId(it.node, acc));
  }
  return acc;
}

/* ------------------------------- serialização ---------------------------- */

// O { arquivo } inteiro tem de ser ASCII puro -- um unico acento em QUALQUER
// LUGAR (mesmo dentro de um comentario, sem relacao com o valor) faz o
// bison/flex deste fork rejeitar o ARQUIVO INTEIRO com "syntax error", sem
// dizer o motivo (CLAUDE.md, confirmado rodando). Por isso a exportacao
// BLOQUEIA em vez de deixar passar.
function isAscii(s) {
  for (let i = 0; i < s.length; i++) if (s.charCodeAt(i) > 126) return false;
  return true;
}

// Charset de identificador nu do scanner real (edl_scanner.l:48):
// [a-zA-Z0-9~!@#$%^&*\-_+=<>?/]+ -- sem espaco, sem dois-pontos, sem aspas.
const BARE_IDENT_RE = /^[a-zA-Z0-9~!@#$%^&*\-_+=<>?/]+$/;
// Um valor puramente numerico tokeniza como NUMERO no scanner, não como
// IDENT -- emiti-lo "nu" produziria base::Integer/Float, não
// base::Identifier/String. Força aspas nesse caso.
const LOOKS_LIKE_NUMBER_RE = /^[+-]?\d+(\.\d+)?$/;

// Um item de lista pode ser um VETOR NUMERICO cru em vez de um
// texto/identificador -- caso do Table2/Table3.data, cujo 'data:' e' uma
// lista de SUBLISTAS numericas ('{ [ 1 2 3 ] [ 4 5 6 ] }', ver o comentario
// de astValueToChildNode() em scripts/edl_to_ui_project.js). Emitir isso
// como string entre aspas quebraria Table2::loadData() (espera um
// base::List de verdade, nao uma base::String); reconhecer a FORMA
// '[ numeros ]' e deixar passar cru resolve sem precisar de um tipo de nó
// novo no modelo de dados.
const RAW_VECTOR_RE = /^\[\s*[+-]?\d+(\.\d+)?(\s+[+-]?\d+(\.\d+)?)*\s*\]$/;

// Bare IDENT vira base::Identifier; "quoted" vira base::String. Identifier
// É-UM String (DECLARE_SUBCLASS(Identifier, String)), então um IDENT nu
// satisfaz um ON_SLOT que peça String OU Identifier -- o oposto (string
// entre aspas) NÃO satisfaz um slot que só aceite Identifier. Por isso a
// forma nua é a escolha padrão sempre que o texto permitir.
function serializeTextLiteral(value) {
  const v = String(value);
  if (RAW_VECTOR_RE.test(v.trim())) return v.trim();
  if (v && BARE_IDENT_RE.test(v) && !LOOKS_LIKE_NUMBER_RE.test(v)) return v;
  return JSON.stringify(v);
}

function serializeLeafValue(slotDef, sv) {
  if (!sv) return null;
  if (sv.kind === "boolean") return sv.value ? "true" : "false";
  if (sv.kind === "number") return String(sv.value);
  if (sv.kind === "vector") {
    const nums = String(sv.value).trim().split(/\s+/).filter(Boolean);
    return `[ ${nums.join(" ")} ]`;
  }
  if (sv.kind === "unit") {
    if (sv.unit) return `( ${sv.unit} ${sv.value} )`;
    return String(sv.value);
  }
  if (sv.kind === "text") {
    if (slotDef.acceptsNumber && LOOKS_LIKE_NUMBER_RE.test(String(sv.value).trim())) {
      return String(sv.value).trim();
    }
    return serializeTextLiteral(sv.value);
  }
  return null;
}

// A ordem de emissao normalmente NAO importa (o setter de cada slot e'
// independente) -- mas 'data' em Table1/Table2/Table3/Table4 e' EXCECAO
// confirmada rodando: Table::loadData() valida o tamanho da tabela contra
// os vetores de breakpoint ('x'/'y'/...) JA' atribuidos, entao 'data' tem
// que ser emitido DEPOIS deles. O catalogo ordena por heranca (a base
// Table declara 'data' antes de Table1 declarar 'x'), o oposto do que o
// setter exige -- por isso 'data' e' empurrado pro fim da lista aqui, so'
// para SERIALIZACAO (a ordem do catalogo em si, usada pelo painel de
// propriedades, fica como esta -- nao afeta a UI).
function orderedSlotsForSerialization(slots) {
  const withoutData = slots.filter((s) => s.name !== "data");
  const dataSlot = slots.find((s) => s.name === "data");
  return dataSlot ? [...withoutData, dataSlot] : slots;
}

function serializeNode(node, indent, byFactory) {
  const pad = "   ".repeat(indent);
  const pad1 = "   ".repeat(indent + 1);
  const entry = byFactory[node.factory];
  const lines = [];
  lines.push(`${pad}( ${node.factory}`);

  for (const slotDef of entry ? orderedSlotsForSerialization(entry.slots) : []) {
    const sv = node.slotValues[slotDef.name];
    const kids = node.children[slotDef.name];
    if (kids && kids.length) {
      // Lista sempre que o catalogo diz que o slot aceita (acceptsChildList)
      // OU quando ha mais de um item de qualquer jeito -- um '( A ) ( B )'
      // nu nem e' sintaxe EDL valida pra dois itens; precisa de chaves. A
      // classificacao estatica do catalogo pode nao ver isso (mesma razao
      // do fallback permissivo de isCompatible(): o setter real aceita
      // PairStream item-a-item, invisivel na assinatura do ON_SLOT.
      if (slotDef.acceptsChildList || kids.length > 1) {
        lines.push(`${pad1}${slotDef.name}: {`);
        kids.forEach((it) => {
          const value = it.node.isText
            ? serializeTextLiteral(it.node.text)
            : serializeNode(it.node, 0, byFactory).trim();
          lines.push(`${pad1}   ${it.key}: ${value}`);
        });
        lines.push(`${pad1}}`);
      } else {
        // slot de objeto UNICO (sem PairStream aceito) -- forma NUA, sem
        // chaves: uma lista aqui faria dynamic_cast<PairStream*> falhar no
        // consumidor (RfSensor::setSlotModeStream vs setSlotModeSingle,
        // Component::setSlotComponent(PairStream) vs (Component) --
        // confirmado no fonte real).
        const only = kids[0].node;
        const value = only.isText ? serializeTextLiteral(only.text) : serializeNode(only, 0, byFactory).trim();
        lines.push(`${pad1}${slotDef.name}: ${value}`);
      }
    } else if (sv !== undefined) {
      const text = serializeLeafValue(slotDef, sv);
      if (text !== null) lines.push(`${pad1}${slotDef.name}: ${text}`);
    }
  }

  lines.push(`${pad}) // ${node.factory}`);
  return lines.join("\n");
}

function projectToEdl(root, byFactory) {
  if (!root) return "";
  return serializeNode(root, 0, byFactory) + "\n";
}

// UMD-lite: em Node (src/ui/edl_builder.test.js), exporta tudo via
// module.exports; no navegador (concatenado por compile.js dentro do mesmo
// <script> do app), os `function`/`const` acima ficam disponíveis como
// identificadores comuns do escopo do script -- este bloco não roda lá
// porque `module` não existe nesse contexto.
if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    buildCatalogIndex, originLabel, originRank, isCompatible, compatibleFactories,
    isLeafSlot, isChildSlot, defaultKindFor,
    freshId, resetIdCounter, makeNode, makeTextLeaf, findNode, updateNode, removeNode, maxId,
    isAscii, serializeTextLiteral, serializeLeafValue, serializeNode, projectToEdl,
  };
}
