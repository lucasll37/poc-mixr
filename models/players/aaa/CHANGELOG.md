# Changelog — `aaa`

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

## [0.1.0] — 2026-09-11

### Adicionado

Primeira decisão real deste modelo (exercício de `TODO.md`: "implementar uma antiaérea com domo
que dispara o mesmo míssil ... que entra no seu raio de atuação"), gerado a partir de
`models/template` por `scripts/models.sh` e depois preenchido camada por camada.

- **`domain::Dome`/`inDome()`** (`include/domain/DomePolicy.hpp`) — regra pura: um alcance cai
  dentro do domo quando está em `[minRangeM, maxRangeM]`, os dois limites inclusivos.
- **`bt::TargetInDomeCondition`/`FireMissileAction`/`WatchAction`** — a árvore de dois ramos
  (`configs/aaa_tree.xml`): dispara se houver alvo no domo e munição disponível, senão observa.
  `Watch` existe para a árvore sempre produzir uma decisão observável (`bt=WATCHING`), mesmo sem
  alvo — a mesma obrigação de nunca deixar `xboard` sem escrita (`models/template/docs/CONTRATO.md`,
  seção 3).
- **`ubf::AaaState`** — percepção: consulta o radar de aquisição próprio da antiaérea via
  `xtrack::nearestHostileTrack(site, "aaaTrkMgr")` (generalização de
  `libs/xtrack::nearestHostileTrack()`, feita nesta mesma passada — ver
  `libs/xtrack/README.md`) e lê `SamVehicle::getMinLaunchRange()`/`getMaxLaunchRange()` (nativos)
  e `StoresMgr::available()`.
- **`ubf::AaaBehavior`** — carrega/tica a árvore (slot `treeFile`), mesmo padrão em camadas do
  A-4 (`BtBehavior`), reduzido a uma regra única.
- **`ubf::AaaAction`** — dispara de verdade: `Player::getStoresManagement()` →
  `StoresMgr::releaseOneMissile()` → `AbstractWeapon::setTargetPlayer(alvo, true)` → `unref()`,
  a MESMA sequência já usada pelo lançador do A-4 (`FlightAction.cpp`), reusando
  `xmissile::GuidedMissile` (`models/players/missile`) **sem nenhuma modificação** — confirmado
  nesta mesma sessão que o míssil não tem nenhum acoplamento ao tipo do lançador.
- **`xnative::AaaSite`** — o `Player` estacionário, subclasse de `mixr::models::SamVehicle` (só
  para herdar os slots nativos `minLaunchRange`/`maxLaunchRange`, o "domo"). **Achado por
  auditoria, documentado em `docs/ARCHITECTURE.md`**: `SamVehicle::isLauncherReady()`/
  `getNumberOfMissiles()` contam munição via `dynamic_cast<const Sam*>`, e `GuidedMissile` é
  IRMÃ de `mixr::models::Sam` (as duas derivam de `Missile` diretamente) — os dois acessores
  nativos ficam sempre falso/zero com um `( GuidedMissile )` no `stores:`, em silêncio.
  `ubf::AaaState` usa `StoresMgr::available() > 0` diretamente por causa disso.
- **Sem subclasse de `AgentTC`** — o `.edl` do cenário declara `( UbfAgent state: (AaaState)
  behavior: (AaaBehavior) )` nativo direto (decide à taxa de fundo, 10 Hz — de sobra para um alvo
  a ~100 m/s entrar no domo). Ver `docs/ARCHITECTURE.md` para o porquê completo.
- Decisão de arquitetura (BT/UBF completo em vez de uma classe `xnative` mínima) confirmada com o
  usuário via `AskUserQuestion` antes de implementar — ver `docs/ARCHITECTURE.md`.
