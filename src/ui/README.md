# `src/ui/` — editor visual de cenários EDL

Aplicação **React + Express**, autocontida, para montar/editar visualmente os arquivos
de cenário `.epp`/`.epp.in` que as pocs deste repositório consomem — um mapa
georreferenciado pra posicionar players e uma paleta de elementos (aviões, mísseis, DIS,
mensagens, terreno, joystick, Tacview) pra montar a árvore do cenário sem escrever EDL à
mão. **É o primeiro código JavaScript/Node do repositório** — CLAUDE.md, na raiz, tem o
contexto completo do projeto MIXR que este editor serve; leia-o primeiro se for mexer
aqui.

## Por que fora do Meson

Não existe precedente de subprojeto JS wireado ao build C++ deste repositório — o único
precedente de ferramenta não-compilada é `tools/joystick_mapper.py`, deliberadamente fora
do grafo do Meson/Makefile. `src/ui/` segue o mesmo princípio: projeto Node
**autocontido**, rodado via `npm`, nunca um `subdir()` do `meson.build` raiz. O `Makefile`
raiz só ganhou quatro alvos finos (`ui-install`, `ui-dev`, `ui-build`, `ui-test`) que
apenas chamam `npm --prefix src/ui ...` — toda a lógica de verdade mora aqui.

## Rodar

```bash
npm install        # ou: make ui-install, na raiz do repo
npm run dev         # frontend (Vite, :5173) + backend (Express, :5175) juntos
```

Abra `http://localhost:5173`. O Vite faz proxy de `/api/*` pro backend em dev; em
produção (`npm run build && npm start`) é o mesmo processo Express que serve os
estáticos buildados **e** as rotas `/api`.

## Decisão de projeto: só-leitura + exportar

O backend (`server/`) **nunca escreve no working tree**. Ele só lê, pra alimentar a UI
com dados reais do repositório — nunca inventados:

- `GET /api/scenarios` — lista os `.epp`/`.epp.in` reais em `src/poc/*/configs/` e
  `app/configs/`, pra importar/editar um cenário existente.
- `GET /api/scenarios/content?path=...` — conteúdo de um desses arquivos.
- `GET /api/scenarios/fragment?name=...` — fragmentos `@include:` do `app/`
  (`app/configs/fragments/`).
- `GET /api/aircraft` — aeronaves JSBSim disponíveis, lidas de
  `models/plugins/data/flight/jsbsim/aircraft/` (hoje: `c310`, `aim1`).
- `GET /api/terrain-tiles` e `GET /api/terrain-tiles/:nome/contours` — tiles SRTM de
  `shared/data/terrain/srtm/*.hgt.gz` e as curvas de nível derivadas deles (ver abaixo).
- `GET /api/behavior-tree` — `flight_tree.xml` (referência, só-leitura — a árvore de
  comportamento em si não é editável por este app).

O cenário editado nunca é salvo de volta no repositório automaticamente: o botão
**exportar .epp** gera o texto EDL e baixa como arquivo — o usuário decide onde colar.
Essa é uma decisão deliberada de v1 (evita o risco de sobrescrever um `.epp` de produção
por engano); uma versão futura poderia oferecer gravação direta, mas exigiria confirmação
explícita.

## O motor EDL (`web/src/edl/`)

O núcleo tecnicamente mais arriscado do projeto: um lexer + parser recursivo-descendente
que lê um `.epp`/`.epp.in` real para uma árvore tipada (`ScenarioNode`/`SlotValue`, em
`web/src/model/`) e um serializer que devolve texto EDL válido.

**Achado de projeto, documentado em `edl/parser.ts`**: resolver se um `{ }` do EDL é uma
lista posicional (`{ a b c }`) ou um mapa nomeado (`{ chave: valor }`) **não exige**
consultar o catálogo de schema em tempo de parse — é sempre derivável da própria sintaxe,
item a item (tem `chave:` na frente ou não). "Todos com chave" vira mapa, "nenhum com
chave" vira lista, "vazio" é indiferente (os dois formatos serializam de volta como
`{ }`); só "alguns com, alguns sem" no MESMO `{ }` é ambiguidade de verdade — e nunca
acontece nos `.epp` reais do repositório, então vira erro de parse com localização em vez
de adivinhar. Isso significa que o parser funciona para **qualquer** `factoryName`,
catalogado ou não — nunca descarta ou achata o que não reconhece; o catálogo de schema
(`web/src/schema/`) entra só DEPOIS do parse, pra anotar quais nodes estão fora dele
(ainda visualizáveis/reexportáveis, só sem edição guiada por formulário) e pra dar
significado semântico extra (unidade, referência a player, onde cada elemento pode ser
inserido).

Testado com round-trip semântico (importar → serializar → reimportar → comparar a árvore
tipada, ignorando formatação) contra os `.epp`/`.epp.in` **reais** do repositório —
`src/poc/single-thread`, `src/poc/multi-thread`, `src/poc/bandit-dis` e
`app/configs/scenario_intercept_missile.epp.in` (com `@include:` resolvido) — ver
`web/src/edl/__tests__/roundtrip.test.ts`. `npm test` roda essa suíte e mais: guarda
ASCII do serializer, ordenação de `PluginLoader`, leitura binária dos 4 tiles SRTM reais,
e conversão lat/lon ↔ offset NM.

## Limitações conhecidas (v1)

- **Comentários do arquivo original não são preservados** ao reexportar — o foco é
  edição estrutural, não prosa. Reimportar um `.epp` bem comentado e reexportar perde os
  comentários.
- **Formatação numérica exata não é preservada** (`2.0` pode virar `2`) — só a unidade
  escolhida no arquivo original é preservada; o valor numérico é o mesmo.
- **`@include:`/`@TOKEN@`** (templating específico do `app/`, não faz parte da gramática
  EDL de verdade) é resolvido só para `@include:` (busca o fragmento via
  `/api/scenarios/fragment`); outros `@TOKEN@` soltos (`@NUM_TC_THREADS@`,
  `@SCENARIO_ID@`...) passam batom como identifiers literais — a UI não os resolve nem
  os interpreta como número.
- **Catálogo de schema é incremental e mantido à mão** (`web/src/schema/*.schema.ts`) —
  cobre os grupos e slots vistos nos cenários reais do repositório hoje; um slot fora
  dele ainda é preservado no round-trip, só não ganha um campo de formulário dedicado.
- **A tabela de "quais fábricas nativas cada binário-alvo encadeia"** (avisada no
  inspector ao adicionar DIS/terreno/joystick) é estática, mantida à mão — precisa de
  atualização manual se um novo binário/fábrica for adicionado ao repositório.
- **Tile de mapa base é o servidor público do OpenStreetMap** — aceitável pra uso
  interno de baixo volume desta PoC; uso pesado ou produção de verdade deveria trocar por
  um tile server próprio (ex: MapTiler com API key).
- **O bundle de produção é grande** (~1 MB minificado, principalmente MapLibre GL JS) —
  aceitável para uma ferramenta interna, não otimizado para carregamento público.

## Estrutura

```
src/ui/
├── web/src/
│   ├── model/           # ScenarioNode/SlotValue (a arvore tipada) + treeOps (navegacao/mutacao)
│   ├── schema/           # catalogo de fabricas EDL conhecidas -- a "fonte da verdade" (ver acima)
│   ├── edl/              # lexer + parser + serializer + ordering (PluginLoader) + guarda ASCII
│   ├── store/            # Zustand -- o documento do cenario, selecao, topologia, historico
│   ├── components/
│   │   ├── map/          # MapLibre + curvas de nivel + conversao lat/lon <-> NM
│   │   ├── tree/          # arvore navegavel do cenario
│   │   ├── inspector/     # formulario schema-driven pro node selecionado
│   │   ├── palette/       # abas de elementos disponiveis pra inserir
│   │   └── topology/      # seletor single-thread/multi-thread/sem-decisao + assistente de agente
│   └── api/client.ts     # chamadas as rotas /api
└── server/src/
    ├── routes/            # aircraft, terrain, scenarios, behaviorTree -- todas SO-LEITURA
    └── srtm/              # decodificacao binaria .hgt.gz (zlib, sem escrever em disco) + curvas de nivel
```

## Testes de verdade, sem servidor: verificação manual

1. `npm run dev`, abra `http://localhost:5173`.
2. Importe `app/configs/scenario_intercept_missile.epp.in`.
3. Confira no mapa: falcon1..4 nas posições relativas certas ao redor do ponto de
   referência (Serra do Mar / Nova Friburgo), bandit1 mais distante.
4. Selecione `falcon1`, confira os slots de `BtBehavior` com as unidades certas.
5. Arraste o marcador de `falcon2`; `initXPos`/`initYPos` atualizam no inspector.
6. Exporte e compare com o original (`diff`) — só a posição de `falcon2` deve mudar.
