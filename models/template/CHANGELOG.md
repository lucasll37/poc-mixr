# Changelog — `template`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em
[`../../.claude/rules/models-plugin.md`](../../.claude/rules/models-plugin.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../tests/guard/check_modelo_estrutura.sh) a
trava (ela descobre projetos por `find`, então este diretório já nasce coberto). **Vale em
dobro aqui**: este diretório é o único ponto de partida copiável deste repositório — o que faltar
nele falta em todo modelo que nascer dele.

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [`meson.build`](meson.build)** — hoje `0.1.0`. Não existe outra:
não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`../../libs/xplugin/PluginAbi.hpp`](../../libs/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — todo commit deste repositório se chama
`up`.

> **Se você copiou este diretório para começar um modelo novo** (ver
> [`docs/PRIMEIROS-PASSOS.md`](docs/PRIMEIROS-PASSOS.md)): apague tudo abaixo e comece um
> `[0.1.0]` (ou a versão que o seu `meson.build` declarar) descrevendo o SEU modelo, não este
> template.

---

## [Não versionado]

### Adicionado

- **Uma camada `bt/` de verdade — o template passou a decidir por árvore de comportamento, não
  mais por um `if`.** Dois nós (`( ExampleThreshold )`, condição, e `( ExampleLabel )`, ação),
  registrados em `src/bt/bt_factory.cpp`, mais a árvore em `configs/example_tree.xml` (um
  `Fallback` de dois ramos, a mesma forma do `flight_tree.xml` de produção). `ExampleBehavior`
  carrega o XML pelo slot novo `treeFile:` e tica uma vez por ciclo; a regra pura
  (`domain::ExampleThreshold`) **continua existindo e continua testada sem BT nenhum** — quem a
  avança agora é o nó. Sem árvore carregada o modelo degrada para a regra direta, em vez de
  parar de decidir. Os nós ficam livres do MIXR pela interface `bt/DecisionContext.hpp`, o que
  permite a suíte nova `tree` carregar a árvore de PRODUÇÃO contra um contexto falso.
- **Os alvos `create-bt`, `update-bt` e `open-groot`** — os três que só `A-4` tinha. Com eles
  vieram `tools/dump_tree_model.cpp` (gera o `<TreeNodesModel>` que o Groot exige) e
  `tools/update_bt_models.py`. O `Makefile` deste projeto passa a ter os **mesmos 8 alvos** do
  `A-4`, e todo modelo gerado por `make new-model` nasce com eles.
- **Duas camadas novas na suíte**: `tree` (a árvore de produção contra um `FakeContext`) e
  `tree-model-sync` (guarda: registrar um nó e esquecer o `make update-bt` deixaria o Groot
  recusando a árvore, sem nenhum teste vendo). A guarda de contagem do `make test` subiu de 2
  para 4 — o valor vale tanto aqui quanto num scaffold copiado, onde o teste do mirror não
  existe.

### Corrigido

- **`tools/update_bt_models.py` destruía uma árvore cujo comentário MENCIONASSE
  `<TreeNodesModel>`** (achado rodando, com perda real do arquivo). A regex do bloco não conhece
  comentário: casava a partir da menção e, com `re.DOTALL`, engolia o resto do comentário, o
  `-->`, o `<root>` e a `<BehaviorTree>` inteira — reportando "substituído", sem erro. Os
  comentários agora são mascarados antes da busca (mesma técnica de
  `tools/mixr_source_scan.py::mask_source()` na raiz). O mesmo defeito existia, latente, na cópia
  de `A-4` — corrigido nas duas. Ver `../players/A-4/CHANGELOG.md`.

### Mudado

- **Saiu de `models/players/template/` para `models/template/`** — o template nunca foi um
  *player*: é o ponto de partida de um modelo de QUALQUER categoria (`player`/`system`/`others`),
  e morar dentro de uma delas sugeria o contrário. `git mv`, histórico preservado. O que precisou
  acompanhar, além do caminho literal: `ROOT := $(abspath ../..)` (dois níveis, não três) e a
  profundidade de todo link relativo deste diretório; o glob de `dispatch_factory_cpp_paths()` em
  `src/ui/scripts/generate_edl_catalog.py`, que só varria `models/<categoria>/*/` e deixaria as
  três classes de exemplo caírem para `concrete: false` **em silêncio** (some da paleta do editor
  EDL e do `edl_lint.py` — conferido nos dois sentidos); e `tests/guard/check_colisao_fabrica.py`,
  que agora precisa pular `template` também no nível de CATEGORIA. `MODELOS_PRODUCAO` e
  `check_modelo_estrutura.sh` não precisaram de nada — os dois já descobriam por `find` e o filtro
  `*/template/*` continua valendo na profundidade nova. `scripts/models.sh` ganhou um passo que
  reperfila a profundidade dos links dos `.md` copiados: sem ele, todo modelo gerado por
  `make new-model` nascia com 15 links quebrados, porque o destino (`models/<categoria>/<nome>/`)
  está um nível mais fundo que o template. (2026-09-10)

- **`install`/`install-host` deixaram de nomear o diretório de dados.** `make new-model` renomeia
  o literal `'template'` dentro do `meson.build` (então o `install_data` passa a escrever em
  `share/mixr-plugins/<seu-modelo>/`) mas não toca no `Makefile` — um caminho com `template`
  cravado ali quebrava o primeiro `install-host` do scaffold copiado com *"cp: cannot stat"*
  (reproduzido). Os dois alvos passaram a copiar o diretório inteiro, sem nome nenhum escrito.

- **O diretório pai passou de `models/player/` para `models/players/`** — este projeto passou a
  morar em `models/players/template/` (hoje `models/template/`, ver a entrada acima). `git mv`,
  histórico preservado. Detalhe da varredura →
  [`../players/A-4/CHANGELOG.md`](../players/A-4/CHANGELOG.md). (2026-09-07)

---

## [0.1.0] — 2026-09-05

Nasceu como o segundo ponto de partida copiável de `models/players/` — ao lado de
`fixtures/stub`, mas para o caso oposto: um modelo que vai crescer além de uma decisão única e
quer nascer já separado em camadas.

### Adicionado

- **Um esqueleto mínimo em camadas** (`domain/` → `ubf/` → `xnative/`), com uma única decisão de
  exemplo (`domain::ExampleThreshold`, um Schmitt trigger sobre a altitude do player) percorrendo
  as três de ponta a ponta — percepção (`ExampleState`) → decisão (`ExampleBehavior`, com dois
  slots `base::Distance`) → ação (`ExampleAction`, que escreve no `xboard`).
- **`docs/ARCHITECTURE.md`** — o porquê de cada camada, o porquê de `domain::` morar aninhado em
  `mixr::models::xtemplate` (evita colisão de `type_info` entre plugins carregados juntos — mesmo
  raciocínio de `models/players/missile/src/domain/Guidance.hpp`), a tabela comparando os quatro
  pontos de referência de `models/players/` (`stub`, este `template`, `missile`, `A-4`), e o roteiro
  para crescer até uma árvore do BehaviorTree.CPP quando uma regra só deixar de bastar.
- **`docs/PRIMEIROS-PASSOS.md`** — o roteiro mecânico completo: copiar, renomear (projeto, módulo,
  namespace — os quatro lugares que têm que concordar), substituir a decisão de exemplo pela do
  usuário, publicar via `install-host`, e um checklist antes do primeiro commit.
- **`tests/domain/test_ExampleThreshold.cpp`** (4 casos, sem MIXR) e
  **`tests/check_contract.sh`** (a forma do `.so` — 1 símbolo `T`, deps resolvidas; cópia literal
  do de `fixtures/stub`, que já era genérico).
- **`Makefile` autocontido**, no molde de `models/players/missile/Makefile` (mesma profundidade —
  três níveis até a raiz do repositório).
- **Nunca entra no `models:` do Makefile raiz nem em `tests/meson.build`** — de propósito: não é
  produção, nenhum cenário aponta para ele. Ver `docs/PRIMEIROS-PASSOS.md` passo 7 para como
  integrar o modelo que nascer daqui, quando for a hora.
