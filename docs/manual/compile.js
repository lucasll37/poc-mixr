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

// PINADO (achado por auditoria: instalar sem versao ja causou a regressao de
// "tela branca" documentada abaixo -- o Babel 8 mudou um default por baixo
// dos pes de quem so pedia "@babel/standalone"). 7.29.8 e a mesma versao ja
// cacheada e testada aqui; atualizar isto e' uma decisao deliberada, nunca
// um efeito colateral de rodar 'npm install' de novo.
const BABEL_STANDALONE_VERSION = "7.29.8";

function ensureBabelStandalone() {
  const modPath = path.join(CACHE, "node_modules", "@babel", "standalone");
  const pkgPath = path.join(modPath, "package.json");
  if (fs.existsSync(pkgPath)) {
    const installed = JSON.parse(fs.readFileSync(pkgPath, "utf8")).version;
    if (installed === BABEL_STANDALONE_VERSION) return modPath;
    console.log(`docs/manual/.cache tinha @babel/standalone ${installed} -- reinstalando ${BABEL_STANDALONE_VERSION} (pinado)...`);
    fs.rmSync(path.join(CACHE, "node_modules"), { recursive: true, force: true });
  }
  console.log(`instalando @babel/standalone@${BABEL_STANDALONE_VERSION} em docs/manual/.cache/ (uma vez)...`);
  fs.mkdirSync(CACHE, { recursive: true });
  execFileSync("npm", ["install", "--no-save", "--prefix", CACHE, `@babel/standalone@${BABEL_STANDALONE_VERSION}`], { stdio: "inherit" });
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

  // O preset "react" precisa de runtime: "classic" explícito. O default do
  // Babel 8 (instalado aqui sem pin, por 'npm install @babel/standalone') é
  // "automatic", que emite 'import { jsx as _jsx } from "react/jsx-runtime"'
  // no topo do código gerado -- inválido num <script> CLÁSSICO (não
  // type="module", sem bundler nenhum aqui): o navegador aborta o bloco
  // inteiro com "Cannot use import statement outside a module", e a página
  // fica com o <div id="root"> vazio (tela branca, sem erro visível na
  // tela). Com "classic" o JSX vira React.createElement, que casa com o
  // React UMD embutido acima.
  const { code } = Babel.transform(src, { presets: [["react", { runtime: "classic" }]], filename: "doc.jsx", comments: true });

  // Smoke-check pos-transpile (achado por auditoria): pega a MESMA classe de
  // regressao do "runtime: automatic" acima de forma AUTOMATIZADA, nao so
  // por comentario -- se um futuro default do Babel voltar a emitir
  // import/export no codigo transpilado, isso quebra a pagina em SILENCIO
  // (tela branca, sem erro visivel na tela), exatamente como ja aconteceu
  // uma vez. Sem este check, so um pin de versao nao pegaria uma regressao
  // equivalente causada por outra mudanca de default numa versao futura.
  if (/^\s*(import|export)\s/m.test(code)) {
    throw new Error(
      "doc.jsx transpilado ainda contem 'import'/'export' -- o Babel voltou a emitir codigo de " +
      "modulo ES (mesmo sintoma do bug de tela branca ja documentado acima). Confira o preset " +
      "'react' (precisa de runtime:'classic' explicito). NAO escrevendo index.html."
    );
  }
  const createElementCount = (code.match(/React\.createElement/g) || []).length;
  if (createElementCount < 10) {
    throw new Error(
      `doc.jsx transpilado tem so ${createElementCount} ocorrencia(s) de React.createElement -- ` +
      "sinal de que a transpilacao nao rodou de verdade (esperado: centenas). NAO escrevendo index.html."
    );
  }

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
