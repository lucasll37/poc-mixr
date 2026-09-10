// Lógica PURA do editor gráfico de .edl (src/ui/edl_builder.jsx) -- sem React,
// sem JSX, sem `import`/`export` de verdade: é um arquivo <script> comum,
// concatenado por src/ui/compile.js na MESMA tag <script> do app transpilado
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
  "simulation", "libs",
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
// SEM fallback permissivo: um slot-lista cujo `objectTypes` está vazio não
// aceita NENHUMA classe -- nem mesmo quando `acceptsChildList` é true. Isto
// já foi diferente (ver histórico): um ON_SLOT que só declara
// 'base::PairStream', sem um segundo tipo de objeto único (Station.networks,
// IoHandler.devices, IoDevice.adapters, NetIO.inputEntityTypes/
// outputEntityTypes, ...) não tem NENHUM tipo visível na própria assinatura
// -- o dynamic_cast/isClassType() de cada item mora dentro do CORPO do
// setter, que nenhuma regex sobre o ON_SLOT alcança. A resposta a isso não é
// mais "aceitar qualquer coisa" -- é `generate_edl_catalog.py::
// LIST_SLOT_TYPE_OVERRIDES`/`TEXT_ONLY_LIST_SLOTS`, uma tabela curada lendo
// o `.cpp` de cada um dos ~35 casos deste fork (nenhum sobrou vazio,
// conferido: todo slot com `acceptsChildList` tem ou `objectTypes`
// preenchido ou `textOnly: true`). Um slot `textOnly` (ex.:
// TacviewOutput.typeMap, PluginModule.provides -- tabelas de nome/string, ou
// Navigation.feba -- vetores brutos `[ x y ]`) nunca teve classe nenhuma que
// sirva; o caminho é sempre o item de texto ("+texto"), nunca um card
// arrastado -- `isCompatible` devolve `false` para qualquer candidata ali,
// do mesmo jeito que para um slot com `objectTypes` genuinamente
// incompatível.
function isCompatible(candidateFactory, slot, byFactory) {
  const cand = byFactory[candidateFactory];
  if (!cand) return false;
  return slot.objectTypes.some((t) => cand.chain.includes(t));
}

// isCompatible() fica pura (so' tipo/heranca) -- isOfferable() e' o unico
// ponto que soma concretude (entry.concrete === true, ver
// generate_edl_catalog.py::dispatch_factory_cpp_paths()), usado so' nos
// lugares que OFERECEM uma classe ao usuario (paleta, dropdown de slot,
// validacao de drag-and-drop). Motivo: os testes existentes de
// isCompatible() usam uma fixture sem campo `concrete`; embutir a checagem
// em isCompatible() quebraria esses testes em silencio.
function isOfferable(candidateFactory, slot, byFactory) {
  const cand = byFactory[candidateFactory];
  return !!cand && cand.concrete === true && isCompatible(candidateFactory, slot, byFactory);
}

function compatibleFactories(slot, catalog, byFactory) {
  return catalog
    .filter((e) => isOfferable(e.factory, slot, byFactory))
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

/* ------------------------- papéis primários (Player) ---------------------- */

// Os ~10 "papéis primários" que Player::updateSystemPointers() resolve por
// TIPO (findByType() sobre a lista genérica components:, nunca por nome de
// slot -- Player.hpp documenta que os nomes de chave convencionais
// (dynamicsModel:, pilot:, ...) são cosméticos). O catálogo
// (src/ui/scripts/generate_edl_catalog.py) já entrega isso pronto e
// herdado por cadeia, em entry.primaryComponents -- aqui só se explicita o
// contrato e se blinda contra uma entrada sem o campo (ex.: um projeto
// salvo/catálogo gerado antes desta feature existir).
function primaryRolesFor(factory, byFactory) {
  const entry = byFactory[factory];
  return (entry && entry.primaryComponents) || [];
}

// Entre os itens JÁ presentes em node.children.components, acha o
// PRIMEIRO cuja cadeia de herança inclui o baseClass do papel -- mesma
// semântica de findByType() (primeiro que casa vence; o MIXR não promete
// nenhuma ordem além dessa). Devolve o item {key, node} ou null.
//
// Serve só para AGRUPAR e rotular o que já está montado (RoleSection, em
// edl_builder.jsx: `dynamicsModel -> JSBSimModel` em vez de uma lista
// anônima de 12 componentes). Papel vazio NÃO vira mais nada na tela --
// ver a nota sobre a remoção das "pendências" logo abaixo.
function roleFillStatus(node, role, byFactory) {
  const items = (node.children && node.children.components) || [];
  for (const it of items) {
    if (it.node.isText) continue;
    const cand = byFactory[it.node.factory];
    if (cand && cand.chain.includes(role.baseClass)) return it;
  }
  return null;
}

/* ---------------------------- itens em aberto ----------------------------- */

// NÃO EXISTE MAIS "PENDÊNCIA" -- e a razão está medida, não é gosto.
//
// Havia aqui `emptyChildSlots()`/`nodePendencies()`/`collectPendencies()`,
// que chamavam de "pendência" todo slot-filho declarado pelo catálogo e
// deixado vazio, mais todo papel primário de Player sem preenchimento. Em
// EDL praticamente TODO slot é opcional com default sensato, então essa
// conta media o TAMANHO da árvore, não o que falta nela: 145 para as 90
// entradas de src/poc/dis/flight, 321 para as 153 de built-in_mixr_1 e
// 2280 para as 771 de sandbox/A4-6DOF -- os três cenários corretos e
// rodando (razão ~1,6 / 2,1 / 3,0 por nó). Nos 2280: 518 eram o slot
// genérico `components` vazio (o normal em toda classe derivada de
// Component), 720 eram os 10 papéis cobrados de 72 TEMPLATES de armamento
// dentro de StoresMgr.stores (uma bomba no cabide não tem piloto nem
// rádio; os 8 ( Aircraft ) de verdade tinham ZERO), 336 eram o par
// mutuamente exclusivo não usado (o cenário posiciona por xPos/yPos, então
// latitude/longitude eram cobradas em cima) e 58 eram falso positivo puro
// -- slot COM valor reportado como vazio, porque a regra só olhava
// node.children e nunca node.slotValues (ex.: WorldModel.latitude
// = -22.25, Tws.threshold = ( Decibel 0.0 )).
//
// Apertar o predicado não salvava: aplicando TODAS as correções mecânicas
// possíveis o A4-6DOF ainda dava 558, e os 558 restantes eram slots
// opcionais legítimos. O MIXR não tem conceito de slot obrigatório
// (BEGIN_SLOT_MAP/ON_SLOT só registram setters com default), então
// "obrigatório" não é derivável do fonte -- a régua estava errada na raiz.
//
// O que sobra é este coletor: só o que o AUTOR ainda tem de resolver antes
// de exportar. Os alvos de arraste da árvore (o PlaceholderCard de um slot
// vazio, em ChildSlotSection) continuam intactos -- aquilo é a mecânica de
// edição, nunca foi contagem.

// Placeholder de template (`@NUM_TC_THREADS@`, `@RUN_ID@`). Mora AQUI, e
// não em edl_parser_core.js, porque os dois lados precisam da mesma regra
// e o `require` só existe na direção parser -> core (nunca o contrário, e
// compile.js concatena core ANTES do parser no navegador). Fonte única.
const TOKEN_PLACEHOLDER_RE = /@([A-Za-z_][A-Za-z0-9_]*)@/g;

// Toda ocorrência de @TOKEN@ que ainda está literal num valor de folha da
// árvore. Varre TODOS os tipos de folha -- inclusive `unit`, `number` e
// `vector`, que a varredura antiga (só `raw`/`text`) deixava passar: um
// `( Meters @ALT@ )` era mudo. Também desce nas folhas de texto de lista.
//
// `label` segue a MESMA convenção de extractPlacements(): a chave que o
// PAI deu ao item (`falcon1: ( Aircraft ... )`), não um valor de slot.
function leafTextOf(sv) {
  if (!sv || typeof sv !== "object") return "";
  if (sv.kind === "raw") return typeof sv.raw === "string" ? sv.raw : "";
  return typeof sv.value === "string" ? sv.value : "";
}

function collectOpenIssues(root) {
  const out = [];
  function scan(text, node, label, slotName) {
    if (typeof text !== "string" || text.indexOf("@") === -1) return;
    TOKEN_PLACEHOLDER_RE.lastIndex = 0;
    let m;
    while ((m = TOKEN_PLACEHOLDER_RE.exec(text))) {
      out.push({
        nodeId: node.id,
        factory: node.factory,
        label: label || node.factory,
        slotName,
        token: m[1],
      });
    }
  }
  function walk(node, label) {
    if (!node || node.isText) return;
    const sv = node.slotValues || {};
    Object.keys(sv).forEach((slotName) => scan(leafTextOf(sv[slotName]), node, label, slotName));
    const children = node.children || {};
    Object.keys(children).forEach((slotName) => {
      children[slotName].forEach((item) => {
        if (item.node && item.node.isText) scan(item.node.text, node, label, slotName);
        else walk(item.node, item.key);
      });
    });
  }
  walk(root, null);
  return out;
}

// Caminho da RAIZ até um nó (ids, inclusive das duas pontas) -- usado pra
// "pular" de um item em aberto (aba Abertos) ou de um aviso de carga
// (LoadWarningsBanner) até o cartão dele na árvore:
// expandir todo id deste caminho é o que garante que o cartão-alvo fique
// VISÍVEL (um ancestral colapsado esconderia ele), sem precisar expandir a
// árvore inteira. Devolve null se o id não existe na árvore atual (pode
// acontecer se a árvore mudou entre o cálculo da lista e o clique).
function findAncestorPath(root, targetId) {
  if (!root) return null;
  function walk(node, path) {
    if (node.isText) return null;
    const nextPath = [...path, node.id];
    if (node.id === targetId) return nextPath;
    const children = node.children || {};
    for (const items of Object.values(children)) {
      for (const it of items) {
        const found = walk(it.node, nextPath);
        if (found) return found;
      }
    }
    return null;
  }
  return walk(root, []);
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

// Mesma forma de numero (decimal simples/'.5'/'5.'/notacao cientifica) que
// NUM_RE reconhece em edl_parser_core.js -- sem a alternativa hexadecimal,
// que nao faz sentido nem pra um valor de texto solto nem pro vetor
// Table2/Table3.data abaixo. Duplicada (nao referenciada) porque este
// arquivo carrega ANTES de edl_parser_core.js no bundle concatenado (ver
// compile.js) e roda sozinho em teste. ACHADO POR AUDITORIA (nao
// redescobrir): a forma antiga, so' '\d+(\.\d+)?', nao reconhecia notacao
// cientifica nem '.5'/'5.' -- um valor assim caia no fallback de
// JSON.stringify() e virava base::String na reexportacao, silenciosamente
// trocando o tipo do lado MIXR (Number esperado, String entregue).
const RAW_NUMBER_SRC = "[+-]?(?:\\d*\\.\\d+(?:[eE][+-]?\\d+)?|\\d+\\.\\d*(?:[eE][+-]?\\d+)?|\\d+(?:[eE][+-]?\\d+)?)";

// Um valor puramente numerico tokeniza como NUMERO no scanner, não como
// IDENT -- emiti-lo "nu" produziria base::Integer/Float, não
// base::Identifier/String. Força aspas nesse caso.
const LOOKS_LIKE_NUMBER_RE = new RegExp(`^${RAW_NUMBER_SRC}$`);

// Um item de lista pode ser um VETOR NUMERICO cru em vez de um
// texto/identificador -- caso do Table2/Table3.data, cujo 'data:' e' uma
// lista de SUBLISTAS numericas ('{ [ 1 2 3 ] [ 4 5 6 ] }', ver o comentario
// de astValueToChildNode() em edl_to_ui_project.js). Emitir isso
// como string entre aspas quebraria Table2::loadData() (espera um
// base::List de verdade, nao uma base::String); reconhecer a FORMA
// '[ numeros ]' e deixar passar cru resolve sem precisar de um tipo de nó
// novo no modelo de dados.
const RAW_VECTOR_RE = new RegExp(`^\\[\\s*${RAW_NUMBER_SRC}(\\s+${RAW_NUMBER_SRC})*\\s*\\]$`);

// Bare IDENT vira base::Identifier; "quoted" vira base::String. Identifier
// É-UM String (DECLARE_SUBCLASS(Identifier, String)), então um IDENT nu
// satisfaz um ON_SLOT que peça String OU Identifier -- o oposto (string
// entre aspas) NÃO satisfaz um slot que só aceite Identifier. Por isso a
// forma nua é a escolha padrão sempre que o texto permitir.
function serializeTextLiteral(value) {
  const v = String(value);
  if (RAW_VECTOR_RE.test(v.trim())) return v.trim();
  if (v && BARE_IDENT_RE.test(v) && !LOOKS_LIKE_NUMBER_RE.test(v)) return v;
  // ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): o tokenizer real
  // (edl_scanner.l, replicado em tokenizeEdlText()/edl_parser_core.js deste
  // projeto -- ver o teste "tokenize: string entre aspas NAO interpreta
  // escape") NUNCA interpreta \X como escape -- \ + qualquer caractere e
  // copiado LITERAL, byte a byte, na volta. JSON.stringify() sozinho ESCAPA
  // barra invertida (\ -> \\), e como o tokenizer nunca desfaz isso na
  // leitura, um ciclo carregar+exportar DOBRAVA a contagem de barras de um
  // valor com \ literal (ex.: um caminho estilo Windows,
  // "C:\data\mission.acmi" virava "C:\\data\\mission.acmi" so de reexportar
  // SEM editar nada -- confirmado reproduzindo, nao-idempotente: 1->3->7->15
  // barras em ciclos repetidos). Corrigido desfazendo SO esse escapamento
  // especifico (a sequencia de DUAS barras que o JSON produz para CADA barra
  // original vira UMA de volta) -- o resto do que JSON.stringify faz (aspa
  // embutida, caracteres de controle) continua intacto, porque nao era o que
  // estava quebrado.
  return JSON.stringify(v).replace(/\\\\/g, "\\");
}

// Um valor de texto/vetor "vazio" (string vazia, ou só espaço) NÃO conta
// como preenchido -- limpar um campo de volta pro vazio é a forma do
// usuário dizer "na verdade, sem valor", não "valor = string vazia
// deliberada". Sem isto, limpar um campo deixava o slot ainda PRESENTE no
// .edl exportado (serializeLeafValue emitia '""'/'[  ]') e no contador
// "N/M preenchidos" -- confirmado como bug: editar e depois apagar um
// atributo não removia o item, só trocava o valor por vazio. Não se aplica
// a number/boolean/unit -- 0/false/"sem unidade" são valores LEGÍTIMOS,
// não "vazios".
function isEmptyLeafValue(sv) {
  if (!sv) return true;
  if (sv.kind === "text" || sv.kind === "vector") return String(sv.value).trim() === "";
  // "raw" e' o valor de um slot que o CATALOGO NAO CONHECE (fabrica ou slot
  // sem slotDef resolvido -- ver scalarToSlotValue() em edl_parser_core.js):
  // o texto EXATO da fonte, nunca reinterpretado. Mesma regra de "limpar de
  // volta pra vazio remove o slot" que texto/vetor ja tem.
  if (sv.kind === "raw") return String(sv.raw).trim() === "";
  return false;
}

// 'slotDef' pode ser null/undefined aqui -- acontece quando o CHAMADOR (o
// laco extra de serializeNode(), mais abaixo) esta emitindo um slot que o
// catalogo nao conhece: nesse caso o valor so' pode ser kind "raw" (nunca
// "text"/"number"/etc, ver scalarToSlotValue()), que nao consulta slotDef
// nenhum -- o teste `slotDef &&` so existe pra o branch de "text" nao
// quebrar se algum dia for chamado assim tambem.
function serializeLeafValue(slotDef, sv) {
  if (isEmptyLeafValue(sv)) return null;
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
  // Valor bruto (slot/fabrica nao catalogado) -- o texto exato da fonte,
  // sem reinterpretar aspas/tipo. Eliminar a ambiguidade bare-vs-quoted-
  // vs-numero na CARGA (edl_parser_core.js) e' o que evita corromper um
  // valor como um "42" nu virando string entre aspas na reexportacao.
  if (sv.kind === "raw") return String(sv.raw);
  if (sv.kind === "text") {
    if (slotDef && slotDef.acceptsNumber && LOOKS_LIKE_NUMBER_RE.test(String(sv.value).trim())) {
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

  const knownNames = new Set();
  for (const slotDef of entry ? orderedSlotsForSerialization(entry.slots) : []) {
    knownNames.add(slotDef.name);
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

  // Slots que o CATALOGO NAO CONHECE -- fabrica inteiramente desconhecida
  // (entry undefined, knownNames vazio) ou uma classe conhecida com um
  // slot a mais que o catalogo nao lista (typo, classe em desenvolvimento,
  // plugin de terceiro nunca introspectado). Sem este laco, esse conteudo
  // -- PRESERVADO na arvore pelo carregador de .edl (edl_parser_core.js) --
  // seria descartado aqui em silencio, o oposto do que "carregar um .edl
  // real e mapear tudo" promete. node.unknownSlotForms lembra se a FONTE
  // usou a forma nua ('slot: (Classe)') ou de lista ('slot: {chave:
  // (Classe)}') para cada slot desses -- pra um slot conhecido essa decisao
  // sai de slotDef.acceptsChildList (acima); pra um desconhecido nao ha
  // essa informacao no catalogo, entao ela e' lembrada de onde veio, nunca
  // adivinhada pela contagem de itens (as duas formas NAO sao equivalentes
  // no lado C++ -- PairStream vs. objeto direto).
  const extraNames = new Set([
    ...Object.keys(node.slotValues || {}),
    ...Object.keys(node.children || {}),
  ]);
  extraNames.forEach((name) => {
    if (knownNames.has(name)) return;
    const kids = node.children[name];
    if (kids && kids.length) {
      const remembered = node.unknownSlotForms && node.unknownSlotForms[name];
      const asList = remembered === "list" ? true : remembered === "single" ? false : kids.length > 1;
      if (asList) {
        lines.push(`${pad1}${name}: {`);
        kids.forEach((it) => {
          const value = it.node.isText
            ? serializeTextLiteral(it.node.text)
            : serializeNode(it.node, 0, byFactory).trim();
          lines.push(`${pad1}   ${it.key}: ${value}`);
        });
        lines.push(`${pad1}}`);
      } else {
        const only = kids[0].node;
        const value = only.isText ? serializeTextLiteral(only.text) : serializeNode(only, 0, byFactory).trim();
        lines.push(`${pad1}${name}: ${value}`);
      }
    } else if (node.slotValues[name] !== undefined) {
      const text = serializeLeafValue(null, node.slotValues[name]);
      if (text !== null) lines.push(`${pad1}${name}: ${text}`);
    }
  });

  lines.push(`${pad}) // ${node.factory}`);
  return lines.join("\n");
}

// Conta, na arvore inteira, quantos NOS tem fabrica nao catalogada ou pelo
// menos um slot nao catalogado (um por no, nao um por slot -- um Aircraft
// com tres slots desconhecidos conta 1, nao 3). Alimenta o badge
// persistente "N nao catalogados" da barra de abas (edl_builder.jsx), que
// continua visivel mesmo depois da faixa de avisos do carregamento ser
// dispensada.
function countUncataloged(root, byFactory) {
  let count = 0;
  function walk(node) {
    if (!node || node.isText) return;
    const entry = byFactory[node.factory];
    if (!entry) {
      count++;
    } else {
      const known = new Set(entry.slots.map((s) => s.name));
      const names = [...Object.keys(node.slotValues || {}), ...Object.keys(node.children || {})];
      if (names.some((n) => !known.has(n))) count++;
    }
    const children = node.children || {};
    Object.keys(children).forEach((slotName) => {
      children[slotName].forEach((item) => walk(item.node));
    });
  }
  walk(root);
  return count;
}

function projectToEdl(root, byFactory) {
  if (!root) return "";
  return serializeNode(root, 0, byFactory) + "\n";
}

/* ------------------------- destaque de sintaxe .edl ------------------------ */
// Porta, em JS puro (sem TextMate, sem VSCode), as MESMAS regras de
// .vscode/extensions/edl/syntaxes/edl.tmLanguage.json -- usado só na
// pré-visualização (ExportPanel), pra ficar consistente com o realce que
// quem edita os cenários já vê no editor de texto. Não é o parser real
// (nem o TextMate é) -- é so' classificação léxica por regex, na MESMA
// ordem de precedência da gramática original (comentário/string primeiro,
// depois "identificador logo após '('" = nome de classe, depois
// "identificador logo antes de ':'" = nome de slot, depois booleano/
// numero/pontuação, e por último QUALQUER outro identificador = valor
// solto). O charset de identificador ([-a-zA-Z0-9~!@#$%^&*_+=<>?/]) é
// copiado literal do wordPattern/grammar de lá (edl_scanner.l:48 por
// baixo) -- não é um palpite.
//
// Só cobre a forma que ESTA ferramenta de fato produz: a gramática real
// tem uma variante "nome de classe na linha seguinte ao '('" (para EDL
// escrito à mão com o '(' sozinho no fim da linha) que serializeNode()
// nunca gera (sempre escreve "( Fabrica" na mesma linha) -- omitida de
// propósito, não por descuido.
const EDL_IDENT_CHARS = "-a-zA-Z0-9~!@#$%^&*_+=<>?/";
const EDL_TOKEN_RE = new RegExp(
  [
    "(?<comment>//[^\\n]*)",
    '(?<dqstring>"(?:\\\\.|[^"\\\\])*")',
    "(?<ltstring><(?=[^<>\\n]*>)(?:\\\\.|[^>\\\\])*>)",
    "(?<formopen>\\()(?<formws>[ ,\\t\\v\\f]*)(?<classname>[" + EDL_IDENT_CHARS + "]+)",
    "(?<slotname>[" + EDL_IDENT_CHARS + "]+)(?<slotcolon>:)",
    "(?<bool>true|TRUE|false|FALSE)(?![" + EDL_IDENT_CHARS + "])",
    "(?<hexnum>0[xX][a-fA-F0-9]+[uUlL]*)(?![" + EDL_IDENT_CHARS + "])",
    "(?<floatnum>[+-]?(?:\\d*\\.\\d+(?:[Ee][+-]?\\d+)?|\\d+\\.\\d*(?:[Ee][+-]?\\d+)?|\\d+[Ee][+-]?\\d+)[fFlL]?)(?![" + EDL_IDENT_CHARS + "])",
    "(?<octalnum>0\\d+[uUlL]*)(?![" + EDL_IDENT_CHARS + "])",
    "(?<intnum>[+-]?\\d+[uUlL]*)(?![" + EDL_IDENT_CHARS + "])",
    "(?<punctclose>\\))",
    "(?<punctblock>[{}])",
    "(?<punctlist>[\\[\\]])",
    "(?<ident>[" + EDL_IDENT_CHARS + "]+)",
  ].join("|"),
  "g"
);

// Devolve [{text, cls}], cls===null pra trechos sem estilo (espaço em
// branco, pontuação não reconhecida, etc.) -- concatenar todo `text`, NA
// ORDEM, reproduz o texto de entrada byte a byte (testado explicitamente:
// esse round-trip é o que garante que o destaque nunca CORROMPE a
// pré-visualização, só colore por cima).
function tokenizeEdlText(text) {
  const tokens = [];
  let last = 0;
  EDL_TOKEN_RE.lastIndex = 0;
  let m;
  while ((m = EDL_TOKEN_RE.exec(text))) {
    if (m.index > last) tokens.push({ text: text.slice(last, m.index), cls: null });
    const g = m.groups;
    if (g.comment !== undefined) tokens.push({ text: g.comment, cls: "tok-comment" });
    else if (g.dqstring !== undefined) tokens.push({ text: g.dqstring, cls: "tok-string" });
    else if (g.ltstring !== undefined) tokens.push({ text: g.ltstring, cls: "tok-string" });
    else if (g.classname !== undefined) {
      tokens.push({ text: g.formopen, cls: "tok-punct" });
      if (g.formws) tokens.push({ text: g.formws, cls: null });
      tokens.push({ text: g.classname, cls: "tok-class" });
    } else if (g.slotname !== undefined) {
      tokens.push({ text: g.slotname, cls: "tok-slot" });
      tokens.push({ text: g.slotcolon, cls: "tok-punct" });
    } else if (g.bool !== undefined) tokens.push({ text: g.bool, cls: "tok-bool" });
    else if (g.hexnum !== undefined) tokens.push({ text: g.hexnum, cls: "tok-num" });
    else if (g.floatnum !== undefined) tokens.push({ text: g.floatnum, cls: "tok-num" });
    else if (g.octalnum !== undefined) tokens.push({ text: g.octalnum, cls: "tok-num" });
    else if (g.intnum !== undefined) tokens.push({ text: g.intnum, cls: "tok-num" });
    else if (g.punctclose !== undefined) tokens.push({ text: g.punctclose, cls: "tok-punct" });
    else if (g.punctblock !== undefined) tokens.push({ text: g.punctblock, cls: "tok-punct" });
    else if (g.punctlist !== undefined) tokens.push({ text: g.punctlist, cls: "tok-punct" });
    else if (g.ident !== undefined) tokens.push({ text: g.ident, cls: "tok-value" });
    last = EDL_TOKEN_RE.lastIndex;
  }
  if (last < text.length) tokens.push({ text: text.slice(last), cls: null });
  return tokens;
}

/* ------------------------- mapa: posições georreferenciadas ------------- */

// Fatores de conversão pra METROS -- as 9 opções que a família de unidade
// "Distance" do catálogo declara para initXPos/initYPos/initAlt (ver
// generate_edl_catalog.py, slot de Player). Espelha as constantes
// base::distance::*2M do próprio MIXR (contexts/src/mixr/include/mixr/
// base/units/distance_utils.hpp) -- só os fatores numéricos, sem linkar o
// header nem duplicar a lib inteira aqui.
const DISTANCE_TO_METERS = {
  CentiMeters: 0.01, Feet: 0.3048, Inches: 0.0254, KiloMeters: 1000,
  Meters: 1, MicroMeters: 1e-6, Microns: 1e-6, NauticalMiles: 1852,
  StatuteMiles: 1609.344,
};

// Lê um slot-valor (o que o LeafWidget de um slot de família "Distance" de
// fato produz: kind "unit" com unit escolhida, "unit" com unit "" -- número
// cru --, ou undefined -- slot nunca tocado) e devolve METROS, ou undefined
// se vazio/inválido. Número cru (sem unidade escolhida) JÁ é em metros --
// convenção do próprio MIXR: o comentário do slot em Player.hpp diz
// "initXPos <base::Number> ! X position (+north) (meters)".
function leafValueToMeters(sv) {
  if (!sv) return undefined;
  const n = Number(sv.value);
  if (!Number.isFinite(n)) return undefined;
  if (sv.kind === "unit" && sv.unit) {
    const factor = DISTANCE_TO_METERS[sv.unit];
    return factor === undefined ? undefined : n * factor;
  }
  return n;
}

// Percorre a árvore inteira e devolve um item por PLAYER -- qualquer nó cuja
// cadeia de herança inclua "Player" (Aircraft, GuidedMissile, SpaceVehicle,
// etc.: o MESMO critério que já distingue "vai na seção de sistemas
// principais" de "é o próprio avião", ver roleFillStatus). Posição LOCAL
// (norte/leste/altitude), em METROS, relativa ao ponto de referência da
// Station -- a mesma convenção que o mapa do dashboard C++ já usa (ver
// app/MapPanel.cpp, CLAUDE.md): não é latitude/longitude real -- nenhum
// cenário de produção deste repositório declara initLatitude/
// initLongitude, só initXPos/initYPos/initAlt --, mas já basta pra dar uma
// noção do layout do cenário, que é o que se pediu. initXPos/initYPos/
// initAlt ausentes contam como 0 (o próprio default do slot no MIXR): um
// player ainda sem posição definida aparece na origem, visível, em vez de
// sumir do mapa.
//
// `label`: Player não tem slot "name" (só "id", numérico -- confirmado no
// catálogo real) -- quem identifica um player de verdade, na convenção
// deste repositório, é a CHAVE do PairStream (`falcon1: ( Aircraft ... )`,
// ver qualquer .edl de produção no CLAUDE.md), não um valor de slot. Por
// isso o rótulo vem da `key` que o PAI deu a este item (a mesma que já
// aparece como keyBadge na árvore), com o nome da fábrica como fallback só
// pra raiz (que não tem chave -- não é item de lista de ninguém).
function extractPlacements(root, byFactory) {
  const out = [];
  function walk(node, label) {
    if (!node || node.isText) return;
    const entry = byFactory[node.factory];
    if (entry && entry.chain && entry.chain.indexOf("Player") !== -1) {
      const sv = node.slotValues || {};
      const north = leafValueToMeters(sv.initXPos);
      const east = leafValueToMeters(sv.initYPos);
      const alt = leafValueToMeters(sv.initAlt);
      out.push({
        id: node.id,
        factory: node.factory,
        label: label || node.factory,
        origin: entry.origin,
        north: north === undefined ? 0 : north,
        east: east === undefined ? 0 : east,
        alt: alt === undefined ? 0 : alt,
        hasExplicitPosition: north !== undefined || east !== undefined,
      });
    }
    const children = node.children || {};
    Object.keys(children).forEach((slotName) => {
      children[slotName].forEach((item) => walk(item.node, item.key));
    });
  }
  walk(root, null);
  return out;
}

// UMD-lite: em Node (src/ui/edl_builder.test.js), exporta tudo via
// module.exports; no navegador (concatenado por compile.js dentro do mesmo
// <script> do app), os `function`/`const` acima ficam disponíveis como
// identificadores comuns do escopo do script -- este bloco não roda lá
// porque `module` não existe nesse contexto.
if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    buildCatalogIndex, originLabel, originRank, isCompatible, isOfferable, compatibleFactories,
    isLeafSlot, isChildSlot, defaultKindFor,
    primaryRolesFor, roleFillStatus, TOKEN_PLACEHOLDER_RE, collectOpenIssues, findAncestorPath,
    countUncataloged,
    freshId, resetIdCounter, makeNode, makeTextLeaf, findNode, updateNode, removeNode, maxId,
    isAscii, isEmptyLeafValue, serializeTextLiteral, serializeLeafValue, serializeNode, projectToEdl,
    tokenizeEdlText,
    leafValueToMeters, extractPlacements,
  };
}
