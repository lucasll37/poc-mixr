# A4-6DOF-MISSILE — um A-4 detecta outro pelo radar e dispara um míssil

Exercício (ver `TODO.md`, raiz do repositório): "implementar um míssil ... para verificar
como isso é modelado em termos de uso idiomático do MIXR". Dois `( Aircraft )` A-4, o mínimo
necessário para observar disparo → guiagem → detonação de ponta a ponta, sem nenhum outro
ramo de decisão competindo (evasão de verdade, apoio, patrulha múltipla) — não é um cenário
de produção.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-6DOF-MISSILE        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-6DOF-MISSILE -deterministic 6000
```

## A pergunta que este cenário responde

> Como se modela, de forma idiomática, um míssil guiado sobre o framework MIXR — e como um
> `( Aircraft )` já existente (`models/players/A-4`) dispara um contra outro?

A resposta tem duas metades, cada uma num subprojeto separado:

- **`models/players/missile`** — `( GuidedMissile )`, subclasse **cinemática** de
  `mixr::models::Missile` (sem `dynamicsModel` nenhum): guiagem por navegação proporcional de
  verdade e espoleta de proximidade, os dois em `domain/Guidance.hpp`, puros e testados sem
  MIXR. Ver o `README.md`/`docs/ARCHITECTURE.md` daquele diretório para o porquê de não
  repetir a abordagem histórica (JSBSim dedicado) que este repositório já teve e removeu.
- **`models/players/A-4`** — dois nós de árvore novos (`LaunchEnvelopeCondition`/
  `LaunchMissileAction`), um ramo novo numa árvore de **demonstração**
  (`configs/flight_tree_missile_demo.xml` — a árvore de produção não muda), e a extensão de
  `ubf::FlightAction::execute()` que faz o disparo de verdade:
  `Player::getStoresManagement()` → `StoresMgr::releaseOneMissile()` →
  `AbstractWeapon::setTargetPlayer(alvo, true)`.

## Os dois players, deliberadamente assimétricos

- **`a4_shooter`** (azul) — a mesma pilha de radar de `src/poc/dis/flight` (Gimbal+Antenna
  TWS, SensorMgr+Tws, OnboardComputer+AirTrkMgr), um `StoresMgr` com **um** `( GuidedMissile )`
  e o agente `( FlightAgentTC )` com a árvore de demonstração. Começa patrulhando rumo 090° a
  ~250 kt.
- **`a4_target`** (vermelho) — só `dynamicsModel`+`pilot`, sem sensor, sem stores, sem agente:
  `Autopilot::reset()` semeia os três modos de *hold* a partir de `initHeading`/`initAlt`/
  `initVelocity` quando nenhum `cmdHeading`/`cmdAltitude`/`cmdSpeed` é declarado — o mesmo
  padrão degenerado já usado por `bandit1` em `src/poc/dis/bandit`. Voa reto e nivelado, sem
  reagir a nada — o alvo existe só para ser interceptado.

Começam 15 NM separados, o alvo diretamente à frente do rumo inicial do atirador, fechando a
~470 kt de taxa de aproximação combinada.

## O que foi medido rodando (`-deterministic 6000`, `numTcThreads=1`)

A árvore do atirador entra em `bt=EVADE` assim que o radar detecta o alvo (t≈2 s, a 27.532 m
— fora do envelope de disparo configurado, 0,3–5,0 NM) e o ramo de evasão fixa um rumo de
fuga; enquanto o atirador gira lentamente para longe (a manobra de quebra do
`domain::ThreatPolicy`, ~0,3°/s pela resposta do próprio `Autopilot`/JSBSim), o alvo continua
se aproximando em linha reta. O disparo acontece assim que o alcance entra no envelope **e**
a marcação relativa ainda cabe no cone de 45° — medido na gravação Tacview
(`data/recordings/mission_*.acmi`, não versionada):

| evento | tempo simulado | observação |
|---|---|---|
| primeiro contato (radar) | t≈2,0 s | alcance 27.532 m — fora do envelope |
| disparo (`LaunchMissile`) | t≈67,1 s | míssil `AIM-X` (`Type=Weapon+Missile`) aparece na gravação, co-localizado com `a4_shooter` |
| aproximação mais próxima medida | t≈91,8–91,9 s | ~35 m entre o míssil e `a4_target` (dentro de `maxBurstRng: 150 m`, pouco acima de `lethalRange: 30 m`) |
| detonação + remoção (`DELETE_REQUEST`) | t≈93,9 s | o objeto do míssil some da gravação (`-<id>`), ~2 s depois da espoleta disparar — o *linger timer* de `xnative::GuidedMissile::updateTC()` |

Confirma, nessa ordem: o envelope de lançamento gateando corretamente o disparo (não dispara
nos primeiros ~65 s, enquanto fora de alcance); o míssil guiando de verdade até convergir a
poucas dezenas de metros do alvo (não uma trajetória balística/reta); a espoleta de
proximidade detonando perto do ponto de menor aproximação, não em qualquer outro instante; e
a limpeza pós-detonação removendo o míssil da lista de players/do Tacview — sem ela, o objeto
detonado ficaria para sempre na gravação (achado histórico já documentado em
`models/players/missile/docs/ARCHITECTURE.md`).

**Determinismo confirmado**: `tests/determinism/check_determinism.sh` com 1, 2 e 4 threads
T/C (mais uma repetição de 4), 6000 frames — dumps `frame=` byte-idênticos nas três
configurações. A guiagem por navegação proporcional e a espoleta de proximidade não
introduzem nenhuma dependência de ordem entre threads (nenhum estado compartilhado entre
aeronaves; o `domain::FuzeState` é privado de cada `GuidedMissile`).

**Depois do disparo**: com o único míssil do cabide consumido,
`domain::WorldView::weaponReady` vira `false` no frame seguinte (via
`StoresMgr::available()`), `LaunchEnvelopeCondition` deixa de suceder sozinho e a árvore cai
de volta para o ramo de evasão (`bt=EVADE`/`BREAK`) — sem precisar de nenhum estado extra no
nó para "já disparei uma vez".

## Sem terreno, de propósito

Ao contrário de `A4-6DOF`/`A4-6DOF-RANDOM`, este cenário **não** declara `terrain:` em
`simulation:`. O ponto aqui é o mecanismo de disparo, não navegação sobre relevo — terreno só
acrescentaria risco de `CRASH_EVENT` (AGL < 0) sem nenhum ganho para o que se quer observar.
Sem banco de elevação, `getAltitudeAgl()` devolve a altitude HAE diretamente (ver a seção
"Terreno" do `CLAUDE.md` raiz) — nunca negativo nas altitudes usadas aqui (~2000 m).

## Ler também

- [`models/players/missile/README.md`](../../models/players/missile/README.md) e
  [`docs/ARCHITECTURE.md`](../../models/players/missile/docs/ARCHITECTURE.md) — o modelo do
  míssil em si.
- [`models/players/A-4/CHANGELOG.md`](../../models/players/A-4/CHANGELOG.md) — a entrada
  "Envelope de lançamento de míssil", com a lista completa das peças novas no lançador.
- [`../A4-6DOF/README.md`](../A4-6DOF/README.md) — o cenário do "player máximo" navegando por
  `Route`/`Steerpoint`, referência para a pilha de sensor completa (esta poc usa só o
  subconjunto necessário para detecção TWS).
