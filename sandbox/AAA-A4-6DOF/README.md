# AAA-A4-6DOF — uma antiaérea com domo dispara contra um A-4, que evade pelo RWR

Exercício (ver `TODO.md`, raiz do repositório, segundo da série que começa com
`sandbox/A4-6DOF-MISSILE`): "implementar uma antiaérea com domo que dispara o mesmo míssil ...
que entra no seu raio de atuação. Depois, crie o cenário ... onde uma aeronave A-4 adentra no
domo de uma antiaérea. A antiaérea dispara contra essa aeronave, que percebe o míssil se
aproximar através do seu sistema de RWR e inicia manobras evasivas ... efeitos estocásticos
com o uso da `libs/xrandom` no retardo de ação do piloto evasivo." Não é um cenário de
produção — é o mínimo necessário para observar detecção → disparo → percepção via RWR →
atraso estocástico de reação → evasão, de ponta a ponta.

```bash
./build/app/src/app -folder ./sandbox -scenario AAA-A4-6DOF                    # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario AAA-A4-6DOF -deterministic 6000
```

## A pergunta que este cenário responde

> Como se modela, de forma idiomática, uma antiaérea com "domo" sobre o MIXR — reaproveitando
> o mesmo `( GuidedMissile )` cinemático já implementado para o A-4 — e como o alvo percebe a
> ameaça por um sensor **passivo** (RWR), com um atraso de reação estocástico e reprodutível?

A resposta tem duas metades, cada uma num subprojeto separado:

- **`models/players/aaa`** (novo) — `( AaaSite )`, subclasse de `mixr::models::SamVehicle`
  (que já traz os slots nativos `minLaunchRange`/`maxLaunchRange` — o "domo" já é dado nativo,
  não código novo). Pilha completa `domain/bt/ubf/xnative`, com uma árvore de comportamento de
  verdade (`Sequence(TargetInDome, FireMissile)` num `Fallback` com `Watch` de degradação) e
  decisão via `( UbfAgent )` **nativo** (sem subclasse de `AgentTC` — a antiaérea decide na
  taxa de FUNDO, ~10 Hz, de sobra para um alvo a ~130 m/s). Ver
  [`docs/ARCHITECTURE.md`](../../models/players/aaa/docs/ARCHITECTURE.md) para o porquê de não
  existir nenhum `AgentTC` próprio e para a armadilha do `SamVehicle`/`Sam`-cast (documentada
  abaixo também).
- **`models/players/A-4`** (estendido) — um segundo par condição/ação de árvore
  (`RwrThreatDetectedCondition`/`EvadeRwrThreatAction`, rótulos `RWR_EVADE`/`RWR_BREAK`), uma
  árvore de **demonstração** (`configs/flight_tree_rwr_evade_demo.xml` — a árvore de produção
  não muda), e `domain::EvasionReactionPlan` — o atraso estocástico, semeado via `libs/xrandom`
  a partir do mesmo `instanceSeed` que já deriva o jitter de patrulha, com um salt de propósito
  próprio (`kRwrReactionSalt`).

## Os dois players, deliberadamente assimétricos

- **`aaa_site`** (vermelha) — estacionária (sem `dynamicsModel:`, `initVelocity: 0.0`), com um
  radar de **aquisição** próprio (`Gimbal`+`Antenna`+`SensorMgr(Tws)`+`OnboardComputer
  (AirTrkMgr "aaaTrkMgr")`), um `StoresMgr` com **um** `( GuidedMissile )` no cabide e
  `agent: ( UbfAgent state:(AaaState) behavior:(AaaBehavior) )`. Domo: `minLaunchRange: 500 m`,
  `maxLaunchRange: 4500 m`.
- **`a4_intruder`** (azul) — 6-DOF completo (JSBSim+Autopilot), **sem** radar de busca próprio
  (não há outra aeronave neste cenário para detectar) — só o trio de RWR **passivo**
  (`( Rwr )`/`( RwrTrkMgr )`, mesma receita já provada em `src/poc/built-in_mixr_1`: antena
  dedicada `ant_rwr` com `playerOfInterestTypes` incluindo `ground` — obrigatório, é assim que
  o RWR enxerga o radar de aquisição **terrestre** da `aaa_site`). Agente
  `( FlightAgentTC )` com a árvore de demonstração.

Geometria: `a4_intruder` começa na origem, patrulhando rumo 090 (LESTE) a ~250 kt.
`aaa_site` fica parada 6000 m a leste (diretamente no caminho da primeira perna de patrulha),
virada para OESTE (rumo 270). Sem terreno (`simulation:` não declara `terrain:` — o ponto
desta poc é o mecanismo de detecção/disparo/evasão, não navegação sobre relevo).

## Armadilha confirmada rodando — não redescobrir: polarização cruzada zera o RWR por completo

O primeiro cenário funcional tinha `ant_rwr` (a antena do RWR do A-4) com
`polarization: vertical`, enquanto o radar de aquisição da `aaa_site` transmite
`polarization: horizontal`. `Antenna::onRfEmissionEvent()` (`Antenna.cpp:601-690`) computa o
ganho de recepção como `raGain = aea * pGain`, onde `pGain = getPolarizationGain(em->
getPolarization())` é uma tabela fixa (`Antenna.cpp:718-731`) — e a célula
`[VERTICAL][HORIZONTAL]` (e vice-versa) vale **exatamente 0.0** (polarizações ortogonais
seriam completamente rejeitadas por uma antena real). Com `raGain=0`, o sinal recebido
(`RfSystem::rfReceivedEmission()`, `signal = em->getPower() * rl * raGain / losses`) sai
**exatamente zero** — `Rwr::receive()` continua enfileirando o pacote (`np=1` a cada ciclo,
confirmado via `gdb` ao vivo), mas o SNR nunca vence o limiar (`snDbl = 10*log10(0) = -inf`),
então nenhum relatório chega ao `RwrTrkMgr`. **Nada no pipeline de recepção estava quebrado**
— era a física simplificada do modelo de antena do próprio MIXR fazendo exatamente o que
deveria fazer contra duas antenas cruzadas de propósito.

Diagnosticado com `gdb -batch`, breakpoints em `Antenna::onRfEmissionEvent`/
`Gimbal::fromPlayerOfInterest`/`RfSystem::rfReceivedEmission`/`Rwr::receive`, imprimindo
`raGain` a cada emissão recebida: **exatamente `0.000000` em toda chamada**, enquanto o
caminho de retorno da própria `aaa_site` (`RF_EMISSION_RETURN`, usado pela detecção do
`Tws` — um caminho **diferente**, que não passa pela tabela de polarização — ver
`Antenna::onRfEmissionReturnEventAntenna()`) chegava com `raGain=1.44`. É por isso que a
detecção da própria `aaa_site` sempre funcionou enquanto o RWR do A-4 ficava cego: são dois
caminhos de recepção distintos dentro da mesma classe, só um deles gateado por polarização.

**Corrigido**: `ant_rwr` não declara mais `polarization:` — o default nativo (`NONE`) dá
ganho de polarização `1.0` contra QUALQUER emissor, o comportamento correto de um RWR de
verdade (ele tem que detectar um radar hostil independente de como esse radar está
polarizado). Ver o comentário em `configs/scenario_aaa_a4_6dof.edl.in` sobre `ant_rwr`.

## O que foi medido rodando

Tempo real (`./build/app/src/app -folder ./sandbox -scenario AAA-A4-6DOF`, log em
`data/logs/`), duas execuções independentes (sementes de patrulha/reação idênticas — nenhum
override no `.edl`):

| evento | execução 1 | execução 2 |
|---|---|---|
| início (`-- -> PATROL`) | t=0,000 s | t=0,000 s |
| RWR detecta a ameaça, atraso estocástico vence, evasão inicia (`PATROL -> RWR_EVADE`) | t≈3,724 s | t≈3,736 s |
| antiaérea dispara (`missil lancado contra a4_intruder`) | t≈13,402 s | t≈13,415 s |

A diferença entre as duas execuções (~0,01 s) é ruído de escalonamento do laço de tempo real
(10 Hz, relógio de parede) — não do sorteio em si: o atraso é sorteado **uma vez**, na borda
`Idle -> Waiting` de `domain::EvasionReactionPlan`, a partir de um `instanceSeed` derivado do
NOME do player (`deriveSeed(masterSeed, fnv1a64("a4_intruder"))`, depois
`deriveSeed(instanceSeed, kRwrReactionSalt)`) — determinístico, nunca por `dt`. O atraso
sorteado (~3,7 s) cai dentro da janela configurada (`evadeReactionMinDelay: 2.0 s`,
`evadeReactionMaxDelay: 6.0 s`).

`-deterministic 6000` (50 Hz, dump a cada 100 frames = 2,0 s simulados):

| frame | bt (`a4_intruder`) | observação |
|---|---|---|
| 100 (t=2,0 s) | `PATROL` | RWR já detectou o radar da `aaa_site`, atraso ainda não venceu |
| 200 (t=4,0 s) | `RWR_EVADE` | transição ocorreu em algum ponto de (2,0 s, 4,0 s] — consistente com o ~3,7 s medido em tempo real |
| 300–2100 | `RWR_EVADE` | quebra evasiva em curso (rumo/altitude/velocidade mudando) |
| ~3400–5800 | `RWR_BREAK` | manobra sustentada (segunda fase do nó `EvadeRwrThreatAction`, mesmo padrão de dois rótulos de `ReportAndEvadeAction`) |
| 5900–6000 | `PATROL` | ameaça não mais presente (`ThreatPolicy` independente da evasão, `rwrThreat`, perde histerese) — retoma patrulha |

Contadores de instância no fim do dump (`meta=`): `GuidedMissile count=2 mc=2 tc=4` — a
antiaérea liberou um flyout do cabide (o item-molde do `StoresMgr` mais a cópia lançada).
`AaaAction tc=6001` — uma instância nova a cada decisão de fundo da antiaérea (~10 Hz),
confirmando que ela decidiu a cada ciclo, não só uma vez.

**Determinismo confirmado**: `tests/determinism/check_determinism.sh` com 1, 2 e 4 threads
T/C (mais uma repetição de 4), 6000 frames — dumps `frame=` **byte-idênticos** nas três
configurações. É a primeira vez, neste repositório, que `libs/xrandom` decide algo com efeito
colateral de disparo/evasão (não só cosmético, como o jitter de patrulha) — e o resultado se
mantém idêntico independente de quantas threads do pool decidem em paralelo, porque o sorteio
acontece numa borda de estado discreta (uma vez por episódio de ameaça), nunca por `dt`.

## Por que o gatilho de evasão é só "presença de emissor hostil no RWR"

Decisão confirmada com o usuário (`AskUserQuestion`, ver o plano desta implementação): o RWR
nativo do MIXR (`Track`, `RwrTrkMgr`) **não distingue** "sendo varrido por um radar de busca"
de "míssil guiado se aproximando" — `Track::isMissileWarning()` existe no framework mas não
tem nenhum `caller` neste fork (achado por auditoria, não usado). A implementação aqui é fiel
ao pedido literal ("percebe... através do seu sistema de RWR"): `RwrThreatDetectedCondition`
dispara assim que `xtrack::nearestHostileTrack(air, "rwrTrkMgr")` encontra QUALQUER emissor
hostil — no caso deste cenário, o próprio radar de **aquisição** da `aaa_site`, que ilumina o
A-4 continuamente desde antes de decidir disparar. Consequência observável: a evasão começa
(t≈3,7 s) **bem antes** do disparo em si (t≈13,4 s) — o A-4 já está evadindo quando o míssil
sai do cabide, exatamente como um RWR de verdade avisaria "estou sendo iluminado", não
"míssil confirmado a caminho".

## Ler também

- [`models/players/aaa/README.md`](../../models/players/aaa/README.md) e
  [`docs/ARCHITECTURE.md`](../../models/players/aaa/docs/ARCHITECTURE.md) — o modelo da
  antiaérea.
- [`models/players/A-4/CHANGELOG.md`](../../models/players/A-4/CHANGELOG.md) — a entrada da
  extensão de RWR/evasão estocástica.
- [`../A4-6DOF-MISSILE/README.md`](../A4-6DOF-MISSILE/README.md) — o cenário anterior da
  mesma série (A-4 detecta e dispara contra outro A-4 via radar ativo).
- [`libs/xrandom/`](../../libs/xrandom) — a derivação de semente reprodutível (ver também a
  seção `libs/xrandom` do `CLAUDE.md` raiz).
