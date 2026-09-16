# Changelog — `missile`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em [`../../../README.md`](../../../README.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../../../tests/guard/check_modelo_estrutura.sh) a
trava (ela descobre projetos por `find`, então este diretório já nasce coberto).

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [`meson.build`](meson.build)** — hoje `0.1.0`. Não existe
outra: não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`../../../../libs/xplugin/PluginAbi.hpp`](../../../../libs/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — ver
[`CONTRIBUTING.md`](../../../../CONTRIBUTING.md), na raiz do repositório, para a convenção de
mensagem de commit em uso.

---

## [0.1.0] — 2026-09-13

**Corrigido: `GuidedMissile` não semeava `cmdHeadingRad_`/`cmdPitchRad_`/`cmdSpeedMps_` na
transição PRE_RELEASE→ACTIVE**, achado investigando um relato de "o míssil passa do lado do
alvo e não acontece nada" em `sandbox/A4-6DOF-MISSILE`. `weaponDynamics()` já roda todo frame
independente do TSG (`AbstractWeapon::dynamics()` não tem gate nenhum por TSG na chamada — só
`weaponGuidance()` consulta `isGuidanceEnabled()`), então enquanto `tof < tsg` (1,0 s) os três
campos ficavam no inicializador de classe (`0.0`) e o míssil guinava ativamente para
rumo/pitch **geográfico zero** (Norte, nivelado) e desacelerava em direção a **zero m/s**, no
turn-rate/aceleração máximos, antes de a navegação proporcional assumir — até ~66° de rumo e
uma fração relevante da velocidade perdidos logo no primeiro segundo de voo, medido no próprio
cenário via a gravação Tacview (`data/recordings/mission_*.acmi`). `mixr::models::Missile`
nativo evita exatamente isto em `atReleaseInit()` (semeia `cmdPitch`/`cmdHeading`/`cmdVelocity`
com a atitude/velocidade de lançamento) — mas esses são campos PRÓPRIOS de `Missile`, nunca
lidos por `GuidedMissile::weaponDynamics()`, que consome campos próprios homônimos.
`GuidedMissile::atReleaseInit()` (novo override) semeia os três campos certos
(`getHeadingR()`/`getPitchR()`/`getVpMax()`). Determinismo confirmado inalterado (o
míssil continua convergindo ao alvo na mesma fixture, agora sem a manobra inicial
desperdiçada).

Ganhou também o único ponto de observabilidade do desfecho do disparo: `LOG(INFO/WARNING)` no
instante em que a espoleta de proximidade dispara, com o alcance de menor aproximação e
acerto/erro (`xnative/GuidedMissile.cpp`) — sem isso, nem o Tacview (o REID de detonação fica
fora do `enabledList`, mesma armadilha do `REID_WEAPON_RELEASED`) nem o alvo (nenhum dano
visível) davam qualquer sinal de que algo aconteceu; um acerto e um erro pareciam idênticos na
tela. A linha de log de lançamento em
`models/players/air/A-4/src/ubf/FlightAction.cpp` (existente, comentada) foi reativada pelo mesmo
motivo.

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
