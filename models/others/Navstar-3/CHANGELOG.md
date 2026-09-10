# Changelog — `Navstar-3`

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

- Primeiro modelo deste repositório na categoria `others` (`models/others/`). Satélite de
  navegação (família GPS/NAVSTAR) em órbita circular ao redor da Terra, corpo `( SpaceVehicle )`
  **nativo** do MIXR, sem nenhum `DynamicsModel` anexado — a órbita é 100% calculada pela camada
  `domain/` deste modelo (`domain::CircularOrbit`, dois corpos, órbita circular) e aplicada via
  `Player::setGeocPosition(ecef, slaved=true)`. Investigado e confirmado no fonte vendorizado
  antes de escrever código: o MIXR não tem mecânica orbital nativa nenhuma
  (`SpaceVehicle`/`SpaceDynamicsModel` são stubs sem física) — ver `docs/ARCHITECTURE.md`, que
  também recupera o prior art removido deste repositório (`poc/10-satellite-constellation`,
  commit `87c6b15`).
- `domain::EclipseGeometry` — teste de sombra cilíndrica (satélite iluminado pelo Sol vs. na
  sombra da Terra), com uma direção de sol fixa e configurável (o MIXR não tem efeméride solar).
  É a única decisão da árvore de comportamento (`SUNLIT`/`ECLIPSE`) — escolhida por ser o único
  estado físico genuíno de duas alternativas que faz sentido para um satélite, em vez de uma
  árvore mínima de rótulo sempre constante.
- `xnative::Navstar3AgentTC` (fase 3 do frame de tempo crítico, adaptado de
  `models/players/paratrooper/src/xnative/ParatrooperAgentTC.cpp`) — nenhuma classe de "corpo
  físico" própria foi necessária (ao contrário de `paratrooper`): `mixr::models::factory`
  despacha `"SpaceVehicle"` nativamente. Quatro classes registradas, todas prefixadas
  `Navstar3*`: `Navstar3State`, `Navstar3BtBehavior`, `Navstar3Action`, `Navstar3AgentTC`.
- Suíte de 4 camadas (domain/tree/native/contract, mesmo padrão de `models/players/paratrooper`)
  — incluindo o round-trip `groundTrack()`↔`eciPosition()`, a deriva do nó ascendente por órbita
  (propriedade real de órbitas GPS: período ≈ metade de um dia sideral), a geometria de sombra em
  quatro configurações, e — na camada `native/` — o `Player` de verdade se movendo entre frames
  sem `Station` nenhuma, via bancada.
- Cenário de demonstração: `src/poc/navstar3-orbit/` — satélite sozinho, hermético. Determinismo
  confirmado com 1200 frames, 1/2/4 threads T/C, dumps byte-idênticos.
  (`tests/determinism/check_determinism.sh`).
- Efeito colateral: `src/ui/scripts/generate_edl_catalog.py` só descobria classes de modelo sob
  `models/players/*` — este foi o primeiro modelo real fora dessa pasta, e expôs a lacuna
  (`dispatch_factory_cpp_paths()`/`origin_of()` generalizados para as três categorias de
  `scripts/models.sh`). Ver `CLAUDE.md` deste diretório.
