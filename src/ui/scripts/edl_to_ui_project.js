#!/usr/bin/env node
"use strict";
/*
 * Converte um .edl real (com ou sem '@token@'/'@include:@' sobrando) para o
 * formato de PROJETO do editor grafico ({id,factory,slotValues,children} --
 * ver src/ui/edl_builder_core.js). Wrapper Node fino sobre o motor de parse
 * COMPARTILHADO, src/ui/edl_parser_core.js -- que e' de onde vem toda a
 * logica de tokenizacao/parse/montagem da arvore (a mesma usada pelo botao
 * "Carregar .edl" do editor grafico, no navegador). Este arquivo so cuida
 * do que e' genuinamente Node: ler o '@include:'/'@TOKEN@' do disco (a
 * parte que edl_parser_core.js deliberadamente NAO faz -- ver o comentario
 * de parseEdlDocument() la) e a plumbing de linha de comando.
 *
 * Uso:
 *   node src/ui/scripts/edl_to_ui_project.js <arquivo.edl ou .edl.in> > saida.json
 *
 * Existe para gerar o cenario DEFAULT que a ferramenta carrega sob demanda
 * (botao "Carregar preset", ver src/ui/README.md) -- chamado automaticamente
 * por src/ui/scripts/build.js, sempre contra o cenario de referencia
 * (src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in -- o TEMPLATE,
 * nao o '.generated.edl' -- ver a nota abaixo).
 *
 * ATENCAO -- rode contra o '.edl.in', nao o '.generated.edl': confirmado
 * lendo os dois lado a lado que o 'scenario.generated.edl' committed desta
 * poc estava DESATUALIZADO em relacao ao template (provides: sem
 * 'ThreadTagProbe', type/model "A4" em vez de "C310", velocidades de
 * cruzeiro antigas) -- um artefato gerado, comitado uma vez, que nao
 * acompanhou edicoes seguintes do '.in'. Por isso expandTemplates() abaixo
 * aceita '@TOKEN@'/'@include:frag@' (mesma mecanica de edl_lint.py) e roda
 * direto contra o template, que e' a fonte de verdade.
 */

const fs = require("fs");
const path = require("path");

const REPO_ROOT = path.resolve(__dirname, "..", "..", "..");
const core = require(path.join(__dirname, "..", "edl_builder_core.js"));
const parser = require(path.join(__dirname, "..", "edl_parser_core.js"));
const CATALOG_PATH = path.join(REPO_ROOT, "src", "ui", "edl_catalog.generated.json");
const FRAGMENTS_DIR = path.join(REPO_ROOT, "app", "configs", "fragments");

// Mesma mecanica de src/ui/scripts/edl_lint.py (expand_templates()):
// '@include:...@' puxa o fragmento de app/configs/fragments/, e qualquer
// '@TOKEN@' restante (ex.: '@NUM_TC_THREADS@') vira um numero neutro --
// aqui "2", o mesmo valor que as pocs de producao usam por convencao
// (bandit/single-thread/multi-thread), nao "1" (o de edl_lint.py, que so'
// precisa nao quebrar sintaxe): o cenario default desta ferramenta pode ser
// reexportado e inspecionado por um humano, entao vale a pena o numero
// parecer plausivel.
//
// Isto e' deliberadamente DIFERENTE do que o carregador INTERATIVO
// (botao "Carregar .edl", parseEdlDocument() em edl_parser_core.js) faz --
// la' o '@TOKEN@' fica LITERAL na arvore (o usuario edita o valor real
// antes de exportar) porque adivinhar um numero plausivel so' faz sentido
// pra gerar um preset DESCARTAVEL como este, nao pra editar um cenario de
// producao de verdade.
function expandTemplates(text) {
  text = text.replace(/@include:([^@]+)@/g, (_, name) => {
    const frag = path.join(FRAGMENTS_DIR, name);
    return fs.existsSync(frag) ? fs.readFileSync(frag, "utf8") : "";
  });
  text = text.replace(/@([A-Za-z_][A-Za-z0-9_]*)@/g, "2");
  return text;
}

function convert(text, byFactory) {
  return parser.convert(text, byFactory);
}

function main() {
  const file = process.argv[2];
  if (!file) {
    console.error("uso: node src/ui/scripts/edl_to_ui_project.js <arquivo.edl>");
    process.exit(1);
  }
  if (!fs.existsSync(CATALOG_PATH)) {
    console.error(`catalogo nao encontrado em ${CATALOG_PATH} -- rode 'node src/ui/scripts/generate_edl_catalog.py' (ou 'make open-edl-builder') primeiro.`);
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

module.exports = { tokenize: parser.tokenize, parseForm: parser.parseForm, convert };
