#!/usr/bin/env node
"use strict";
//
// Suite PERMANENTE, no DOM de verdade (jsdom), do editor grafico de cenario
// (edl-builder.html). Ate aqui, toda verificacao no DOM real deste arquivo
// era um script DESCARTAVEL no scratchpad da sessao (dezenas deles, ver o
// diario de passadas em CLAUDE.md, secao "src/ui") -- nunca commitado, nunca
// reexecutado automaticamente. edl_builder.test.js/edl_parser_core.test.js
// cobrem a logica PURA (edl_builder_core.js/edl_parser_core.js); esta suite
// cobre o que so existe depois de montar: cliques, foco, destaque na previa,
// contagem de cartoes, o fluxo de exportar.
//
// Roda contra src/ui/edl-builder.html JA COMPILADO -- nao chama compile.js
// (mesmo principio de edl_builder.test.js: cada passo testa o SEU proprio
// insumo). build.js chama esta suite DEPOIS de compile.js (passo 6/6), entao
// 'node src/ui/scripts/build.js'/'make open-edl' ja cobre isto sempre.
// Chamada direta: node src/ui/edl_builder_dom.test.js
//
// jsdom NAO e dependencia do resto do projeto (nenhum outro lugar deste
// repositorio precisa de DOM simulado) -- instalado sob demanda, mesmo
// padrao (pinado, self-healing) que compile.js ja usa para
// @babel/standalone, mas num PREFIXO PROPRIO (.cache-dom/), nunca dentro de
// src/ui/.cache/. 'npm install <pkg> --prefix
// DIR --no-save' reconcilia o node_modules INTEIRO daquele prefixo contra
// um manifesto efemero de UM pacote so -- rodar isso duas vezes com pacotes
// DIFERENTES no MESMO prefixo faz a segunda chamada PODAR as dependencias
// da primeira como "extraneous". Compartilhar .cache/ com
// ensureBabelStandalone() fazia cada 'node src/ui/scripts/build.js' reinstalar
// os DOIS pacotes do zero, sempre -- o oposto do que "cacheado" promete, e
// silenciosamente dependente de rede toda vez.
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const UI_DIR = __dirname;
const CACHE = path.join(UI_DIR, ".cache-dom");
const HTML_PATH = path.join(UI_DIR, "edl-builder.html");

// PINADO -- mesmo raciocinio do BABEL_STANDALONE_VERSION em compile.js:
// atualizar e decisao deliberada (rodar este arquivo de novo), nunca efeito
// colateral de um 'npm install' sem versao.
const JSDOM_VERSION = "24.1.1";

function ensureJsdom() {
  const modPath = path.join(CACHE, "node_modules", "jsdom");
  const pkgPath = path.join(modPath, "package.json");
  if (fs.existsSync(pkgPath)) {
    const installed = JSON.parse(fs.readFileSync(pkgPath, "utf8")).version;
    if (installed === JSDOM_VERSION) return modPath;
    console.log(`src/ui/.cache-dom tinha jsdom ${installed} -- reinstalando ${JSDOM_VERSION} (pinado)...`);
    fs.rmSync(modPath, { recursive: true, force: true });
  }
  console.log(`instalando jsdom@${JSDOM_VERSION} em src/ui/.cache-dom/ (uma vez)...`);
  fs.mkdirSync(CACHE, { recursive: true });
  execFileSync("npm", ["install", "--no-save", "--prefix", CACHE, `jsdom@${JSDOM_VERSION}`], { stdio: "inherit" });
  return modPath;
}

let failures = 0;
let ran = 0;

// Cada teste monta a PROPRIA janela jsdom (mountFreshPage abaixo) e nunca a
// fecha sozinho -- sem fechar, os timers do Scheduler de janelas de testes
// ANTERIORES continuam competindo pelo laco de eventos do MESMO processo
// Node, e um teste mais adiante na lista passa a esperar mais do que
// deveria por uma condicao que ja e verdadeira (medido: sem isto, o
// teste de Expandir/Recolher tudo -- o terceiro da lista -- estourava um
// timeout de 5s que sozinho, em processo proprio, resolvia em <1s).
// 'test()' fecha toda janela que a PROPRIA rodada abriu, sucesso ou falha.
const openWindows = [];

async function test(name, fn) {
  ran++;
  try {
    await fn();
    console.log(`  ok  -- ${name}`);
  } catch (err) {
    failures++;
    console.error(`  FALHA -- ${name}`);
    console.error(`    ${err.stack || err}`);
  } finally {
    while (openWindows.length) {
      const w = openWindows.pop();
      try { w.close(); } catch { /* melhor esforco -- nao mascarar o resultado do teste por isto */ }
    }
  }
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg || "assertion failed");
}

// Monta uma instancia NOVA da pagina -- cada teste parte de estado limpo (o
// React da pagina nao tem como ser "resetado" no lugar; recriar o jsdom e
// mais barato e mais confiavel que tentar desfazer estado a mao). Devolve
// {window, document} com console.error/warn capturados (a pagina nunca deve
// logar erro durante o fluxo feliz -- se logar, e sinal de exception engolida
// por dentro do React, nao um teste que precisa saber "o que" logou).
// Achado rodando, nao suposto: o evento 'load' do jsdom dispara ANTES do
// primeiro commit do React. Os <script> (inclusive o
// 'ReactDOM.createRoot(...).render(...)' final) executam sincronamente
// durante o parse, mas o Scheduler que o React 18 usa por baixo do
// 'createRoot' agenda o commit inicial numa macrotask PROPRIA (MessageChannel,
// ou setTimeout se o ambiente nao tiver MessageChannel) -- 'load' e essa
// macrotask nao tem ordem garantida entre si, e medido aqui 'load' vence.
// Por isso esta funcao POLLA o '#root' em vez de esperar um evento so: e o
// unico sinal que realmente significa "o primeiro render aconteceu".
function waitFor(predicate, { timeoutMs = 15000, intervalMs = 20 } = {}) {
  return new Promise((resolve, reject) => {
    const start = Date.now();
    const tick = () => {
      let value;
      try { value = predicate(); } catch (err) { reject(err); return; }
      if (value) { resolve(value); return; }
      if (Date.now() - start > timeoutMs) { reject(new Error(`timeout (${timeoutMs}ms) esperando a condicao`)); return; }
      setTimeout(tick, intervalMs);
    };
    tick();
  });
}

function mountFreshPage(JSDOM) {
  if (!fs.existsSync(HTML_PATH)) {
    throw new Error(
      `${path.relative(path.join(UI_DIR, "..", ".."), HTML_PATH)} nao existe -- ` +
      `rode 'node src/ui/scripts/build.js' antes (ele chama compile.js, que gera este arquivo).`
    );
  }
  const html = fs.readFileSync(HTML_PATH, "utf8");
  const consoleErrors = [];
  const dom = new JSDOM(html, {
    runScripts: "dangerously",
    resources: "usable",
    url: "http://localhost/edl-builder.html",
  });
  const { window } = dom;
  openWindows.push(window);
  // scrollIntoView nao existe no jsdom (nao implementa layout de verdade) --
  // sem o stub, o useEffect de scroll do ExportPanel lancaria
  // "scrollIntoView is not a function" e o React engoliria como erro de
  // render, mascarando o teste real por tras de um erro de infraestrutura.
  window.HTMLElement.prototype.scrollIntoView = function () {};
  const origError = window.console.error.bind(window.console);
  window.console.error = (...args) => { consoleErrors.push(args.map(String).join(" ")); origError(...args); };
  return waitFor(() => window.document.getElementById("root").children.length > 0)
    .then(() => ({ window, document: window.document, consoleErrors }));
}

function clickButtonByText(document, text) {
  const btn = Array.from(document.querySelectorAll("button")).find((b) => b.textContent.trim().startsWith(text));
  if (!btn) throw new Error(`nenhum <button> com texto comecando em ${JSON.stringify(text)}`);
  btn.dispatchEvent(new btn.ownerDocument.defaultView.MouseEvent("click", { bubbles: true, cancelable: true }));
  return btn;
}

// dispara o setter nativo do input (o mesmo truque ja usado nos scripts
// descartaveis anteriores, ver CLAUDE.md "Passada seguinte -- destaque
// granular por campo"): setar '.value' direto nao dispara o onChange do
// React, porque o React intercepta o setter da PROPRIA instancia via um
// descriptor customizado -- e preciso chamar o setter da PROTOTYPE chain.
function setReactInputValue(window, input, value) {
  const proto = input.tagName === "TEXTAREA" ? window.HTMLTextAreaElement.prototype : window.HTMLInputElement.prototype;
  const setter = Object.getOwnPropertyDescriptor(proto, "value").set;
  setter.call(input, value);
  input.dispatchEvent(new window.Event("input", { bubbles: true }));
}

async function main() {
  ensureJsdom();
  const { JSDOM } = require(path.join(CACHE, "node_modules", "jsdom"));

  console.log("edl_builder_dom.test.js (jsdom, contra edl-builder.html compilado)");

  await test("pagina monta sem erro de console, arvore vazia por padrao", async () => {
    const { document, consoleErrors } = await mountFreshPage(JSDOM);
    assert(document.title.includes("MIXR"), `titulo inesperado: ${document.title}`);
    assert(document.querySelector(".eb-root-drop"), "esperava o placeholder de raiz vazia (.eb-root-drop)");
    assert(document.querySelectorAll(".eb-card-head").length === 0, "arvore deveria nascer vazia (sem preset carregado)");
    assert(consoleErrors.length === 0, `console.error inesperado no mount: ${consoleErrors.join(" | ")}`);
  });

  // O clique e' sincrono (React atende o evento na hora), mas o COMMIT que
  // ele agenda passa pelo mesmo Scheduler assincrono do mount inicial (ver o
  // comentario de waitFor() acima) -- toda asserção sobre o EFEITO de um
  // clique espera a condicao esperada em vez de ler o DOM na sequencia.
  async function loadPresetAndWait(document) {
    clickButtonByText(document, "Carregar preset");
    await waitFor(() => document.querySelectorAll(".eb-card-head").length > 0);
  }

  function clickAndWait(el, window, predicate) {
    el.dispatchEvent(new window.MouseEvent("click", { bubbles: true, cancelable: true }));
    return waitFor(predicate);
  }

  await test("Carregar preset popula a arvore e a aba Abertos reflete o estado real", async () => {
    const { document } = await mountFreshPage(JSDOM);
    await loadPresetAndWait(document);
    const cardCount = document.querySelectorAll(".eb-card-head").length;
    assert(cardCount > 10, `esperava dezenas de cartoes depois do preset, achei ${cardCount}`);
    const abertosBtn = Array.from(document.querySelectorAll("button")).find((b) => b.textContent.trim().startsWith("Abertos"));
    assert(abertosBtn, "botao da aba Abertos nao encontrado");
    assert(/Abertos (✓|\(\d+\))/.test(abertosBtn.textContent), `rotulo da aba Abertos fora do formato esperado: ${abertosBtn.textContent}`);
  });

  await test("Expandir tudo / Recolher tudo mudam a contagem de cartoes visiveis", async () => {
    const { window, document } = await mountFreshPage(JSDOM);
    await loadPresetAndWait(document);
    const beforeExpand = document.querySelectorAll(".eb-card-head").length;
    const expandBtn = Array.from(document.querySelectorAll("button")).find((b) => b.textContent.trim() === "Expandir tudo");
    assert(expandBtn, "botao 'Expandir tudo' nao encontrado (so aparece com a arvore carregada)");
    await clickAndWait(expandBtn, window, () => document.querySelectorAll(".eb-card-head").length > beforeExpand);
    const expanded = document.querySelectorAll(".eb-card-head").length;
    const collapseBtn = Array.from(document.querySelectorAll("button")).find((b) => b.textContent.trim() === "Recolher tudo");
    // 'Recolher tudo' zera expandedIds -- mas a RAIZ e' sempre expandida por
    // 'isRoot' (handleCollapseAll -> new Set(), TreeNode: 'isRoot ||
    // expandedIds.has(...)'), entao os filhos DIRETOS da raiz continuam
    // aparecendo como cartao (so os NETOS somem). "so a raiz visivel" e'
    // sobre PROFUNDIDADE (nada abre alem do 1o nivel), nao sobre CONTAGEM
    // total -- por isso a espera e' por "menos que expandido", nunca por
    // um numero fixo (que envelheceria a cada classe nova no catalogo).
    await clickAndWait(collapseBtn, window, () => document.querySelectorAll(".eb-card-head").length < expanded);
    const collapsed = document.querySelectorAll(".eb-card-head").length;
    assert(collapsed >= 1 && collapsed < beforeExpand, `recolher tudo deveria voltar pra profundidade 1 (menos que os ${beforeExpand} do default), achei ${collapsed}`);
    assert(expanded > collapsed, `expandir tudo (${expanded}) deveria mostrar mais cartoes que recolher tudo (${collapsed})`);
  });

  await test("selecionar um no destaca a regiao correspondente na previa .edl", async () => {
    const { window, document } = await mountFreshPage(JSDOM);
    await loadPresetAndWait(document);
    assert(document.querySelectorAll(".eb-edl-line-highlight").length === 0, "sem selecao, a previa nao deveria ter linha destacada");
    const rootCard = document.querySelector(".eb-card-head");
    await clickAndWait(rootCard, window, () => document.querySelectorAll(".eb-edl-line-highlight").length > 0);
    const highlighted = document.querySelectorAll(".eb-edl-line-highlight").length;
    assert(highlighted > 0, "selecionar a raiz deveria destacar pelo menos uma linha na previa");
  });

  await test("focar um campo de texto ja preenchido estreita o destaque pro CAMPO, nao o no inteiro", async () => {
    const { window, document } = await mountFreshPage(JSDOM);
    await loadPresetAndWait(document);
    const rootCard = document.querySelector(".eb-card-head");
    await clickAndWait(rootCard, window, () => document.querySelectorAll(".eb-edl-line-highlight").length > 0);
    const nodeHighlight = document.querySelectorAll(".eb-edl-line-highlight").length;
    assert(nodeHighlight > 1, `esperava o no raiz cobrindo varias linhas, achei ${nodeHighlight}`);

    const filledInput = Array.from(document.querySelectorAll(".eb-props .eb-field input[type=text]"))
      .find((el) => el.value && el.value.trim() !== "");
    assert(filledInput, "nenhum campo de texto ja preenchido no painel de propriedades da raiz");
    filledInput.focus();
    await waitFor(() => document.querySelectorAll(".eb-edl-line-highlight").length !== nodeHighlight);
    const fieldHighlight = document.querySelectorAll(".eb-edl-line-highlight").length;
    assert(fieldHighlight >= 1, "focar um campo deveria manter pelo menos uma linha destacada");
    assert(fieldHighlight < nodeHighlight, `destaque por campo (${fieldHighlight}) deveria ser mais estreito que o do no inteiro (${nodeHighlight})`);
  });

  await test("editar um campo de texto reflete na previa .edl ao vivo", async () => {
    const { window, document } = await mountFreshPage(JSDOM);
    await loadPresetAndWait(document);
    const rootCard = document.querySelector(".eb-card-head");
    await clickAndWait(rootCard, window, () => document.querySelectorAll(".eb-edl-line-highlight").length > 0);
    const filledInput = Array.from(document.querySelectorAll(".eb-props .eb-field input[type=text]"))
      .find((el) => el.value && el.value.trim() !== "");
    assert(filledInput, "nenhum campo de texto editavel encontrado");
    const marker = "ZZ_TESTE_DOM_ZZ";
    setReactInputValue(window, filledInput, marker);
    await waitFor(() => document.querySelector(".eb-edl-preview").textContent.includes(marker));
    const previewText = document.querySelector(".eb-edl-preview").textContent;
    assert(previewText.includes(marker), "o valor editado deveria aparecer na previa .edl imediatamente");
  });

  await test("Exportar .edl completa sem excecao para um cenario real carregado", async () => {
    const { window, document } = await mountFreshPage(JSDOM);
    await loadPresetAndWait(document);
    // jsdom nao tem File System Access API -- cai sempre no caminho de
    // download (Blob + <a download> + click(), com fallback window.open()
    // se Blob/URL.createObjectURL faltarem). Sem mexer no DOM real (o
    // ambiente de testes nao pode navegar), so' confirma-se que o fluxo
    // termina com uma mensagem de sucesso, nunca uma excecao escapando.
    let openedDataUri = null;
    window.open = (url) => { openedDataUri = url; return null; };
    const exportBtn = Array.from(document.querySelectorAll("button")).find((b) => b.textContent.trim().startsWith("Exportar"));
    assert(exportBtn, "botao 'Exportar .edl(.in)' nao encontrado");
    exportBtn.dispatchEvent(new window.MouseEvent("click", { bubbles: true, cancelable: true }));
    await waitFor(() => /✓ (baixado como|salvo em)/.test(document.body.textContent) || openedDataUri !== null);
    const succeeded = /✓ (baixado como|salvo em)/.test(document.body.textContent) || openedDataUri !== null;
    assert(succeeded, `esperava confirmacao de exportacao (download, salvo, ou fallback window.open); texto: ${document.body.textContent.slice(0, 300)}`);
  });

  console.log(`\n${ran - failures}/${ran} testes OK` + (failures ? ` -- ${failures} FALHARAM` : ""));
  if (failures > 0) process.exit(1);
}

main().catch((err) => { console.error(err); process.exit(1); });
