# Changelog — `paratrooper`

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

- Corpo físico (`xnative::Paratrooper`, deriva de `mixr::models::Effect`) e decisão (FSM de três
  estágios `FREEFALL`→`CANOPY`→`LANDED`, `domain::ParachuteFsm`, mão única — nunca reverte) de um
  paraquedista, atravessando as quatro camadas (`domain/`→`bt/`→`ubf/`→`xnative/`) que
  `models/players/template` demonstra. `Effect` escolhido em vez de `LifeForm` porque este último é
  auto-grudado ao terreno pelo framework (`Player::positionUpdate()`), errado durante queda
  livre/velame — e porque `Effect` deixa este modelo um substituto de EDL direto para
  `C130ParatrooperPlaceholder` (`models/players/C-130`) numa tarefa futura.
- `xnative::ParatrooperAgentTC` (fase 3 do frame de tempo crítico, adaptado de
  `models/players/C-130/src/xnative/FlightAgentTC.cpp`) e uma rede de segurança contra o
  `CRASH_EVENT` genérico do `Player` (que, sem sobrescrita, cascatearia por `Effect::
  crashNotification()` como uma detonação — dano/fumaça/chamas e `KILL_EVENT`, semanticamente
  errado para um pouso seguro) e contra a expiração de `maxTOF` enquanto pousado. Ver
  `docs/ARCHITECTURE.md` para o detalhe completo de cada decisão.
- Suíte de 4 camadas (domain/tree/native/contract, mesmo padrão de `models/players/C-130`) — 26
  casos ao todo, incluindo a física por estágio, o TOF, e a rede de segurança contra o crash
  genérico medida rodando (bancada sem `Station`, ver `tests/native/`).
- Cenário de demonstração: `src/poc/paratrooper-drop/` — autônomo, não ligado ao C-130 (a
  liberação de verdade a partir de um `StoresMgr` é tarefa futura, adiada de propósito).
