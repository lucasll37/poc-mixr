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

## [Não lançado]

### Adicionado

- **Ponto de saída**: um paraquedista liberado de uma aeronave nasce **15 m atrás e 10 m abaixo**
  dela, e não colado nela (o que o mecanismo nativo faz sozinho, com offset zero). Implementado
  como uma sobrescrita de `Paratrooper::dynamics()` que fixa `initPosition`/`initAltitude` —
  para uma arma em `PRE_RELEASE` esses dois são um deslocamento em eixos do CORPO do lançador,
  não posição no terreno de jogo — imediatamente antes de `AbstractWeapon::dynamics()` lê-los;
  a rotação (fazer "atrás" acompanhar rumo/arfagem/rolamento) fica com o código nativo. Dois
  slots novos, `releaseOffsetAft` e `releaseOffsetBelow` (`<Distance>`, defaults 15 m/10 m),
  copiados em `copyData()` porque quem voa é o `clone()` da estação, não o objeto declarado nela.
  Medido contra o C-130 de `models/players/C-130` liberando pelo `StoresMgr` de verdade: 15,001 m
  atrás e 9,998 m abaixo, contra um controle com os offsets zerados. Sete casos novos em
  `tests/native/test_paratrooper.cpp` (bancada com aeronave lançadora, sem `Station`); os três
  posicionais falham se a sobrescrita for removida, e o do clone falha se o par sair de
  `copyData()` — conferido nos dois sentidos. Não altera `src/poc/paratrooper-drop`, cujos
  paraquedistas são declarados direto em `players: {}` e nunca passam por `PRE_RELEASE`.

## [0.1.0] — 2026-09-10

- Corpo físico (`xnative::Paratrooper`, deriva de `mixr::models::Effect`) e decisão (FSM de três
  estágios `FREEFALL`→`CANOPY`→`LANDED`, `domain::ParachuteFsm`, mão única — nunca reverte) de um
  paraquedista, atravessando as quatro camadas (`domain/`→`bt/`→`ubf/`→`xnative/`) que
  `models/template` demonstra. `Effect` escolhido em vez de `LifeForm` porque este último é
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
