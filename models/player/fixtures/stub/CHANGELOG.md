# Changelog — `stub`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em [../../README.md](../../../README.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../../../tests/guard/check_modelo_estrutura.sh) a
trava. **Vale em dobro aqui**: este diretório é o ponto de partida copiável para um modelo novo
(`cp -r`), então o que falta nele falta em todo modelo que nascer dele.

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [meson.build](meson.build)** — hoje `1.0.0`. Não existe outra:
não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`shared/xplugin/PluginAbi.hpp`](../../../../shared/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — todo commit deste repositório se chama
`up`. As entradas anteriores à criação deste arquivo foram reconstruídas do código e dos
documentos: uma linha por mudança que alguém precisaria saber antes de mexer neste fixture, não
uma por commit.

---

## [Não versionado]

### Adicionado

- **`RLBridgeBehavior` trivial** (`genAction()` devolve sempre `nullptr`) — sétimo nome de
  fábrica. **Não é feature: é manutenção de contrato.** O cenário de PRODUÇÃO ganhou
  `RLBridgeBehavior` no `provides:` quando `models/player/A4` passou a publicá-lo, e `provides:` é
  igualdade EXATA de conjunto contra o que a `.so` exporta — sem esta classe, o stub deixava de
  ser contrato-compatível com o cenário que ele existe para rodar, e
  `plugin-modelo-estranho`/`plugin-deposito-terceiro` quebravam. Nenhum cenário de teste liga a
  ponte de RL de verdade (isso exigiria `shared/xrlbridge`, que o stub não linka): o nome só
  precisa existir e se recusar a decidir. `docs/CONTRATO.md` ganhou a linha correspondente na
  tabela de nomes. (2026-09-03)
- Este `CHANGELOG.md`. (2026-09-03)

### Mudado

- **`make install-host` deposita em `models/plugins/`, nunca mais em `dist/`** — o mesmo depósito
  que um `.so` de terceiro usa; quem sincroniza `models/plugins/` → `dist/lib/mixr-plugins/` é o
  alvo `sync-plugins` do `make install` da raiz. (2026-09-03)

---

## [1.0.0] — 2026-09-02

O fixture nasceu inteiro neste dia, com os dois papéis que justificam ele existir.

### Adicionado

- **Um modelo mínimo e completo (~270 linhas em `src/stub.cpp`), escrito SÓ contra o SDK** — sem
  árvore de comportamento e sem uma linha de `domain/`, registrando os mesmos nomes de fábrica,
  com os mesmos slots, que o cenário de produção nomeia. Voa reto e nivelado: o comportamento não
  é o ponto.
- **O papel que justifica existir: a prova de que o contrato BASTA.** `PluginDescV1` é contrato de
  *empacotamento*; o que a aplicação de fato exige de um modelo (os nomes de fábrica e seus slots,
  as classes base obrigatórias e — a mais fácil de esquecer — o dever de **escrever no `xboard`**)
  estava espalhado e não escrito. O teste `plugin-modelo-estranho` roda o cenário de **produção**
  contra este `.so` trocando **apenas** o `file:` do `( PluginModule )`. É o único teste do
  repositório que pode falhar por *"o contrato não basta"* — todos os outros carregam o mesmo
  modelo compilado do mesmo fonte. Já pagou por si duas vezes na implementação: revelou que
  `Player::getInitHeading()` não existe, e é o que garante que a lista de slots do cenário está
  completa.
- **`docs/CONTRATO.md`** — a lista escrita dessas obrigações, incluindo a única que falha em
  **silêncio**: sem escrever no `xboard`, o host sobe, o cenário parseia, os aviões voam pelo
  `Autopilot` nativo e o dump sai com `bt=--` e `dec=0`, com todos os outros testes verdes.
- **`tests/check_contract.sh`** — a forma do artefato, automatizando os dois critérios que
  `models/README.md` §2.3 documentava à mão: exatamente **um** símbolo `T` exportado e nenhuma
  dependência dinâmica não resolvida. Não afirma nada sobre comportamento (isso é
  `tests/plugin/run_stub_model.py`, na raiz).
- **`Makefile` autocontido** e **`README.md`** — as outras peças obrigatórias à época (o
  `CHANGELOG.md`, a quinta, é desta rodada). **Armadilha
  ao copiar este diretório**: o `Makefile` calcula a raiz do repositório a partir de onde ele
  está (`ROOT := $(abspath ../../..)`, três níveis, por morar em `models/player/fixtures/`); um modelo
  novo criado em `models/meu-modelo/` está só dois níveis abaixo e precisa de
  `ROOT := $(abspath ../..)`, ou o build procura o SDK no diretório **pai** do `poc-mixr`.
- **Fica em `fixtures/`, e não como irmão de `flight`/`missile`, de propósito** — não é um
  terceiro modelo de produção.

### Também usado por

- `plugin-deposito-terceiro` (2026-09-02) — prova que um `.so` que chegou pelo **depósito**
  `models/plugins/` roda de verdade numa simulação. O stub faz o papel do terceiro justamente por
  já ser um modelo válido e por **não** ser nenhum dos plugins que o host consome direto, o que
  isola a variável testada: o caminho por onde o `.so` chegou, não se o modelo é válido.
