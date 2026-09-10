# `navstar3-orbit` — o modelo `Navstar-3`, sozinho

## O que esta poc demonstra

O modelo `models/others/Navstar-3` — um satélite de navegação (família GPS/NAVSTAR) orbitando a
Terra em órbita circular, corpo `( SpaceVehicle )` **nativo** do MIXR, sem nenhum
`DynamicsModel` anexado. A posição é 100% calculada pela camada `domain/` do modelo
(`domain::CircularOrbit`) e aplicada via `Player::setGeocPosition(ecef, slaved=true)` a cada
ciclo de decisão — ver `models/others/Navstar-3/docs/ARCHITECTURE.md` para o porquê (o MIXR não
tem mecânica orbital nativa nenhuma) e o desenho completo.

Elementos orbitais default: ~20.180 km de altitude, 55° de inclinação, período ~12h — valores
representativos de uma órbita GPS/MEO moderna, não uma reconstrução histórica do Navstar-3 real
(SVN-3, 1978, sem efeméride precisa preservada). `sunRightAscension: 180` faz o satélite nascer
na sombra da Terra (rótulo `ECLIPSE`) — a transição para `SUNLIT` acontece por volta de
`t=1700s` de tempo simulado (ver `models/others/Navstar-3/tests/domain/` para a mesma geometria
testada isoladamente).

## Rodar

```bash
./build/app/src/app -folder src/poc -scenario navstar3-orbit                       # interativo
./build/app/src/app -folder src/poc -scenario navstar3-orbit -deterministic 3000   # headless
tests/determinism/check_determinism.sh ./build/app/src/app navstar3-orbit 1200 "" \
   src/poc/navstar3-orbit/configs/scenario_navstar3_orbit.edl.in
```

Porta Tacview **1243**. Sem `terrain:`, sem `networks:` (hermética) — roda ao lado das demais
pocs sem colidir. `fastForwardRate: 60` acelera o modo interativo/real-time (o período orbital
de ~12h levaria 12h reais sem isso); `-deterministic` não é afetado por esse slot (chama
`tcFrame()` direto).

**Não usar `-f`**: esse caminho assume a frota fixa `falcon1..4` (`app::adHocScenario()`), que
não existe aqui — sai com `"player 'falcon1' nao encontrado!"`. `-folder` descobre a frota em
runtime (`app::discoverFleet()`), o único caminho que funciona para um cenário sem `AirVehicle`
nenhum.

## Medido rodando (3000 frames, 60 s simulados, `-deterministic`)

1. **Altitude HAE constante** — `alt=20180000.000000000` em toda amostra, com ruído de ponto
   flutuante da ordem de `4e-9` relativo (`20179999.999999996` a `20180000.000000004`) —
   consistente com o round-trip exato de `convertGeod2Ecef()`/`convertEcef2Geod()` documentado
   em `docs/ARCHITECTURE.md`. Circularidade por construção, não medida — o teste de regressão
   real está em `tests/domain/`.
2. **Posição avançando suavemente**: `n`/`e` (coordenadas locais NED, relativas ao ponto de
   referência do cenário) crescem monotonicamente frame a frame — `n=1536.75 e=138.22` em
   `frame=100` até `n=45661.06 e=4107.56` em `frame=3000` — o satélite está de fato se movendo,
   não preso num ponto fixo.
3. **`spd=3181 m/s`, não os ~3874 m/s de `v=√(μ/a)`** — a velocidade no dump é relativa ao
   referencial ECEF (fixo-na-Terra, em rotação), não a velocidade orbital inercial. Ver
   `docs/ARCHITECTURE.md` do modelo — o número bate com o cálculo à mão a partir da decomposição
   leste/norte da velocidade inercial no cruzamento do equador, menos a velocidade de
   corrotação da Terra.
4. **`dec` avança na mesma taxa que `frame`** — `dec=101` em `frame=100`, `dec=3001` em
   `frame=3000` (offset de 1 pelo warm-up tick que `StationBuilder` dispara logo após o
   `RESET_EVENT`, mesmo comportamento documentado para as demais pocs deste repositório).
5. **`bt=ECLIPSE` em toda amostra dos 60 s simulados** — esperado: a transição para `SUNLIT`
   não acontece antes de `t≈1700s`, muito além da janela desta execução curta. O ciclo completo
   `ECLIPSE→SUNLIT→ECLIPSE` ao longo de uma órbita inteira (`~43073 s`) é medido com `dt`
   sintético grande em `models/others/Navstar-3/tests/native/test_navstar3.cpp` (mais rápido que
   rodar o cenário de verdade por 12h simuladas), não nesta poc.
6. **Tacview**: com `typeMap: { NAVSTAR-3: "Misc+Satellite" }`/`colorMap: { NAVSTAR-3: "Blue" }`
   /`modelMap: { NAVSTAR-3: "GPS-Block-IIF" }`, a gravação sai com `Name=GPS-Block-IIF,
   Type=Misc+Satellite,Color=Blue` — confirmado lendo o `.acmi` gerado por `./app
   -deterministic`. **Rodando via `./dist/bin/node` (o runner headless) a identidade sai com os
   valores DEFAULT** (`Type=Misc,Color=Violet`) em vez dos mapeados — `node` não chama
   `TacviewOutput::publishIdentities()` (só `./app` chama), e `REID_PLAYER_DATA` sozinho não
   carrega `ac_type`/`side` suficientes para resolver o mapa na primeira aparição. Não é um bug
   do modelo/cenário — ver `models/others/Navstar-3/CLAUDE.md`.
7. **Determinismo**: `tests/determinism/check_determinism.sh` sobre 1200 frames — dumps
   byte-idênticos com 1, 2 e 4 threads T/C (mais a repetição de 4), uma decisão por frame.
8. **Fumaça real (real-time, ~8s de parede, `fastForwardRate: 60`)**: o servidor Tacview sobe em
   `0.0.0.0:1243`, e `data/recordings/mission.acmi` cresce (~30 KB em 8 s) — a cadeia
   DataRecorder→TacviewOutput drena de verdade para um `( SpaceVehicle )`, não só para
   `AirVehicle`.

## Por que não entra na lista `pocs` de `tests/meson.build`

O vocabulário desta poc (`SUNLIT`/`ECLIPSE`) não bate com o vocabulário de combate
(`EVADE`/`SUPPORT`/`RTB`) que `tests/scenario/run_scenario_test.py` afirma — mesmo precedente já
usado por `onnx-policy`/`c130-airdrop`/`paratrooper-drop`. Coberta diretamente por
`tests/determinism/check_determinism.sh` com o caminho do cenário explícito (ver "Rodar" acima).

## Ler também

- [`../../../models/others/Navstar-3/docs/ARCHITECTURE.md`](../../../models/others/Navstar-3/docs/ARCHITECTURE.md)
  — todas as decisões de design do modelo, inclusive o veredito de que o MIXR não tem mecânica
  orbital nativa e o prior art recuperável do histórico de git
- [`../../../models/others/Navstar-3/CLAUDE.md`](../../../models/others/Navstar-3/CLAUDE.md) —
  gotchas não-óbvios encontrados escrevendo este modelo (inclusive a diferença entre testar via
  `./app` e via `./dist/bin/node`)
