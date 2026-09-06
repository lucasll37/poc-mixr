#!/usr/bin/env node
//
// edl_builder.jsx -> edl-builder.html
//
// Mesmo padrao de docs/compile.js (doc.jsx -> index.html) -- copiado, nao
// importado, de proposito: src/ui/ e um subprojeto AUTOCONTIDO (mesma
// convencao de qualquer coisa sob src/, ver CLAUDE.md), com seu proprio
// cache (./src/ui/.cache/), sem depender de docs/ em nada. Unico passo que
// precisa de rede (baixar React/ReactDOM 18 UMD + o transpilador JSX, uma
// vez) -- depois disso "node src/ui/compile.js" roda sem rede nenhuma, e o
// ARQUIVO GERADO continua zero-rede pra abrir.
//
// O que faz, em ordem:
//   1) injeta src/ui/edl_catalog.generated.json como 'const EDL_CATALOG = ...;'
//      e src/ui/edl_default_scenario.generated.json como
//      'const EDL_DEFAULT_SCENARIO = ...;', nos dois casos num <script>
//      proprio ANTES do app -- os dois mudam por fonte gerada
//      separadamente (`make edl-catalog`/`make edl-default-scenario`),
//      diferente de doc.jsx, que embute seus dados a mao dentro do proprio
//      .jsx.
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
// Uso: node src/ui/compile.js   (ou `make edl-builder`, que ja encadeia
// `make edl-catalog` antes)
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const ROOT = path.resolve(__dirname, "../..");
const SRC = path.join(__dirname, "edl_builder.jsx");
const CORE = path.join(__dirname, "edl_builder_core.js");
const DATA = path.join(__dirname, "edl_catalog.generated.json");
const DATA_VAR = "EDL_CATALOG";
const DEFAULT_SCENARIO = path.join(__dirname, "edl_default_scenario.generated.json");
const DEFAULT_SCENARIO_VAR = "EDL_DEFAULT_SCENARIO";
const OUT = path.join(__dirname, "edl-builder.html");
const TITLE = "MIXR — Editor gráfico de cenário .edl";
const CACHE = path.join(__dirname, ".cache");

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
    throw new Error(`${path.relative(ROOT, DATA)} nao existe -- rode 'make edl-catalog' antes.`);
  }
  if (!fs.existsSync(DEFAULT_SCENARIO)) {
    throw new Error(`${path.relative(ROOT, DEFAULT_SCENARIO)} nao existe -- rode 'make edl-default-scenario' antes.`);
  }
  const dataScript = `<script>\nconst ${DATA_VAR} = ${fs.readFileSync(DATA, "utf8")};\n` +
    `const ${DEFAULT_SCENARIO_VAR} = ${fs.readFileSync(DEFAULT_SCENARIO, "utf8")};\n</script>\n`;
  const coreSrc = fs.readFileSync(CORE, "utf8") + "\n";

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
