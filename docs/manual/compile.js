#!/usr/bin/env node
//
// doc.jsx -> index.html
//
// Unico passo que precisa de rede neste diretorio (so pra buscar React/
// ReactDOM 18 UMD e o transpilador JSX, uma vez, em docs/manual/.cache/ --
// depois disso "node docs/manual/compile.js" roda sem rede nenhuma) -- o
// ARQUIVO GERADO continua zero-rede pra abrir (ver docs/manual/README.md,
// "Como abrir").
//
// O que faz, em ordem:
//   1) injeta docs/manual/catalog.generated.js (MODEL/FACTORIES/SNIPPETS/
//      STATS, escrito por tools/generate_manual_catalog.py -- 'make open-docs'
//      roda o gerador ANTES deste script) num <script> proprio, ANTES do
//      app -- mesma mecanica de src/ui/scripts/compile.js para
//      EDL_CATALOG, so que aqui os 4 consts ja vem prontos como texto JS
//      (o gerador escreve 'const NOME = {...};' direto), nao um unico JSON
//      que precisa virar 'const X = <json>;' aqui.
//   2) troca a linha `import React, {...} from "react";` por
//      `const {...} = React;` -- e o unico jeito de o Babel aceitar o
//      arquivo sem virar um modulo ES de verdade (que exigiria um bundler).
//   3) troca `export default function App()` por `function App()` -- mesmo
//      motivo: sem bundler, `export` nao tem pra onde ir.
//   4) transpila JSX -> React.createElement via @babel/standalone (preset
//      "react"), SEM minificar -- o arquivo gerado continua legivel/
//      diffavel, do mesmo jeito que sempre foi.
//   5) concatena quatro <script>: React, ReactDOM, o catalogo gerado, o app
//      transpilado -- e fecha com o mount (ReactDOM.createRoot(...).render(...)).
//
// Uso: node docs/manual/compile.js   (ou `make open-docs`, que roda o gerador
// Python, depois isto, e por fim abre o resultado no navegador)
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const ROOT = path.resolve(__dirname, "..", "..");
const SRC = path.join(__dirname, "doc.jsx");
const CATALOG = path.join(__dirname, "catalog.generated.js");
const OUT = path.join(__dirname, "index.html");
const CACHE = path.join(__dirname, ".cache");

const REACT_VERSION = "18.3.1";
const CDN = "https://cdnjs.cloudflare.com/ajax/libs";

function ensureCached(relPath, url) {
  const dest = path.join(CACHE, relPath);
  if (fs.existsSync(dest)) return dest;
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  console.log(`baixando (uma vez, cacheado em docs/manual/.cache/): ${url}`);
  execFileSync("curl", ["-sL", "--fail", "-o", dest, url]);
  return dest;
}

function ensureBabelStandalone() {
  const modPath = path.join(CACHE, "node_modules", "@babel", "standalone");
  if (fs.existsSync(modPath)) return modPath;
  console.log("instalando @babel/standalone em docs/manual/.cache/ (uma vez)...");
  fs.mkdirSync(CACHE, { recursive: true });
  execFileSync("npm", ["install", "--no-save", "--prefix", CACHE, "@babel/standalone"], { stdio: "inherit" });
  return modPath;
}

function main() {
  const reactPath = ensureCached(`react-${REACT_VERSION}.min.js`, `${CDN}/react/${REACT_VERSION}/umd/react.production.min.js`);
  const reactDomPath = ensureCached(`react-dom-${REACT_VERSION}.min.js`, `${CDN}/react-dom/${REACT_VERSION}/umd/react-dom.production.min.js`);
  ensureBabelStandalone();
  const Babel = require(path.join(CACHE, "node_modules", "@babel", "standalone"));

  let src = fs.readFileSync(SRC, "utf8");

  const importLine = 'import React, { useState, useEffect, useLayoutEffect, useMemo, useRef, useCallback } from "react";';
  if (!src.startsWith(importLine)) {
    throw new Error(`a primeira linha de doc.jsx mudou -- ajuste build.js. Encontrado: ${JSON.stringify(src.split("\n")[0])}`);
  }
  const names = importLine.match(/\{([^}]+)\}/)[1].trim();
  src = `const { ${names} } = React;\n` + src.slice(importLine.length);

  const exportLine = "export default function App()";
  if (!src.includes(exportLine)) {
    throw new Error("nao achei 'export default function App()' -- ajuste build.js.");
  }
  src = src.replace(exportLine, "function App()");

  if (!fs.existsSync(CATALOG)) {
    throw new Error(`${path.relative(ROOT, CATALOG)} nao existe -- rode 'python3 tools/generate_manual_catalog.py > ${path.relative(ROOT, CATALOG)}' antes (ou 'make open-docs', que ja encadeia isso).`);
  }
  // catalog.generated.js ja e' texto JS pronto ('const MODEL = {...};' etc,
  // escrito por tools/generate_manual_catalog.py) -- diferente do
  // EDL_CATALOG de src/ui/scripts/compile.js (um JSON puro que precisa virar
  // 'const X = <json>;' aqui), este arquivo so precisa ser concatenado como
  // esta.
  const catalogScript = `<script>\n${fs.readFileSync(CATALOG, "utf8")}\n</script>\n`;

  // ARMADILHA MEDIDA (nao redescobrir): o preset "react" TEM de levar
  // runtime: "classic" EXPLICITO. O default mudou no Babel 8 (instalado aqui
  // sem pin, por 'npm install @babel/standalone') de "classic" para
  // "automatic" -- e o automatic emite
  // 'import { jsx as _jsx } from "react/jsx-runtime"' no topo do codigo
  // gerado. Como esse codigo e' concatenado num <script> CLASSICO (nao
  // type="module", e nao ha bundler nenhum aqui), o navegador aborta o bloco
  // inteiro com "Uncaught SyntaxError: Cannot use import statement outside a
  // module" -- a pagina abre, o <title> aparece, e o <div id="root"> fica
  // VAZIO: tela branca, sem nenhum erro visivel na tela. Medido no Chrome
  // com @babel/standalone 8.0.4. Com "classic" o JSX volta a virar
  // React.createElement, que e' o que casa com o React UMD embutido acima.
  const { code } = Babel.transform(src, { presets: [["react", { runtime: "classic" }]], filename: "doc.jsx", comments: true });

  const reactSrc = fs.readFileSync(reactPath, "utf8");
  const reactDomSrc = fs.readFileSync(reactDomPath, "utf8");

  const html = `<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
<title>MIXR — Explorador de execução, EDL e classes built-in</title>
</head>
<body>
<div id="root"></div>
<script>
${reactSrc}

</script>
<script>
${reactDomSrc}

</script>
${catalogScript}<script>
${code}
ReactDOM.createRoot(document.getElementById('root')).render(React.createElement(App));
</script>
</body>
</html>
`;

  fs.writeFileSync(OUT, html);
  console.log(`escrito: ${path.relative(ROOT, OUT)} (${html.length} bytes)`);
}

main();
