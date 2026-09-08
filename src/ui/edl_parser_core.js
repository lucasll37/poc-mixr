// Motor de PARSE de .edl real -> arvore de projeto do editor grafico
// ({id,factory,slotValues,children}, ver src/ui/edl_builder_core.js).
//
// Fonte UNICA compartilhada entre navegador (concatenado por
// src/ui/scripts/compile.js na MESMA <script> de edl_builder_core.js, ANTES
// deste arquivo -- ver a ordem em compile.js) e Node (CLI de build-time,
// src/ui/scripts/edl_to_ui_project.js, e testes puros,
// src/ui/edl_parser_core.test.js). Antes desta separacao a logica inteira
// vivia dentro do script de build, sem uso nenhum em runtime no navegador --
// existir aqui, em vez de la, e' o que permite o botao "Carregar .edl" do
// editor grafico reusar exatamente o mesmo parser que ja gera o preset
// embutido, sem duas copias correndo o risco de divergir.
//
// NAO e' um parser fiel a gramatica bison/flex real (edl_parser.y/
// edl_scanner.l) -- e' um parser recursivo-descendente ESTRUTURAL, no
// mesmo espirito de src/ui/scripts/edl_lint.py, so que em vez de so
// validar, CONSTROI a arvore. Onde a fidelidade importa de verdade
// (numeros hex/octal, string alternativa '<...>', virgula como espaco,
// numero de linha em erro) ele acompanha a gramatica real de perto -- ver
// os comentarios de cada regra abaixo.
//
// A PECA NOVA em relacao ao conversor que existia antes (so' usado em
// build-time, contra UM cenario de referencia conhecido-bom): fabrica ou
// slot que o CATALOGO nao conhece nunca e' descartado. Um '.edl' real pode
// usar uma classe em desenvolvimento (catalogo desatualizado), um plugin de
// terceiro nunca introspectado, ou so' um erro de digitacao -- em qualquer
// caso, o conteudo e' preservado byte-fiel (kind "raw" + node.
// unknownSlotForms, ver assignSlot()/scalarToSlotValue() abaixo) em vez de
// silenciosamente sumir na proxima exportacao. E' esse comportamento que
// torna seguro "carregar um .edl real, mapear tudo, editar, reexportar".

/* ------------------------------ ponte com o modelo ------------------------- */
// Em Node, traz makeNode/makeTextLeaf/resetIdCounter/isAscii de
// edl_builder_core.js via require(). Em navegador (concatenado DEPOIS de
// edl_builder_core.js na mesma <script>, ver compile.js), essas funcoes ja
// estao soltas no escopo global do script -- 'core' aqui e' so' um objeto
// que as agrupa de novo, sem duplicar nada nem precisar de bundler/import.
const core = (typeof module !== "undefined" && module.exports)
  ? require("./edl_builder_core.js")
  : { makeNode: makeNode, makeTextLeaf: makeTextLeaf, resetIdCounter: resetIdCounter, isAscii: isAscii };

/* ------------------------------- tokenizer -------------------------------- */
// Mesmo charset de identificador nu do scanner real (edl_scanner.l:48,
// BARE_IDENT_RE em edl_builder_core.js). NUM_RE cobre, nesta ordem de
// alternativa (a PRIMEIRA que casar a partir da posicao atual vence --
// mesma disciplina do scanner real, que sempre tenta a regra de maior
// prioridade primeiro e usa o casamento mais LONGO entre NUM/BARE pra
// decidir o tipo do token, ver matchBareOrNumber() abaixo):
//   1) hex ('0x1F') -- FALTAVA no conversor antigo (so' cobria decimal);
//   2) as tres formas de float da gramatica real (".5", "5.", "5.0e-3" --
//      a mesma regra ja usada por EDL_TOKEN_RE, o destaque de sintaxe em
//      edl_builder_core.js, aqui reaproveitada por igual);
//   3) inteiro decimal simples -- cobre de quebra qualquer sequencia
//      "0" + digitos (octal na gramatica real, edl_scanner.l: '0{D}+'):
//      como este parser nunca precisa do VALOR numerico (so' precisa saber
//      "isto e' um numero" pra decidir kind:number/unit vs texto/raw), nao
//      ha necessidade de uma regra octal separada -- o texto e' preservado
//      cru de qualquer forma.
const NUM_RE = /^(?:0[xX][a-fA-F0-9]+|[+-]?(?:\d*\.\d+(?:[eE][+-]?\d+)?|\d+\.\d*(?:[eE][+-]?\d+)?|\d+(?:[eE][+-]?\d+)?))/;
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

// tokenize() ganha CONTAGEM DE LINHA por token (o conversor antigo nao
// tinha nenhuma -- um erro de sintaxe virava uma Error crua sem dizer ONDE,
// ao contrario do parser real e de edl_lint.py, que sempre apontam a
// linha).
function tokenize(text) {
  const tokens = [];
  let i = 0;
  let line = 1;
  const n = text.length;
  while (i < n) {
    const c = text[i];
    if (c === "\n") { line++; i++; continue; }
    // Espaco em branco real do scanner: '[ ,\t\v\f]' -- a VIRGULA e'
    // whitespace puro, nunca separador (edl_scanner.l). Faltava no
    // conversor antigo.
    if (c === " " || c === "\t" || c === "\r" || c === "," || c === "\v" || c === "\f") { i++; continue; }
    if (c === "/" && text[i + 1] === "/") { while (i < n && text[i] !== "\n") i++; continue; }
    if ("(){}[]:".includes(c)) { tokens.push({ t: c, line }); i++; continue; }
    if (c === '"') {
      // ACHADO POR AUDITORIA (nao redescobrir): a gramatica real
      // (edl_scanner.l:223-232) NAO interpreta escape nenhum -- o padrao
      // '\"(\\.|[^\\"])*\"' so' existe pra o lexer nao terminar a string
      // cedo demais num '\"' embutido; a acao copia o conteudo entre aspas
      // BYTE A BYTE ('utStrcpy(yylval.cvalp, slen, yytext+1)', sem tocar
      // backslash nenhum). '\X' na fonte vira DOIS caracteres no valor
      // final ('\' seguido de X), nunca um so'. A forma antiga aqui
      // ('buf += text[j+1]') descartava o backslash -- 'a\"b' virava 'a"b'
      // em vez de 'a\"b', trocando o CONTEUDO real do valor carregado.
      let j = i + 1, buf = "";
      while (j < n && text[j] !== '"') {
        if (text[j] === "\\" && j + 1 < n) { buf += text[j] + text[j + 1]; j += 2; continue; }
        if (text[j] === "\n") line++;
        buf += text[j]; j++;
      }
      const raw = text.slice(i, Math.min(j + 1, n));
      tokens.push({ t: "STR", v: buf, raw, line });
      i = j + 1;
      continue;
    }
    if (c === "<") {
      // Forma alternativa de string, '<...>' (edl_scanner.l) -- faltava
      // inteiramente no conversor antigo, apesar de o destaque de sintaxe
      // (tokenizeEdlText, edl_builder_core.js) ja reconhecer esse token ha'
      // mais tempo. Restrito a UMA LINHA de proposito, igual ao destaque:
      // o scanner real, por acidente de implementacao, tem uma classe de
      // caractere que NAO exclui '>' do corpo -- um '<' sem fechamento
      // consumiria ate' o ULTIMO '>' do arquivo inteiro. Nenhum .edl deste
      // repositorio usa esta forma hoje; a escolha segura e' a MESMA ja
      // adotada pelo destaque, nao a literal do flex.
      let j = i + 1;
      while (j < n && text[j] !== ">" && text[j] !== "\n") j++;
      if (text[j] === ">") {
        const raw = text.slice(i, j + 1);
        tokens.push({ t: "STR", v: text.slice(i + 1, j), raw, line });
        i = j + 1;
        continue;
      }
      i++; // '<' sem '>' na mesma linha -- nao e' string valida, melhor esforco
      continue;
    }
    const m = matchBareOrNumber(text, i);
    if (!m) { i++; continue; } // caractere fora de qualquer charset conhecido -- pula, melhor esforco
    tokens.push({ t: m.isNumber ? "NUM" : "ID", v: m.text, line });
    i += m.text.length;
  }
  return tokens;
}

/* ------------------------- parser (AST estrutural) ------------------------- */

function parseError(message, line) {
  const err = new Error(line != null ? `linha ${line}: ${message}` : message);
  err.line = line;
  return err;
}

function parseValue(toks, pos) {
  const tok = toks[pos.i];
  if (!tok) throw parseError("fim inesperado do arquivo", toks.length ? toks[toks.length - 1].line : undefined);
  if (tok.t === "(") return parseForm(toks, pos);
  if (tok.t === "{") return parseList(toks, pos);
  if (tok.t === "[") return parseBracket(toks, pos);
  if (tok.t === "STR") { pos.i++; return { kind: "scalar", text: tok.v, quoted: true, raw: tok.raw }; }
  if (tok.t === "ID" || tok.t === "NUM") { pos.i++; return { kind: "scalar", text: tok.v, quoted: false, isNumber: tok.t === "NUM" }; }
  throw parseError("token inesperado: " + JSON.stringify(tok.t), tok.line);
}

// Uma entrada dentro de um form ou de uma lista: 'chave: valor' OU so
// 'valor' (posicional/anonimo) -- o lookahead (ID OU NUM seguido de ':')
// decide. Chave numerica existe de verdade (ex.: steerpoints numerados
// '1: ( Steerpoint ... )'), entao NUM tambem conta, nao so ID.
function parseEntry(toks, pos) {
  const tok = toks[pos.i];
  if (tok && (tok.t === "ID" || tok.t === "NUM") && toks[pos.i + 1] && toks[pos.i + 1].t === ":") {
    const key = tok.v;
    pos.i += 2;
    return { key, value: parseValue(toks, pos) };
  }
  return { key: null, value: parseValue(toks, pos) };
}

function parseForm(toks, pos) {
  const openLine = toks[pos.i] ? toks[pos.i].line : undefined;
  pos.i++; // (
  const nameTok = toks[pos.i];
  if (!nameTok || nameTok.t !== "ID") throw parseError("esperava nome de classe logo apos '('", openLine);
  pos.i++;
  const entries = [];
  while (toks[pos.i] && toks[pos.i].t !== ")") entries.push(parseEntry(toks, pos));
  if (!toks[pos.i]) throw parseError("'(' sem fechamento ')' correspondente", openLine);
  pos.i++; // )
  return { kind: "form", name: nameTok.v, entries };
}

function parseList(toks, pos) {
  const openLine = toks[pos.i] ? toks[pos.i].line : undefined;
  pos.i++; // {
  const items = [];
  while (toks[pos.i] && toks[pos.i].t !== "}") items.push(parseEntry(toks, pos));
  if (!toks[pos.i]) throw parseError("'{' sem fechamento '}' correspondente", openLine);
  pos.i++;
  return { kind: "list", items };
}

function parseBracket(toks, pos) {
  const openLine = toks[pos.i] ? toks[pos.i].line : undefined;
  pos.i++; // [
  const values = [];
  while (toks[pos.i] && toks[pos.i].t !== "]") { values.push(toks[pos.i].v); pos.i++; }
  if (!toks[pos.i]) throw parseError("'[' sem fechamento ']' correspondente", openLine);
  pos.i++;
  return { kind: "bracket", values };
}

/* ------------------------ AST -> arvore de projeto -------------------------- */

// Marca, num node, que o slot 'slotName' -- SEM slotDef resolvido no
// catalogo -- veio da fonte como forma NUA ('slot: (Classe)') ou de LISTA
// ('slot: {chave: (Classe)}'). Isto importa porque as duas formas NAO sao
// equivalentes no lado C++ (PairStream vs. objeto direto); pra um slot
// CONHECIDO essa decisao sai de slotDef.acceptsChildList (o catalogo sabe),
// mas pra um slot desconhecido nao ha essa informacao -- por isso ela e'
// LEMBRADA de onde veio (aqui), nunca adivinhada por contagem de itens na
// hora de reexportar (serializeNode, edl_builder_core.js).
function markUnknownSlotForm(node, slotName, form) {
  if (!node.unknownSlotForms) node.unknownSlotForms = {};
  node.unknownSlotForms[slotName] = form;
}

// Sem slotDef (fabrica ou slot que o catalogo nao conhece), o valor vira
// kind "raw": o texto EXATO da fonte (a fatia crua capturada pelo
// tokenizer pra string, ou o proprio token pra um valor nu), sem tentar
// classificar numero/booleano/texto -- e' exatamente essa tentativa de
// classificar as cegas que corrompia silenciosamente um valor como um "42"
// nu (viraria texto "42", que serializeTextLiteral() forca entre aspas de
// volta -- um Identifier vira String, mudando o tipo real do valor pro
// lado MIXR). "raw" elimina a ambiguidade em vez de adivinhar de volta.
function scalarToSlotValue(value, slotDef) {
  if (!slotDef) {
    return { kind: "raw", raw: value.quoted ? value.raw : value.text };
  }
  if (!value.quoted && (value.text === "true" || value.text === "false") && slotDef.acceptsBoolean) {
    return { kind: "boolean", value: value.text === "true" };
  }
  if (value.isNumber && (slotDef.acceptsNumber || slotDef.unitFamilies.length)) {
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
    if (!slotDef) markUnknownSlotForm(node, slotName, "list");
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
    // -- confirmado no catalogo real) e', por construcao, um WRAPPER de
    // unidade quando usada como '( Classe numeroSo )' -- mesmo raciocinio
    // permissivo ja documentado em isCompatible(). Funciona mesmo quando o
    // SLOT em si e' desconhecido (unitOptions vazio) -- so depende da classe
    // WRAPPER estar catalogada.
    const wrapperEntry = byFactory[value.name];
    const looksLikeNumberWrapper = wrapperEntry && wrapperEntry.chain.includes("Number");
    if ((unitOptions.includes(value.name) || looksLikeNumberWrapper) && single) {
      node.slotValues[slotName] = { kind: "unit", value: value.entries[0].value.text, unit: value.name };
      return;
    }
    node.children[slotName] = [{ key: "1", node: astFormToNode(value, byFactory) }];
    if (!slotDef) markUnknownSlotForm(node, slotName, "single");
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

/* --------------------------- diagnosticos de carga -------------------------- */
// Cada diagnostico e' um objeto ESTRUTURADO ({severity,code,message,line?,
// nodeId?}), nao uma string solta -- permite testar por codigo estavel e
// permite a UI (edl_builder.jsx) "pular ate' o no" de um aviso, o mesmo
// mecanismo ja usado pela aba Pendencias (findAncestorPath).

const INCLUDE_RE = /@include:([^@\n]*)@/g;
const TOKEN_PLACEHOLDER_RE = /@([A-Za-z_][A-Za-z0-9_]*)@/g;

// '@include:frag@' NAO pode ser deixado sem expandir e so' virar um aviso:
// o identificador nu do scanner real INCLUI '@' e para no ':' -- '@include:'
// sozinho ja tokeniza como um SLOT_ID valido, entao nao expandir nao FALHA,
// CORROMPE a estrutura em silencio (vira um slot literal '@include:' =
// valor 'frag@'). Por isso e' sempre erro duro, nunca aviso -- detectado
// tanto na forma bem-formada quanto na malformada (sem o '@' de fechamento
// na MESMA linha, a armadilha ja documentada em CLAUDE.md).
function detectUnresolvedIncludes(text) {
  const names = [];
  INCLUDE_RE.lastIndex = 0;
  let m;
  while ((m = INCLUDE_RE.exec(text))) names.push(m[1]);
  if (names.length === 0 && text.includes("@include:")) names.push(null);
  return names;
}

function collectAsciiWarnings(text, warnings) {
  text.split("\n").forEach((line, idx) => {
    if (!core.isAscii(line)) {
      warnings.push({
        severity: "warning", code: "non-ascii", line: idx + 1,
        message: `linha ${idx + 1} tem caractere fora de ASCII -- a exportacao de verdade vai bloquear ate isso ser corrigido.`,
      });
    }
  });
}

function scanLeafTextForPlaceholders(text, nodeId, slotName, warnings, seen) {
  if (typeof text !== "string") return;
  TOKEN_PLACEHOLDER_RE.lastIndex = 0;
  let m;
  while ((m = TOKEN_PLACEHOLDER_RE.exec(text))) {
    const dedupeKey = `token:${nodeId}:${slotName}:${m[1]}`;
    if (seen.has(dedupeKey)) continue;
    seen.add(dedupeKey);
    warnings.push({
      severity: "warning", code: "token-placeholder", nodeId,
      message: `slot '${slotName}' ainda tem o placeholder '@${m[1]}@' -- edite o valor antes de exportar.`,
    });
  }
}

// Percorre a arvore JA CONSTRUIDA avisando sobre: fabrica desconhecida,
// slot desconhecido numa classe conhecida, e placeholder '@TOKEN@' ainda
// literal em algum valor de folha (o loader interativo NAO substitui
// '@TOKEN@' automaticamente -- ver o comentario de parseEdlDocument()).
function collectStructuralWarnings(root, byFactory, warnings) {
  const seen = new Set();
  function walk(node) {
    if (!node || node.isText) return;
    const entry = byFactory[node.factory];
    if (!entry) {
      warnings.push({
        severity: "warning", code: "unknown-factory", nodeId: node.id,
        message: `fabrica desconhecida '${node.factory}' -- os slots dela foram preservados como valor bruto, editaveis no painel de propriedades.`,
      });
    }
    const knownNames = new Set(entry ? entry.slots.map((s) => s.name) : []);
    Object.keys(node.slotValues || {}).forEach((name) => {
      if (entry && !knownNames.has(name)) {
        const key = `slot:${node.id}:${name}`;
        if (!seen.has(key)) {
          seen.add(key);
          warnings.push({
            severity: "warning", code: "unknown-slot", nodeId: node.id,
            message: `slot '${name}' desconhecido na classe '${node.factory}' -- preservado como valor bruto.`,
          });
        }
      }
      const sv = node.slotValues[name];
      if (sv && sv.kind === "raw") scanLeafTextForPlaceholders(sv.raw, node.id, name, warnings, seen);
      if (sv && sv.kind === "text") scanLeafTextForPlaceholders(sv.value, node.id, name, warnings, seen);
    });
    Object.keys(node.children || {}).forEach((name) => {
      if (entry && !knownNames.has(name)) {
        const key = `slot:${node.id}:${name}`;
        if (!seen.has(key)) {
          seen.add(key);
          warnings.push({
            severity: "warning", code: "unknown-slot", nodeId: node.id,
            message: `slot '${name}' desconhecido na classe '${node.factory}' -- preservado.`,
          });
        }
      }
      (node.children[name] || []).forEach((it) => {
        if (it.node.isText) scanLeafTextForPlaceholders(it.node.text, node.id, name, warnings, seen);
        else walk(it.node);
      });
    });
  }
  walk(root);
}

/* ----------------------------------- API ------------------------------------ */

// Ponto de entrada usado pelo botao "Carregar .edl" (edl_builder.jsx) e
// pela varredura de regressao (edl_parser_core.test.js). NUNCA lanca --
// erro duro sempre volta como { tree: null, errors: [...] }, pra quem
// chama decidir como mostrar (window.alert, painel, etc.) sem precisar de
// try/catch pra cada causa de falha.
//
// '@TOKEN@' (fora de '@include:') e' carregado LITERAL, de proposito --
// diferente de '@include:', um token como '@NUM_TC_THREADS@' nao tem ':' --
// e' um identificador nu GRAMATICALMENTE VALIDO (o charset do scanner
// inclui '@'), entao ja tokeniza e carrega como texto comum, sem nenhum
// codigo especial de substituicao. Preferido a adivinhar um valor (o que
// o script de build faz, substituindo por "2", aceitavel so' pra gerar um
// preset descartavel): aqui o usuario ve o placeholder exato no campo e
// digita o valor real antes de exportar, como qualquer outro campo. Um
// aviso 'token-placeholder' aponta cada ocorrencia pra nao passar
// despercebida.
function parseEdlDocument(text, byFactory) {
  const warnings = [];
  const errors = [];

  const includes = detectUnresolvedIncludes(text);
  if (includes.length) {
    includes.forEach((name) => {
      errors.push({
        severity: "error", code: "include-unresolved",
        message: name
          ? `'@include:${name}@' nao e suportado por "Carregar .edl" (fragmentos externos) -- substitua a diretiva pelo conteudo do fragmento diretamente no arquivo antes de carregar (nenhum .edl deste repositorio usa isso hoje).`
          : `'@include:' encontrado mas sem o '@' de fechamento na MESMA linha -- diretiva malformada, nao pode ser resolvida.`,
      });
    });
    return { tree: null, warnings, errors };
  }

  collectAsciiWarnings(text, warnings);

  let tokens;
  try {
    tokens = tokenize(text);
  } catch (err) {
    errors.push({ severity: "error", code: "syntax-error", message: err.message, line: err.line });
    return { tree: null, warnings, errors };
  }

  const pos = { i: 0 };
  while (tokens[pos.i] && tokens[pos.i].t !== "(") pos.i++; // pula qualquer coisa antes do primeiro form (comentario, ou um 'SLOT_ID' de topo raro -- gramatica real: file ::= form | SLOT_ID form)
  if (!tokens[pos.i]) {
    errors.push({ severity: "error", code: "syntax-error", message: "nenhum '(' encontrado -- arquivo vazio ou so comentarios?" });
    return { tree: null, warnings, errors };
  }

  let ast;
  try {
    ast = parseForm(tokens, pos);
  } catch (err) {
    errors.push({ severity: "error", code: "syntax-error", message: err.message, line: err.line });
    return { tree: null, warnings, errors };
  }

  // Conteudo sobrando depois da primeira forma raiz -- o conversor antigo
  // ignorava isto em silencio (so' parseava UMA forma e nunca olhava o
  // resto). Um lone ':' sobrando (do topo raro 'SLOT_ID form') e' tolerado
  // sem aviso; qualquer outra coisa vira aviso 'multi-root'.
  let trailing = pos.i;
  while (tokens[trailing] && tokens[trailing].t === ":") trailing++;
  if (tokens[trailing]) {
    warnings.push({
      severity: "warning", code: "multi-root",
      message: `linha ${tokens[trailing].line}: ha conteudo depois da primeira forma raiz -- foi ignorado.`,
      line: tokens[trailing].line,
    });
  }

  core.resetIdCounter(1);
  const tree = astFormToNode(ast, byFactory);
  collectStructuralWarnings(tree, byFactory, warnings);

  return { tree, warnings, errors };
}

// Compatibilidade com o formato antigo (usado por
// src/ui/scripts/edl_to_ui_project.js, que so precisa da arvore ou de uma
// excecao -- o mesmo contrato que ja tinha antes desta peca virar
// compartilhada).
function convert(text, byFactory) {
  const result = parseEdlDocument(text, byFactory);
  if (result.errors && result.errors.length) {
    throw new Error(result.errors[0].message);
  }
  return result.tree;
}

if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    tokenize, parseValue, parseEntry, parseForm, parseList, parseBracket,
    scalarToSlotValue, astValueToChildNode, assignSlot, astFormToNode,
    detectUnresolvedIncludes, parseEdlDocument, convert,
  };
}
