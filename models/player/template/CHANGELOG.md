# Changelog — `template`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em [`../../README.md`](../../README.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../../tests/guard/check_modelo_estrutura.sh) a
trava (ela descobre projetos por `find`, então este diretório já nasce coberto). **Vale em
dobro aqui**: este diretório é um dos dois pontos de partida copiáveis deste repositório (o outro
é [`../fixtures/stub`](../fixtures/stub/)) — o que faltar nele falta em todo modelo que nascer
dele.

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [`meson.build`](meson.build)** — hoje `0.1.0`. Não existe outra:
não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`../../../libs/xplugin/PluginAbi.hpp`](../../../libs/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — todo commit deste repositório se chama
`up`.

> **Se você copiou este diretório para começar um modelo novo** (ver
> [`docs/PRIMEIROS-PASSOS.md`](docs/PRIMEIROS-PASSOS.md)): apague tudo abaixo e comece um
> `[0.1.0]` (ou a versão que o seu `meson.build` declarar) descrevendo o SEU modelo, não este
> template.

---

## [0.1.0] — 2026-09-05

Nasceu como o segundo ponto de partida copiável de `models/player/` — ao lado de
`fixtures/stub`, mas para o caso oposto: um modelo que vai crescer além de uma decisão única e
quer nascer já separado em camadas.

### Adicionado

- **Um esqueleto mínimo em camadas** (`domain/` → `ubf/` → `xnative/`), com uma única decisão de
  exemplo (`domain::ExampleThreshold`, um Schmitt trigger sobre a altitude do player) percorrendo
  as três de ponta a ponta — percepção (`ExampleState`) → decisão (`ExampleBehavior`, com dois
  slots `base::Distance`) → ação (`ExampleAction`, que escreve no `xboard`).
- **`docs/ARCHITECTURE.md`** — o porquê de cada camada, o porquê de `domain::` morar aninhado em
  `mixr::models::xtemplate` (evita colisão de `type_info` entre plugins carregados juntos — mesmo
  raciocínio de `models/player/missile/src/domain/Guidance.hpp`), a tabela comparando os quatro
  pontos de referência de `models/player/` (`stub`, este `template`, `missile`, `A4`), e o roteiro
  para crescer até uma árvore do BehaviorTree.CPP quando uma regra só deixar de bastar.
- **`docs/PRIMEIROS-PASSOS.md`** — o roteiro mecânico completo: copiar, renomear (projeto, módulo,
  namespace — os quatro lugares que têm que concordar), substituir a decisão de exemplo pela do
  usuário, publicar via `install-host`, e um checklist antes do primeiro commit.
- **`tests/domain/test_ExampleThreshold.cpp`** (4 casos, sem MIXR) e
  **`tests/check_contract.sh`** (a forma do `.so` — 1 símbolo `T`, deps resolvidas; cópia literal
  do de `fixtures/stub`, que já era genérico).
- **`Makefile` autocontido**, no molde de `models/player/missile/Makefile` (mesma profundidade —
  três níveis até a raiz do repositório).
- **Nunca entra no `models:` do Makefile raiz nem em `tests/meson.build`** — de propósito: não é
  produção, nenhum cenário aponta para ele. Ver `docs/PRIMEIROS-PASSOS.md` passo 7 para como
  integrar o modelo que nascer daqui, quando for a hora.
