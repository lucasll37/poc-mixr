# Changelog — `missile`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em [`../../README.md`](../../README.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../../tests/guard/check_modelo_estrutura.sh) a
trava (ela descobre projetos por `find`, então este diretório já nasce coberto).

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [`meson.build`](meson.build)** — hoje `0.1.0`. Não existe
outra: não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`../../../libs/xplugin/PluginAbi.hpp`](../../../libs/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — ver
[`CONTRIBUTING.md`](../../../CONTRIBUTING.md), na raiz do repositório, para a convenção de
mensagem de commit em uso.

---

## [0.1.0] — 2026-09-10

Primeira decisão real deste modelo, gerado a partir de `models/players/template` via
`scripts/models.sh`: `( GuidedMissile )`, subclasse cinemática de `mixr::models::Missile`
(factory nativa) — sem `dynamicsModel`, guiagem por navegação proporcional
(`domain::proportionalNavigation()`) e espoleta de proximidade
(`domain::proximityFuze()`) escritas em `domain/`, puras e testadas sem MIXR.

As camadas `bt/`/`ubf/` do scaffold de origem (`models/players/template/`) saíram: guiagem de
míssil é controle contínuo, não decisão discreta — nem o `mixr::models::Missile` nativo usa
árvore de comportamento para isto. Ver `docs/ARCHITECTURE.md`.

Exercício documentado em `TODO.md` (raiz): "implementar um míssil ... para verificar como
isso é modelado em termos de uso idiomático do MIXR". Disparado pelo cenário
`sandbox/A4-6DOF-MISSILE/` — um A-4 armado detecta outro A-4 pelo radar e dispara.
