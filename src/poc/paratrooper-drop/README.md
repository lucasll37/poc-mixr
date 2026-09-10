# `paratrooper-drop` — o modelo `paratrooper`, sozinho

## O que esta poc demonstra

O corpo físico e a decisão de `models/players/paratrooper` (queda livre → paraquedas → pouso),
**sem nenhuma ligação com o C-130** — os quatro paraquedistas já nascem em queda, como se
tivessem acabado de sair de uma aeronave num instante anterior. A liberação de verdade a partir
de um `StoresMgr` do C-130 é tarefa futura, deliberadamente adiada (ver
`models/players/paratrooper/docs/ARCHITECTURE.md`).

Escalonados em **altitude** (não só posição): como os limiares de estágio (`deployAgl`/
`groundAgl`) são relativos à AGL, escalonar só em posição faria os quatro pousarem quase no mesmo
instante (a perna sob o velame é sempre a mesma distância AGL) — o que esconderia um bug de
estado compartilhado entre instâncias. Escalonar em altitude MSL faz cada um pousar num frame
diferente, e o dump abaixo mostra os quatro em estágios diferentes no MESMO frame — prova de que
o estágio é por-instância.

## Rodar

```bash
./build/app/src/app -folder src/poc -scenario paratrooper-drop                       # interativo
./build/app/src/app -folder src/poc -scenario paratrooper-drop -deterministic 6000   # headless
tests/determinism/check_determinism.sh ./build/app/src/app paratrooper-drop 6000 "" \
   src/poc/paratrooper-drop/configs/scenario_paratrooper_drop.edl.in
```

Porta Tacview **1242**. Sem `networks:` (hermética) — roda ao lado das demais pocs sem colidir.

**Não usar `-f`**: esse caminho assume a frota fixa `falcon1..4` (`app::adHocScenario()`), que
não existe aqui — sai com `"player 'falcon1' nao encontrado!"`. `-folder` descobre a frota em
runtime (`app::discoverFleet()`), o único caminho que funciona para um cenário sem `AirVehicle`
nenhum.

## Medido rodando (6000 frames, 120 s simulados, `-threads 2`)

1. **Os três estágios acontecem, em frames distintos por instância** (amostra a cada 100 frames):

   | player | último `FREEFALL` | primeiro `CANOPY` | primeiro `LANDED` |
   |---|---|---|---|
   | para1 | frame=1000 (t=20,0 s) | frame=1100 (t=22,0 s) | frame=3800 (t=76,0 s) |
   | para2 | frame=1100 | frame=1200 (t=24,0 s) | frame=3900 (t=78,0 s) |
   | para3 | frame=1200 | frame=1300 (t=26,0 s) | frame=4000 (t=80,0 s) |
   | para4 | frame=1300 | frame=1400 (t=28,0 s) | frame=4100 (t=82,0 s) |

   Em `frame=1300`, os quatro já divergem: `para1`/`para2` em `CANOPY`, `para3` na fronteira,
   `para4` ainda `FREEFALL` — os quatro estágios não são um estado global do plugin.

2. **Velocidade terminal em queda livre converge para `g/dragIndex`** — medido em `para1`:
   `spd=52,97 m/s` em t=20,0 s (frame=1000, ~3,6 constantes de tempo decorridas), convergindo
   para `54,48 m/s` (= 9,80665/0,18), dentro de ~3 % a essa altura — consistente com a curva
   fechada `v(t) = v_term·(1 − e^{-t/τ})`, `τ = 1/dragIndex ≈ 5,56 s`.

3. **Taxa de descida sob o velame é exatamente constante**: `spd=5.500000000` em TODO frame
   amostrado de `CANOPY` (1100 a 1500 e além, para1) — nem decai nem acelera. `pitch`/`roll`
   saem de `-90°`/`0°` (o nariz-baixo herdado da física nativa do `Effect` em queda livre) para
   exatamente `0°`/`0°` ao entrar em `CANOPY` — a correção de atitude documentada em
   `docs/ARCHITECTURE.md` funciona, visível no próprio dump.

4. **Ninguém morre.** `data/messages/mission.jsonl` (via `libs/xmsg`) tem uma condição
   `MsgChanged field: modeNum` que deveria ficar muda o cenário inteiro — **zero** linhas
   `"msg":"mudanca-modo"` nas 496 linhas do arquivo (120 s simulados); `damage`/`crashedFlag`/
   `killedFlag` permanecem `0` em toda amostra de telemetria, inclusive **~38-44 s depois do
   último pouso** — mais de 3× o `maxTOF` original de 10 s que `Effect` herdaria sem a
   sobrescrita de `updateTOF()` deste modelo.

5. **Determinismo**: `tests/determinism/check_determinism.sh` sobre 6000 frames — dumps
   byte-idênticos com 1, 2 e 4 threads T/C (mais a repetição de 4), e uma decisão por frame por
   paraquedista nas três configurações.

6. **Tacview**: com `typeMap: { PARATROOPER: "Ground+Light+Human+Air+Parachutist" }`, os quatro
   aparecem com essa identidade — sem o `typeMap`, um `Effect` de tipo não reconhecido
   (`Chaff`/`Decoy`/`Flare`/`Bomb`/`Bullet`/`Missile`) cai no fallback genérico de `WEAPON` e
   apareceria como **míssil**.

## Por que não entra na lista `pocs` de `tests/meson.build`

O vocabulário desta poc (`FREEFALL`/`CANOPY`/`LANDED`) não bate com o vocabulário de combate
(`EVADE`/`SUPPORT`/`RTB`) que `tests/scenario/run_scenario_test.py` afirma — mesmo precedente já
usado por `onnx-policy`/`c130-airdrop`/`built-in_mixr_1`. Coberta diretamente por
`tests/determinism/check_determinism.sh` com o caminho do cenário explícito (ver "Rodar" acima).

## Ler também

- [`../../../models/players/paratrooper/docs/ARCHITECTURE.md`](../../../models/players/paratrooper/docs/ARCHITECTURE.md)
  — todas as decisões de design do modelo
- [`../c130-airdrop/README.md`](../c130-airdrop/README.md) — o cenário que hoje libera um
  placeholder de paraquedista a partir do C-130 (a integração de verdade com o modelo real desta
  poc é tarefa futura)
