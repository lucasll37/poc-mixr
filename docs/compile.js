#!/usr/bin/env node
//
// doc.jsx -> index.html
//
// Unico passo que precisa de rede neste diretorio (so pra buscar React/
// ReactDOM 18 UMD e o transpilador JSX, uma vez, em docs/.cache/ -- depois
// disso "node docs/compile.js" roda sem rede nenhuma) -- o ARQUIVO GERADO
// continua zero-rede pra abrir (ver docs/README.md, "Como abrir").
//
// O que faz, em ordem:
//   1) troca a linha `import React, {...} from "react";` por
//      `const {...} = React;` -- e o unico jeito de o Babel aceitar o
//      arquivo sem virar um modulo ES de verdade (que exigiria um bundler).
//   2) troca `export default function App()` por `function App()` -- mesmo
//      motivo: sem bundler, `export` nao tem pra onde ir.
//   3) transpila JSX -> React.createElement via @babel/standalone (preset
//      "react"), SEM minificar -- o arquivo gerado continua legivel/
//      diffavel, do mesmo jeito que sempre foi.
//   4) concatena tres <script>: React, ReactDOM, o app transpilado -- e
//      fecha com o mount (ReactDOM.createRoot(...).render(...)).
//
// Uso: node docs/compile.js   (ou `make docs`, que so chama isto)
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const SRC = path.join(__dirname, "doc.jsx");
const OUT = path.join(__dirname, "index.html");
const CACHE = path.join(__dirname, ".cache");

const REACT_VERSION = "18.3.1";
const CDN = "https://cdnjs.cloudflare.com/ajax/libs";

function ensureCached(relPath, url) {
  const dest = path.join(CACHE, relPath);
  if (fs.existsSync(dest)) return dest;
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  console.log(`baixando (uma vez, cacheado em docs/.cache/): ${url}`);
  execFileSync("curl", ["-sL", "--fail", "-o", dest, url]);
  return dest;
}

function ensureBabelStandalone() {
  const modPath = path.join(CACHE, "node_modules", "@babel", "standalone");
  if (fs.existsSync(modPath)) return modPath;
  console.log("instalando @babel/standalone em docs/.cache/ (uma vez)...");
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

  const { code } = Babel.transform(src, { presets: ["react"], filename: "doc.jsx", comments: true });

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
<script>
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
