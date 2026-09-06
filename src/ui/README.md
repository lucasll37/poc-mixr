# `src/ui` — editor gráfico de cenário `.edl`

Ferramenta gráfica para **montar um cenário `.edl` do zero**: arrastar classes de uma paleta
completa (todas as factories que `app/src/mixr_factory.cpp` de fato encadeia — `base`/`models`/
`terrain`/`interop::dis`/`linkage`/`recorder`/`simulation`, mais `shared/x*` e os plugins deste
repositório), preencher campos e exportar `.edl` válido — sem precisar ler C++ nem decorar a
gramática do `edl_parser`.

**Escopo desta v1, decidido explicitamente:** só CRIAR. Abrir/editar um `.edl` real já existente
do repositório fica para uma fase futura — o formato de projeto (`.json`, salvo/aberto pela
própria página) é próprio da ferramenta, não é o `.edl` em si.

A raiz do cenário é fixa em alguma classe que implementa `Station` (`Station`/`ClockStation`,
hoje) — a paleta/drag-and-drop e o `<select>` de fallback só oferecem essas; um botão "×" no
próprio nó raiz descarta o cenário inteiro (não há "Station vazia": a raiz É o cenário). Um
slot-lista pode receber tanto uma classe (arrastada/escolhida) quanto um valor de TEXTO simples
(botão "+ texto") — necessário para slots como `TacviewOutput.modelMap`/`typeMap`/`colorMap`,
cujas entradas são strings, não objetos MIXR (ver o comentário de `makeTextLeaf()` em
`edl_builder_core.js`). O botão "Preset: bandit" carrega tudo de `src/poc/dis/bandit/configs/
scenario.edl` **exceto os players** (`simulation.players` fica vazio de propósito) — um ponto de
partida pronto pra arrastar a própria aeronave em cima.

## Arquivos

| arquivo | papel |
|---|---|
| `edl_builder.jsx` | UI (React): paleta, árvore (outline), painel de propriedades |
| `edl_builder_core.js` | lógica PURA (catálogo, compatibilidade de slot, serializador `.edl`) — sem React/JSX, testável em Node puro |
| `edl_builder.test.js` | testes de `edl_builder_core.js` (`node src/ui/edl_builder.test.js`) |
| `edl_catalog.generated.json` | catálogo de classes+slots, gerado por `scripts/extract_execution_chain.py --edl-catalog` |
| `edl_catalog_overrides.json` | curadoria manual pequena (só os slots "referência por nome" que o tipo C++ sozinho não distingue) |
| `compile.js` | builda `edl_builder.jsx` → `edl-builder.html` (React+Babel via CDN, sem bundler) |
| `edl-builder.html` | **gerado** — a página final, autocontida, zero-rede para abrir |

## Build & uso

```bash
make edl-catalog       # gera edl_catalog.generated.json a partir do fonte do MIXR
make edl-builder       # (encadeia edl-catalog) gera edl-builder.html
make open-edl-builder  # abre no navegador
make edl-builder-test  # testes de unidade puros (node, sem MIXR)
```

A primeira execução de `make edl-builder` baixa React/ReactDOM 18.3.1 UMD e instala
`@babel/standalone` em `src/ui/.cache/` (gitignored) — as próximas rodam sem rede nenhuma.
`edl-builder.html` é **committed**, gerado, mesma convenção de `docs/index.html`: abrir a
ferramenta não exige rodar nada antes.

## Validação do `.edl` exportado

Duas camadas, nenhuma delas aqui (ver o topo de cada arquivo para os detalhes):

- **Leve**: `scripts/edl_lint.py` — fábrica/slot desconhecido, ASCII-only, ordem de `plugins:`,
  referência-por-nome pendurada (aviso, não bloqueia).
- **Profunda**: `make edl-check FILE=<arquivo>` — o binário `edlcheck` (`app/src/
  edlcheck_main.cpp`), que reaproveita a MESMA cadeia de factories de produção e chama o parser
  MIXR de verdade, sem as amarras de terreno/frota/`WorldModel` que o `./app` completo exige.

## Por que `src/ui/` e não `docs/`

`docs/` é documentação/visualização do framework (o `index.html` gerado de `doc.jsx`,
read-only, sem capacidade de editar nada) — este editor é uma ferramenta de trabalho de verdade,
por isso mora sob `src/`, ao lado de `poc/`/`rl/`. Não tem `subdir()` em `src/meson.build`
(mesma categoria de `src/poc/rl-training/`: só JS/Python, sem `main.cpp`/`mixr_factory`, fora do
grafo do Meson) — quem builda é o `Makefile` da raiz.
