#!/usr/bin/env node
//
// edl_builder.jsx -> edl-builder.html
//
// Mesmo padrao de docs/manual/compile.js (doc.jsx -> index.html) -- copiado,
// nao importado, de proposito: src/ui/ e um subprojeto AUTOCONTIDO (mesma
// convencao de qualquer coisa sob src/, ver CLAUDE.md), com seu proprio
// cache (./src/ui/.cache/), sem depender de docs/ em nada. Unico passo que
// precisa de rede (baixar React/ReactDOM 18 UMD + o transpilador JSX, uma
// vez) -- depois disso "node src/ui/scripts/compile.js" roda sem rede
// nenhuma, e o ARQUIVO GERADO continua zero-rede pra abrir.
//
// O que faz, em ordem:
//   1) injeta src/ui/edl_catalog.generated.json como 'const EDL_CATALOG = ...;'
//      e src/ui/preset.json como 'const EDL_PRESET = ...;', nos dois casos
//      num <script> proprio ANTES do app -- os dois sao gerados por passos
//      ANTERIORES do mesmo orquestrador (src/ui/scripts/build.js), diferente
//      de doc.jsx, que embute seus dados a mao dentro do proprio .jsx. A
//      ferramenta abre com a arvore VAZIA por padrao (ver o comentario de
//      `loadPresetTree()` em edl_builder.jsx) -- EDL_PRESET so' e' lido sob
//      demanda, pelo botao "Carregar preset".
//   2) concatena src/ui/edl_builder_core.js (a logica PURA do editor, sem
//      React/JSX -- separado so pra poder ser testado em Node puro por
//      edl_builder.test.js) na MESMA tag <script> do app, ANTES dele --
//      fica em escopo sem precisar de bundler nem de resolucao de modulo
//      no navegador.
//   3) troca `import React, {...} from "react";` (o `{...}` e opcional) por
//      `const {...} = React;` -- unico jeito de o Babel aceitar o arquivo
//      sem virar um modulo ES de verdade (que exigiria um bundler).
//   4) troca `export default function App()` por `function App()` -- mesmo
//      motivo: sem bundler, `export` nao tem pra onde ir.
//   5) transpila JSX -> React.createElement via @babel/standalone (preset
//      "react"), SEM minificar -- o arquivo gerado continua legivel/
//      diffavel.
//   6) concatena tres <script>: React, ReactDOM, o app transpilado -- e
//      fecha com o mount (ReactDOM.createRoot(...).render(...)).
//
// Uso: node src/ui/scripts/compile.js -- chamado automaticamente por
// src/ui/scripts/build.js (que e' o que `make open-edl-builder` roda).
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const UI_DIR = path.resolve(__dirname, "..");
const ROOT = path.resolve(UI_DIR, "..", "..");
const SRC = path.join(UI_DIR, "edl_builder.jsx");
const CORE = path.join(UI_DIR, "edl_builder_core.js");
const PARSER_CORE = path.join(UI_DIR, "edl_parser_core.js");
const DATA = path.join(UI_DIR, "edl_catalog.generated.json");
const DATA_VAR = "EDL_CATALOG";
const PRESET = path.join(UI_DIR, "preset.json");
const PRESET_VAR = "EDL_PRESET";
const OUT = path.join(UI_DIR, "edl-builder.html");
const TITLE = "MIXR — Editor gráfico de cenário .edl";
const CACHE = path.join(UI_DIR, ".cache");

const REACT_VERSION = "18.3.1";
const CDN = "https://cdnjs.cloudflare.com/ajax/libs";

function ensureCached(relPath, url) {
  const dest = path.join(CACHE, relPath);
  if (fs.existsSync(dest)) return dest;
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  console.log(`baixando (uma vez, cacheado em src/ui/.cache/): ${url}`);
  execFileSync("curl", ["-sL", "--fail", "-o", dest, url]);
  return dest;
}

function ensureBabelStandalone() {
  const modPath = path.join(CACHE, "node_modules", "@babel", "standalone");
  if (fs.existsSync(modPath)) return modPath;
  console.log("instalando @babel/standalone em src/ui/.cache/ (uma vez)...");
  fs.mkdirSync(CACHE, { recursive: true });
  execFileSync("npm", ["install", "--no-save", "--prefix", CACHE, "@babel/standalone"], { stdio: "inherit" });
  return modPath;
}

// `import React from "react";` OU `import React, { a, b } from "react";` --
// o grupo dos hooks e opcional.
const IMPORT_RE = /^import React(?:,\s*\{([^}]*)\})?\s*from\s*"react";\r?\n/;

function main() {
  const reactPath = ensureCached(`react-${REACT_VERSION}.min.js`, `${CDN}/react/${REACT_VERSION}/umd/react.production.min.js`);
  const reactDomPath = ensureCached(`react-dom-${REACT_VERSION}.min.js`, `${CDN}/react-dom/${REACT_VERSION}/umd/react-dom.production.min.js`);
  ensureBabelStandalone();
  const Babel = require(path.join(CACHE, "node_modules", "@babel", "standalone"));

  let src = fs.readFileSync(SRC, "utf8");
  const m = src.match(IMPORT_RE);
  if (!m) {
    throw new Error(`edl_builder.jsx: primeira linha nao e um import de React reconhecido -- ajuste compile.js. Encontrado: ${JSON.stringify(src.split("\n")[0])}`);
  }
  const names = (m[1] || "").trim();
  src = (names ? `const { ${names} } = React;\n` : "") + src.slice(m[0].length);

  const exportLine = "export default function App()";
  if (!src.includes(exportLine)) {
    throw new Error("edl_builder.jsx: nao achei 'export default function App()' -- ajuste compile.js.");
  }
  src = src.replace(exportLine, "function App()");

  if (!fs.existsSync(DATA)) {
    throw new Error(`${path.relative(ROOT, DATA)} nao existe -- rode 'node src/ui/scripts/generate_edl_catalog.py > ${path.relative(ROOT, DATA)}' antes (ou 'node src/ui/scripts/build.js', que ja encadeia isso).`);
  }
  if (!fs.existsSync(PRESET)) {
    throw new Error(`${path.relative(ROOT, PRESET)} nao existe -- rode 'node src/ui/scripts/build.js' antes.`);
  }
  const dataScript = `<script>\nconst ${DATA_VAR} = ${fs.readFileSync(DATA, "utf8")};\n` +
    `const ${PRESET_VAR} = ${fs.readFileSync(PRESET, "utf8")};\n</script>\n`;
  // edl_parser_core.js (o motor de parse de .edl real, compartilhado com o
  // botao "Carregar .edl") DEPOIS de edl_builder_core.js -- ele depende de
  // makeNode/makeTextLeaf/resetIdCounter/isAscii ja estarem no escopo do
  // script (ver o comentario de 'core' no topo de edl_parser_core.js).
  const coreSrc = fs.readFileSync(CORE, "utf8") + "\n" + fs.readFileSync(PARSER_CORE, "utf8") + "\n";

  const { code } = Babel.transform(src, { presets: ["react"], filename: "edl_builder.jsx", comments: true });

  const reactSrc = fs.readFileSync(reactPath, "utf8");
  const reactDomSrc = fs.readFileSync(reactDomPath, "utf8");

  const html = `<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
<title>${TITLE}</title>
<style>
/* Reset MINIMO, direto no HEAD (nao dentro da tag de estilo que o proprio
   App injeta mais abaixo, ja dentro do body -- essa so existe DEPOIS que o
   React monta, tarde demais pra evitar o flash da margem padrao do
   user-agent). Sem isto, os 8px de "body { margin }" do navegador
   apareciam como uma borda visivel entre o limite da janela e o fundo da
   ferramenta nas quatro bordas. ARMADILHA JA CONFIRMADA: nao escrever a
   sequencia de caracteres "fecha-tag-de-estilo" dentro deste comentario --
   o conteudo de <style> e' RAW TEXT pro parser HTML, que fecha a tag no
   primeiro sinal dessa sequencia, mesmo dentro de um comentario CSS. */
html, body { margin: 0; padding: 0; height: 100%; }
</style>
</head>
<body>
<div id="root"></div>
<script>
${reactSrc}

</script>
<script>
${reactDomSrc}

</script>
${dataScript}<script>
${coreSrc}${code}
ReactDOM.createRoot(document.getElementById('root')).render(React.createElement(App));
</script>
</body>
</html>
`;

  fs.writeFileSync(OUT, html);
  console.log(`escrito: ${path.relative(ROOT, OUT)} (${html.length} bytes)`);
}

main();
