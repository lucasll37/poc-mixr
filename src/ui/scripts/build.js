#!/usr/bin/env node
"use strict";
//
// Orquestrador UNICO do editor grafico de cenario .edl -- substitui os
// antigos alvos separados do Makefile (`edl-catalog`, `edl-default-scenario`,
// `edl-builder`, `edl-builder-test`): ao rodar `make open-edl-builder`, TUDO
// que este editor precisa acontece aqui, sempre do zero, na ordem certa,
// abortando com erro claro no primeiro passo que falhar -- nunca produz um
// `edl-builder.html` parcial nem deixa abrir uma versao desatualizada (o
// defeito que o `open-edl-builder` antigo tinha: nao dependia de
// `edl-builder`, entao abria o que estivesse la, mesmo obsoleto).
//
// Sempre regenera do ZERO, sem cache de staleness: os tres artefatos
// gerados (edl_catalog.generated.json, preset.json, edl-builder.html) sao
// COMMITADOS mas de conteudo deterministico -- rodar de novo sem nenhuma
// mudanca no fonte do MIXR/modelo produz o MESMO conteudo byte a byte,
// entao nunca ha diff de git por rodar isto (so' por mudanca real).
// "Desatualizado" deixa de ser um estado possivel.
//
// A ferramenta abre com a arvore VAZIA por padrao -- `preset.json` (o
// cenario "player maximo" de built-in_mixr_1, o mais rico do repositorio)
// fica disponivel so' sob demanda, pelo botao "Carregar preset" da UI (ver
// `loadPresetTree()` em edl_builder.jsx). Ainda assim e' gerado aqui, sempre:
// e' o mesmo cenario que o passo 4 usa pra se autotestar (round-trip .edl.in
// -> projeto -> .edl), e carregar-sob-demanda so' funciona se o arquivo
// existir no disco pro compile.js embutir.
//
// Passos, em ordem, cada um abortando o processo inteiro se falhar:
//   1) gera o catalogo (Python stdlib, sem MIXR)                    -- generate_edl_catalog.py
//   2) converte o cenario de referencia p/ o preset carregavel      -- edl_to_ui_project.js
//   3) roda os testes PUROS de edl_builder_core.js (gate de regressao) -- edl_builder.test.js
//   4) self-check: lint leve do .edl que o preset exportaria          -- edl_lint.py
//   5) compila edl_builder.jsx -> edl-builder.html                  -- compile.js
//
// Uso: node src/ui/scripts/build.js   (ou `make open-edl-builder`, que ja
// chama isto antes de abrir o navegador).
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const SCRIPTS_DIR = __dirname;
const UI_DIR = path.resolve(SCRIPTS_DIR, "..");
const ROOT = path.resolve(UI_DIR, "..", "..");

const CATALOG_PATH = path.join(UI_DIR, "edl_catalog.generated.json");
const PRESET_PATH = path.join(UI_DIR, "preset.json");
const PRESET_SOURCE = path.join(
  ROOT, "src", "poc", "built-in_mixr_1", "configs", "scenario_max_player.edl.in"
);

const GREEN = "\x1b[0;32m";
const YELLOW = "\x1b[1;33m";
const RED = "\x1b[0;31m";
const NC = "\x1b[0m";

function step(label, fn) {
  process.stdout.write(`${YELLOW}open-edl-builder:${NC} ${label}...\n`);
  fn();
}

function ok(label) {
  process.stdout.write(`${GREEN}open-edl-builder:${NC} ${label} OK\n`);
}

function fail(label, err) {
  process.stderr.write(`${RED}open-edl-builder:${NC} ${label} FALHOU\n`);
  if (err) process.stderr.write(String(err.message || err) + "\n");
  process.exit(1);
}

// maxBuffer generoso: o catalogo sozinho passa de 2 MB de JSON -- o default
// do Node (1 MB) estoura com ENOBUFS antes mesmo de reportar "maxBuffer
// exceeded" direito. 64 MB cobre qualquer um dos cinco passos com folga.
function run(cmd, args, opts) {
  return execFileSync(cmd, args, { cwd: ROOT, encoding: "utf8", maxBuffer: 64 * 1024 * 1024, ...opts });
}

function generateCatalog() {
  step("1/5 gerando catalogo (generate_edl_catalog.py)", () => {});
  let out;
  try {
    out = run("python3", [path.join(SCRIPTS_DIR, "generate_edl_catalog.py")]);
  } catch (err) {
    fail("gerar catalogo", err);
  }
  fs.writeFileSync(CATALOG_PATH, out);
  const count = JSON.parse(out).length;
  ok(`1/5 catalogo (${count} classes catalogadas)`);
}

function generatePreset() {
  step("2/5 convertendo cenario de referencia p/ preset (edl_to_ui_project.js)", () => {});
  let out;
  try {
    out = run("node", [path.join(SCRIPTS_DIR, "edl_to_ui_project.js"), PRESET_SOURCE]);
  } catch (err) {
    fail("gerar preset", err);
  }
  fs.writeFileSync(PRESET_PATH, out);
  ok("2/5 preset");
}

function runUnitTests() {
  step("3/5 rodando testes puros (edl_builder_core.js + edl_parser_core.js)", () => {});
  for (const file of ["edl_builder.test.js", "edl_parser_core.test.js"]) {
    try {
      const out = run("node", [path.join(UI_DIR, file)]);
      process.stdout.write(out);
    } catch (err) {
      process.stderr.write(String(err.stdout || "") + String(err.stderr || ""));
      fail(`testes de ${file} -- build interrompido antes de compilar`, err);
    }
  }
  ok("3/5 testes");
}

// Self-check: lint leve sobre o .edl que o PRESET recem-convertido
// exportaria -- pega em silencio um round-trip quebrado (fabrica/slot
// desconhecido introduzido por uma mudanca no catalogo) antes de alguem
// nem abrir o navegador. So' aborta em ERRO (edl_lint.py so' bloqueia por
// erro, nao por aviso -- ver o cabecalho dele).
function selfLintPreset() {
  step("4/5 self-check: lint do .edl round-tripado do preset", () => {});
  const core = require(path.join(UI_DIR, "edl_builder_core.js"));
  const catalog = JSON.parse(fs.readFileSync(CATALOG_PATH, "utf8"));
  const tree = JSON.parse(fs.readFileSync(PRESET_PATH, "utf8"));
  const byFactory = core.buildCatalogIndex(catalog);
  const text = core.projectToEdl(tree, byFactory);

  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "edl-builder-selfcheck-"));
  const tmpFile = path.join(tmpDir, "scenario.edl");
  fs.writeFileSync(tmpFile, text);
  try {
    const out = run("python3", [path.join(SCRIPTS_DIR, "edl_lint.py"), tmpFile]);
    process.stdout.write(out);
  } catch (err) {
    process.stdout.write(String(err.stdout || ""));
    fail("self-check do preset -- o round-trip .edl.in -> projeto -> .edl introduziu um erro", err);
  } finally {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  }
  ok("4/5 self-check");
}

function compile() {
  step("5/5 compilando edl-builder.html (compile.js)", () => {});
  try {
    const out = run("node", [path.join(SCRIPTS_DIR, "compile.js")]);
    process.stdout.write(out);
  } catch (err) {
    process.stderr.write(String(err.stdout || "") + String(err.stderr || ""));
    fail("compilar edl-builder.html", err);
  }
  ok("5/5 edl-builder.html");
}

function main() {
  generateCatalog();
  generatePreset();
  runUnitTests();
  selfLintPreset();
  compile();
  process.stdout.write(`${GREEN}open-edl-builder:${NC} pipeline completo -- src/ui/edl-builder.html pronto\n`);
}

main();
