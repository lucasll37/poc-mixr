# `docs/` — explorador de execução, EDL e classes built-in do MIXR

`index.html` é uma página estática, sem dependências de rede (React, ReactDOM e todo o app
ficam embutidos no próprio arquivo), com duas visões sobre o framework:

1. **Execução** — o mesmo ciclo de fases já visto na aba "Componentes" (F6) do `./app`
   (dynamics/transmit/receive/process + as duas threads de decisão/fundo), aqui desenhado
   sobre a árvore de classes do MIXR com um grafo navegável (pan/zoom), timeline com
   transporte (tocar/pausar/velocidade/passo/reiniciar) e trilha "ociosos"/"ligações por
   nome". Tem dois sub-modos, alternados por um toggle no topo:
   - **Cenário ilustrativo** — a árvore pequena e curada (Station → WorldModel → 2 Aircraft
     + Missile, ~24 nós) que a timeline acima percorre passo a passo.
   - **Catálogo completo** — as 122 classes de `mixr::models` (`DECLARE_SUBCLASS`), **sem
     exceção**, agrupadas pela própria cadeia de herança em 19 categorias (Veículos aéreos,
     Sensores de RF, Armamento, Navegação, Assinaturas, ...). Geometria própria — um grid de
     mini-árvores por categoria, não uma lista indentada única — para não empilhar 122 linhas
     numa coluna só e não sobrepor rótulo nenhum. Sem timeline (não é um traço de execução);
     clique num nó para ver herança e slots no painel abaixo, igual ao cenário ilustrativo.
2. **Catálogo** — as 342 classes que `DECLARE_SUBCLASS` no fork, cruzadas com quem de fato
   se registra em fábrica (`IMPLEMENT_*SUBCLASS`, 224 delas — 48 com nome de fábrica
   divergente do nome da classe), quem tem slot (`BEGIN_SLOTTABLE`/`END_SLOTTABLE`, 644
   slots em 135 classes) e quem participa do despacho por fase. Busca por classe, nome de
   fábrica ou slot; filtros por "nome divergente", "trabalha em fase", "não registradas" e
   "no cenário" (as classes que os cenários deste repositório de fato instanciam).

**Curada, não instrumentada.** Sem processo MIXR rodando por trás — herança, nome de
fábrica, registro, slots, fases e os trechos de código (com arquivo e linha reais) foram
extraídos direto da árvore de fontes (`contexts/src/mixr/`, fork v170600) pelo script
`scripts/extract_execution_chain.py` (fora deste diretório) e embutidos em `docs/doc.jsx` como os
objetos `MODEL`/`FACTORIES`/`SNIPPETS`/`STATS` — nada ali é digitado à mão. Isso está avisado na
própria página e não deve ser removido em incrementos futuros. A constante `CATALOG` (a grade de
122 classes por categoria do modo "Catálogo completo") também vem de
`scripts/extract_execution_chain.py --catalog`, mas o agrupamento em categorias/"forests" é um
passo manual sobre essa saída, ainda não versionado como script (ver "próximos incrementos"
abaixo).

## Como abrir

Direto no navegador, sem servidor nenhum (zero requisições de rede, inclusive React):

```
xdg-open docs/index.html     # ou file://.../docs/index.html
```

Ou servido (para simular hospedagem futura, ex. GitHub Pages):

```
python3 -m http.server --directory . 8000
# abrir http://localhost:8000/docs/
```

## `doc.jsx` → `index.html`

`doc.jsx` é a fonte (um componente React único, `App`, com CSS embutido em uma string e os
dados gerados embutidos como constantes — nenhum `fetch`/`import` além de `react`).
`index.html` é gerado a partir dele: JSX transpilado para `React.createElement` (Babel,
preset `react`) e React 18 + ReactDOM 18 (UMD, produção) inlinados no mesmo arquivo, para
que abrir `index.html` não dispare nenhuma requisição de rede. Regenerar depois de editar
`doc.jsx`:

```bash
cd /tmp && mkdir -p mixr-docbuild && cd mixr-docbuild
npm install --no-save @babel/standalone
curl -sL -o react.production.min.js      https://cdnjs.cloudflare.com/ajax/libs/react/18.3.1/umd/react.production.min.js
curl -sL -o react-dom.production.min.js  https://cdnjs.cloudflare.com/ajax/libs/react-dom/18.3.1/umd/react-dom.production.min.js
# transpila JSX -> JS (troca o import por `const {...} = React;` e o export default por mount)
# e concatena <script> com react + react-dom + app compilado num docs/index.html
```

Não há alvo de Makefile para isso (é edição ocasional, não parte do build C++/Meson).

## O que fica para os próximos incrementos

- Um alvo `make docs` que regenere `index.html` a partir de `doc.jsx` automaticamente (hoje é
  manual, ver acima). `make open-docs` já existe, mas só ABRE o `index.html` já gerado — não
  regenera nada.
- O passo que agrupa a saída de `scripts/extract_execution_chain.py --catalog` (uma lista plana
  de classes) nas categorias/"forests" da constante `CATALOG` não está versionado como script —
  foi feito uma vez, à mão, e o resultado vive só como a constante já embutida em `doc.jsx`.
  Reproduzir depois de uma mudança em `contexts/src/mixr/` exigiria refazer esse agrupamento.
- `scripts/generate_catalog_js.py` (versionado) gera `CATALOG_SRC`/`TAXONOMY`/`CATALOG_TOUR` a
  partir de `extract_execution_chain.py --catalog`, mas **nenhuma das três constantes é usada por
  `doc.jsx`** hoje — confirmado (`grep` não acha nenhuma delas na página). Parece um caminho
  alternativo de geração que foi abandonado em favor do agrupamento manual acima antes de ser
  ligado à página; vale decidir entre terminar de conectar (substituindo o agrupamento manual) ou
  remover o script.
