# Changelog — `C-130`

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

## [Não versionado]

### Adicionado

- `C130ActionParatrooperStick`: libera **várias** estações em sequência, espaçadas no tempo, a
  partir de um único cruzamento de steerpoint — o "stick" de paraquedistas de verdade, ao lado de
  `C130ActionParatrooperRelease` (que já liberava uma só por cruzamento). Reusa só mecanismo já
  existente do framework (`mixr::models::Action::process()` + `OnboardComputer::actionManager()`
  chamando a cada ciclo de fundo até `isCompleted()`) — nenhum relógio novo foi inventado.
  Cobertura: 7 testes novos em `tests/native/test_paratrooper_stick.cpp`.
- `xnative::releaseNextStoreOfType()` (`ReleaseHelpers.hpp`/`.cpp`): a busca genérica por
  tipo/liberação de estação, extraída de dentro de `ActionParatrooperRelease::trigger()` para ser
  reusada por `ActionParatrooperStick` também — comportamento idêntico, sem mudança de slot/EDL.
- Cenário novo, `sandbox/C-130_paratrooper-6DOF`: **a integração de verdade** com
  `models/players/paratrooper` — um C-130 libera 30 `( Paratrooper )` reais (não mais o
  `C130ParatrooperPlaceholder`) com 1,5 s de intervalo, no segundo ponto de navegação. `provides:`
  de `src/poc/c130-airdrop` e `sandbox/C-130-6DOF` também precisaram do nome novo (igualdade exata
  de conjunto contra o `.so` — nenhuma mudança de comportamento nesses dois cenários).

## [0.1.0] — 2026-09-09

Primeira versão real, sobre o scaffold gerado de `models/players/template` por `scripts/models.sh`:

- Navegação nativa por `Route`/`Steerpoint` (`C130FlightAgentTC` na fase 3 do frame de tempo
  crítico, árvore de um nó só — `Navigate`, sem `Fallback`), mesmo padrão já provado em
  `src/poc/full-systems-nav/`.
- Piloto automático JSBSim próprio (`data/jsbsim/aircraft/C130/c130ap.xml`), autorado do zero —
  o `C130.xml` vendorizado não tem `<autopilot>` nenhum, e sem ele os hold modes do `Autopilot`
  nativo não têm efeito físico algum (ver `docs/ARCHITECTURE.md`).
- `C130ActionParatrooperRelease` + `C130ParatrooperPlaceholder`: a capacidade de liberar um
  paraquedista a partir de um `Steerpoint`, tratada como liberação de ARMA (família
  `AbstractWeapon`/`Effect`), não como `Player` autônomo — o mecanismo genérico que o futuro
  `models/players/paratrooper` reaproveita trocando só o EDL.
- Cenário de demonstração: `src/poc/c130-airdrop/`.
- **Corrigido, mesmo dia**: o par motor/thruster vendorizado originalmente copiado para
  `data/jsbsim/engine/` (`t56.xml`, `<turbine_engine>`, pareado com `t56_prop.xml`, um
  `<propeller>` — tipos incompatíveis) fazia o empuxo efetivo ficar perto de zero — a aeronave
  desacelerava sob arrasto até estolar e colidir com o terreno (achado só depois de rodar por
  tempo suficiente; os testes curtos da primeira versão não pegaram). Trocado o thruster para
  `direct` (mesmo padrão já usado por `models/players/A-4`'s `J52`, também `<turbine_engine>`) —
  ver o achado completo no cabeçalho de `data/jsbsim/aircraft/C130/c130ap.xml`. Com o motor
  corrigido, rumo/altitude/velocidade se sustentam por voos longos (600 s simulados testados sem
  crash em `c130-airdrop` e em `sandbox/C-130-6DOF`) — a malha de altitude ainda tem folga/atraso
  não perfeitamente calibrado, por isso `c130-airdrop` usa um perfil de altitude achatado (sem
  descida/subida) em vez de arriscar terreno próximo num ponto baixo.
