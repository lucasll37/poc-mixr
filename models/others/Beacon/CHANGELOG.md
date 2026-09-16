# Changelog — `Beacon`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em [`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md); a
guarda [`../../../tests/guard/check_modelo_estrutura.sh`](../../../tests/guard/check_modelo_estrutura.sh)
a trava (ela descobre projetos por `find`, então este diretório já nasce coberto).

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [`meson.build`](meson.build)** — hoje `0.1.0`. Não existe
outra: não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`../../../libs/xplugin/PluginAbi.hpp`](../../../libs/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — ver
[`CONTRIBUTING.md`](../../../CONTRIBUTING.md), na raiz do repositório, para a convenção de
mensagem de commit em uso.

---

## [0.1.0] — 2026-09-13

Gerado a partir de `models/template/` por `scripts/models.sh` (`make new-model NAME=Beacon
CATEGORY=player`), depois reduzido na mão: sem `domain/`/`bt/`/`ubf/` (o propósito deste modelo
não é decisão em camadas), sem árvore de comportamento, sem `BehaviorTree.CPP` — só a classe
`Beacon` (`include/Beacon.hpp` + `src/Beacon.cpp`), a factory (`xnative/factory.{hpp,cpp}`, mesmo
desenho de `models/players/air/A-4`) e a fronteira de plugin (`src/plugin.cpp`).

### Adicionado

- `mixr::models::xBeacon::Beacon` — herda `mixr::models::Player` direto (mesmo padrão mínimo de
  `mixr::models::Building`). Slots `pingInterval` (`Time`, default 5s) e `pingMessage` (`String`,
  default `"ping"`).
- **Emite** `events::EID_PING`/`events::PingMessage` (`../../events/payloads/EID_PING/`, novo
  nesta mudança) periodicamente, na fase de fundo (`updateData()`), para os demais `Beacon`s
  locais ativos do cenário — broadcast direto (`Component::event()` sobre `getPlayers()`), mesmo
  caminho (b) de `xnative::AlertDatalink::broadcastAlert()` (`models/players/air/A-4`), sem depender
  de nenhum subsistema nativo (`Datalink`, `RfSensor`, ...).
- **Trata** o mesmo evento (`onPingEvent()`, registrado via `BEGIN_EVENT_HANDLER`/`ON_EVENT_OBJ`)
  — a mesma classe nos dois papéis, o primeiro caso deste repositório em que isso acontece (ver
  `../../events/README.md`, "Segundo caso de referência").
- `tests/native/test_beacon.cpp` — dois `Beacon` de bancada (`WorldModel`, sem `Station`)
  trocando ping de verdade: emissão no primeiro tick, contagem nos dois sentidos, campos do
  payload, e reemissão só após o intervalo configurado.
- Cenário de demonstração: [`../../../src/poc/my-event/`](../../../src/poc/my-event/).
