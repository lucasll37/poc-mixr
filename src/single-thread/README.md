# single-thread

Quatro caças patrulham quadrantes distintos sobre a Serra do Mar; um intruso cruza a área;
**quem detecta avisa os outros** pelo datalink, e quem recebe o aviso muda de comportamento e vai
apoiar. Sobre terreno real, com dinâmica 6-DOF real, radar real, e a decisão saindo de uma árvore
de comportamento arbitrada por voto.

A regra de projeto do subprojeto é uma só: **herdar do MIXR tudo o que o framework já tem
pronto**. Player, dinâmica 6-DOF, controle de voo, radar, datalink, banco de elevação e agente
são todos nativos. O que continua sendo nosso é o que o framework, por definição, não fornece: a
**política** — percepção, decisão, atuação — e a carga útil da mensagem trocada entre os aviões.

> **O nome diz onde a DECISÃO roda — não que a simulação seja mono-thread.** Aqui o ciclo do UBF
> (percepção → decisão → atuação) fica a cargo do `( SimAgent )` **nativo**, componente da
> `Station`, que decide em `updateData()`: os quatro agentes são avaliados **em sequência, numa
> thread só** — a de background —, a 10 Hz. A **simulação** continua multithread: o cenário
> declara `numTcThreads` e o framework fatia os players pelo pool de threads de tempo crítico,
> exatamente como na **[poc/multi-thread](../multi-thread/)** — as duas passam no check de
> determinismo com 1, 2 e 4 threads. Lá a única diferença é o agente: um `( FlightAgentTC )`
> próprio, componente do **player**, que decide na fase 3 do frame — os quatro **em paralelo**,
> um por thread do pool, a 50 Hz.

```bash
make build
make run-single-thread        # Tacview Real-Time Telemetry na porta 1234; Ctrl+C encerra
make check-single-thread      # verifica o determinismo (1, 2 e 4 threads T/C)
```

> **Rode sempre a partir da raiz do repositório**: o cenário, os dados do JSBSim, o tile SRTM e a
> gravação `.acmi` são resolvidos por caminho relativo (`./src/single-thread/...`,
> `./shared/data/...`).

---

## Índice

1. [O que vem do framework e o que é nosso](#1-o-que-vem-do-framework-e-o-que-é-nosso)
2. [Anatomia de um frame](#2-anatomia-de-um-frame)
3. [Como o framework chama o nosso código](#3-como-o-framework-chama-o-nosso-código)
4. [A árvore de objetos do cenário](#4-a-árvore-de-objetos-do-cenário)
5. [Padrões dos exemplos oficiais usados aqui](#5-padrões-dos-exemplos-oficiais-usados-aqui)
6. [Cada peça nativa, uma a uma](#6-cada-peça-nativa-uma-a-uma)
7. [**Dissecação: o repositório em ordem de dependência**](#7-dissecação-o-repositório-em-ordem-de-dependência)
8. [A cadeia de decisão: UBF + BehaviorTree](#8-a-cadeia-de-decisão-ubf--behaviortree)
9. [Interação entre players](#9-interação-entre-players)
10. [Elevação de terreno](#10-elevação-de-terreno)
11. [Tacview](#11-tacview)
12. [Determinismo](#12-determinismo)
13. [Armadilhas encontradas rodando](#13-armadilhas-encontradas-rodando)
14. [Controle de tempo — acelerar, frear, pausar](#14-controle-de-tempo--acelerar-frear-pausar)
15. [Como verificar tudo](#15-como-verificar-tudo)
16. [O que a poc responde](#16-o-que-a-poc-responde)

> **Sugestão de leitura.** Se você quer *entender o MIXR*, leia 1 → 2 → 3 → 4. Se você quer
> *dissecar este repositório*, pule para a [seção 7](#7-dissecação-o-repositório-em-ordem-de-dependência),
> que percorre todos os arquivos na ordem em que as dependências aparecem. As seções 8 a 12 são
> mergulhos temáticos; a 13 é o catálogo de cicatrizes.

---

## 1. O que vem do framework e o que é nosso

| peça | poc/12 (tudo do zero) | **aqui** (nativo) |
|---|---|---|
| player | `xair::Airplane : Player` | `( Aircraft )` |
| dinâmica 6-DOF | `xair::JsbsimFlightModel : System` | `( JSBSimModel )` |
| controle de voo | `xair::FlightDirector : System` | `( Autopilot )` |
| sensor | `xair::ProximitySensor : System` | `( Gimbal/Antenna )` + `( Tws )` + `( AirTrkMgr )` |
| interação | `xair::AlertRadio : System` | `( AlertDatalink : models::Datalink )` |
| agente UBF | `xair::FlightAgent : AgentTC` | `( SimAgent )` |
| árbitro | `( UbfArbiter )` | `( UbfArbiter )` — igual |
| terreno | — | `( SrtmHgtFile )` + `Player::updateElevation()` |
| percepção / decisão / atuação | `FlightState` / `BtBehavior` / `FlightAction` | **idem** — o UBF não traz implementações |

**Sobraram seis classes próprias**, e nenhuma delas é player, dinâmica, controle ou sensor:

| classe nossa | herda de | por que não dá para herdar pronta |
|---|---|---|
| `xnative::FlightState` | `base::ubf::AbstractState` | o UBF define a **interface** de percepção; `models/` só acrescenta `SimAgent` e `MultiActorAgent` |
| `xnative::BtBehavior` | `base::ubf::AbstractBehavior` | idem, para decisão |
| `xnative::AltitudeSafetyBehavior` | `base::ubf::AbstractBehavior` | idem |
| `xnative::FlightAction` | `base::ubf::AbstractAction` | idem, para atuação |
| `xnative::AlertDatalink` | `models::Datalink` | o framework transporta; **o que fazer com a mensagem** é da aplicação |
| `xnative::TacticalAlert` | `base::Object` | a carga útil é, por definição, da aplicação |

O que se ganha ao herdar não é só linha de código economizada — é modelo que ninguém escreve por
gosto: equação do radar com RCS e perdas, correlação de pistas com filtro alfa-beta, transporte
de datalink com fila de rede, limites de autopilot, integração 6-DOF, consulta a banco de
elevação. E o que se **paga** aparece nas bordas, catalogado na [seção 13](#13-armadilhas-encontradas-rodando).

---

## 2. Anatomia de um frame

Este diagrama é a chave de leitura de tudo o que vem depois. **Onde uma coisa roda determina o
que ela pode ler e o que ela pode escrever.**

```
thread de tempo crítico (PeriodicThread, dt = 1/tcRate FIXO — aqui 50 Hz)
└─ Station::tcFrame(dt) → Simulation::updateTC(dt)
   │      (a lista de players é fatiada entre numTcThreads, com BARREIRA por fase)
   │
   ├─ FASE 0  "dinâmica"
   │    Player::dynamics()  →  JSBSimModel::dynamics()   ← passo do 6-DOF
   │                            ├─ lê  ap/heading_setpoint, ap/altitude_setpoint,
   │                            │      ap/airspeed_setpoint  (escritos pelo Autopilot)
   │                            └─ FDM Run() → superfícies → forças → estado do Player
   │    Datalink::dynamics()  ← fila de saída (nativo)
   │
   ├─ FASE 1  "sensores transmitem"
   │    Antenna/Tws  → emissões de RF para os players de interesse
   │                 → targets[i]->event(RF_EMISSION, em)   ← a interação entre players
   │
   ├─ FASE 2  "sensores recebem"
   │    Radar::receive()          → detecções (limiar S/I)
   │    AlertDatalink::receive()  → PROMOVE o alerta encenado (nosso)
   │
   └─ FASE 3  "lógica e controle"
        AirTrkMgr::process()  → cria/atualiza as pistas (alfa-beta)
        Autopilot::process()  → erro de rumo/altitude/velocidade → setCommanded*()

thread de background (laço de app/RealTimeRun.cpp, bgRate = 10 Hz)
└─ Station::updateData(dt)
   ├─ Simulation::updateData() → updateBgPlayerList()
   │     └─ Player::updateData() → updateElevation()   ← consulta ao banco de terreno
   ├─ SimAgent::updateData() → Agent::controller()
   │     ├─ FlightState::updateState(ator)      PERCEPÇÃO
   │     ├─ UbfArbiter::genAction()             DECISÃO (por voto)
   │     │     ├─ AltitudeSafetyBehavior  (voto 90)
   │     │     └─ BtBehavior              (voto 50) → tick da árvore
   │     └─ FlightAction::execute(ator)         ATUAÇÃO → Autopilot + Datalink
   └─ DataRecorder::processRecords() → TacviewOutput → stream/arquivo ACMI
```

Três leituras que valem guardar:

1. **A decisão foi parar na thread de background.** `SimAgent` deriva de `ubf::Agent`, cujo ciclo
   roda em `updateData()`. É daí que vem o nome do subprojeto: **uma** thread para os quatro
   agentes, em sequência. Manter a decisão dentro da fase 3 exigiria um agente próprio — e o
   framework não traz um pronto ([13.6](#136-ubfagenttc-não-é-registrado-por-nenhuma-factory-do-mixr)).
2. **A elevação do terreno também é de background.** `Player::updateElevation()` é chamado de
   `Player::updateData()`, não de nenhuma das quatro fases. Consequência medida na
   [seção 10](#10-elevação-de-terreno).
3. **As duas caixas são threads diferentes, com relógios diferentes.** É daí que sai a única
   ameaça real ao determinismo — e o artifício que a neutraliza ([seção 12](#12-determinismo)).

> **Detalhe do `dt`:** a lista de players é percorrida 4× por frame, cada vez com `dt/4`. Como
> cada método de fase roda em **uma** dessas passagens, `Player::updateTC()` e
> `System::updateTC()` recompõem `dt*4` no ponto do despacho. Um `dynamics()` recebe o `dt` do
> frame inteiro.

---

## 3. Como o framework chama o nosso código

Não existe registro nem callback. O MIXR dirige o nosso código porque **duas** coisas são
verdade ao mesmo tempo:

1. **o objeto está na árvore de componentes** (foi o `.epp` que o colocou lá), e
2. **a classe herda de algo que o framework já sabe dirigir**.

O despacho é virtual, puro e simples:

```
Component::updateTC(dt)          percorre a lista de componentes
   └─ System::updateTC(dt)       consulta sim->phase() e despacha:
        fase 0 → dynamics(dt*4)
        fase 1 → transmit(dt*4)
        fase 2 → receive(dt*4)
        fase 3 → process(dt*4)
```

Por isso o `AlertDatalink` só precisa herdar `models::Datalink` (que é um `System`) e
sobrescrever `receive()` — quem o chama na fase certa é o `System::updateTC()`, que já existe.

Cada peça é "pega" por um mecanismo diferente:

| classe | como o framework chega nela |
|---|---|
| `Aircraft`, `bandit1`… | estão na lista `players:` do `WorldModel`, percorrida pela `Simulation` |
| `JSBSimModel` | achado **por tipo** em `Player::updateSystemPointers()` (`findByType(typeid(DynamicsModel))`) e chamado por `Player::dynamics()` na fase 0 |
| `Autopilot` | idem, como `Pilot` (`getPilotByType`), rodando na fase 3 |
| `Antenna`/`Tws`/`AirTrkMgr` | são `System`s dentro do player: fases 1 (transmite), 2 (recebe) e 3 (processa pistas) |
| `AlertDatalink` | `System` na lista de componentes → fase 2; e `event(DATALINK_MESSAGE)` pela tabela `BEGIN_EVENT_HANDLER` da classe base |
| `SrtmHgtFile` | objeto do slot `terrain:` do `WorldModel`; carregado por `Terrain::reset()` no `RESET_EVENT` e consultado por `Player::updateElevation()` |
| `SimAgent` | componente da **Station**; o ciclo do UBF roda em `updateData()` (background) |
| `FlightState`/`BtBehavior`/`FlightAction` | **não** são chamados pelo ciclo de componentes: quem os chama é `Agent::controller()` |
| `TacviewOutput` | elo da cadeia de `OutputHandler`s do `DataRecorder`, drenada por `Station::updateData()` |

**A consequência que quase todo mundo descobre tarde:** um nome errado em qualquer slot `*Name:`
produz um componente **inerte, sem diagnóstico**. Vale para `antennaName`, `trackManagerName`,
`actorPlayerName`, `leadPlayerName`. O objeto é construído, entra na árvore, é chamado em todas
as fases — e não faz nada, porque o ponteiro que ele procurava é nulo.

---

## 4. A árvore de objetos do cenário

[`configs/scenario.epp.in`](configs/scenario.epp.in) monta isto (o `@NUM_TC_THREADS@` é
substituído por [`app/ScenarioTemplate.cpp`](src/app/ScenarioTemplate.cpp) antes do parse, porque
o teto depende da máquina):

```
( ClockStation                                       ← shared/xclock: Station + controle de tempo
   components:      { agent1..agent4 : ( SimAgent actorPlayerName: falconN ... ) }
   dataRecorder:    ( DataRecorder enabledList: [ 43 42 ]
                        outputHandler: ( RecorderOutputHandler
                           components: { ( TacviewOutput modelMap/typeMap/colorMap ) } ) )
   simulation: ( WorldModel
        numTcThreads: N
        latitude/longitude: -22.25 / -42.48          ← Serra do Mar (RJ), miolo do tile
        terrain: ( SrtmHgtFile path/file: S23W043.hgt )
        players: {
           falcon1..falcon4 : ( Aircraft
              signature: ( SigSphere radius: 3.0 )   ← RCS: como os OUTROS radares o veem
              dataLogTime: ( Seconds 0.1 )           ← sem isto, não aparece no Tacview
              interpolateTerrain: true               ← bilinear entre os posts de 30 m
              components: {
                 dynamicsModel: ( JSBSimModel  rootDir/model: c310 )
                 pilot:         ( Autopilot    hold modes + limites )
                 datalink:      ( AlertDatalink holdTime )
                 antennas:      ( Gimbal components: { radar: ( Antenna gainPattern ... ) } )
                 sensors:       ( SensorMgr components: { ( Tws antennaName: radar
                                                             trackManagerName: twsTrkMgr ) } )
                 obc:           ( OnboardComputer components: { twsTrkMgr: ( AirTrkMgr ) } )
              } )
           bandit1 : ( Aircraft  dynamicsModel + pilot, SEM sensor/datalink/agente )
        } ) )
```

Cinco coisas que valem entender nessa árvore:

- **`Player` não tem slot para subsistema nenhum.** Tudo entra por `components:` e é localizado
  **por tipo** (`Player::updateSystemPointers()`). Por isso a ordem no `.epp` é irrelevante *para
  encontrar*, os rótulos (`dynamicsModel:`, `pilot:`…) são livres, e um player sem um dado
  subsistema não é erro — é só um player que não tem aquilo (o caso do `bandit1`).
- **Mas a ordem é ordem de EXECUÇÃO dentro da fase.** `Component::updateTC()` percorre os
  subcomponentes na ordem declarada. Como `sensors:` vem antes de `obc:`, o `Radar::process()`
  (que faz `newReport`) roda antes do `TrackManager::process()` (que drena) — e a pista aparece no
  **mesmo** frame. Inverta os dois no EDL e ela atrasa um frame, sem uma linha de C++ mudar.
- **O agente mora na `Station`, não no player.** É assim que o `SimAgent` nativo funciona: ele
  amarra o ator pelo nome (`actorPlayerName`). Essa é *a* diferença para a
  [poc/multi-thread](../multi-thread/).
- **`signature:` é o que torna o avião detectável.** O radar de um player lê a `RfSignature` do
  outro para resolver a equação do radar. Sem ela, o avião é invisível — não importa quantos
  radares existam.
- **`terrain:` é slot do `WorldModel`, e a factory dele não vem de graça.**
  `mixr::models::factory` **não** encadeia a de terreno; sem `mixr::terrain::factory` no
  [`mixr_factory.cpp`](src/mixr_factory.cpp), o `( SrtmHgtFile )` não constrói nada e o mundo fica
  sem chão, em silêncio.

**As altitudes não são arbitrárias.** Cada falcon voa no **pico do próprio circuito de patrulha
+ 300 m** (falcon1 1750, falcon2 1850, falcon3 2050, falcon4 2100 m), porque com terreno
carregado o `CRASH_EVENT` nativo passa a valer: abaixo do solo o player vai para `CRASHED` e
**congela**. O intruso voa a 2400 m — acima de todos —, e é isso que faz o desconflito vertical
da evasão ser para **baixo**, que é o único caso em que o terreno tem algo a dizer. Ver a
[seção 10](#10-elevação-de-terreno).

---

## 5. Padrões dos exemplos oficiais usados aqui

Os exemplos que acompanham o MIXR são a única documentação de *como se escreve uma aplicação*
com ele. A destilação está em `contexts/MIXR-PATTERN-CONTEXT.md`; abaixo, os cinco padrões que
este subprojeto usa e onde eles aparecem.

### 5.1 O *builder* canônico

Trinta e oito dos `main.cpp` oficiais fazem a mesma coisa: chamam `base::edl_parser(arquivo,
minhaFactory, &erros)`, desembrulham o `base::Pair` de topo, fazem `dynamic_cast` para o tipo
raiz esperado, e abortam se qualquer passo falhar. Aqui isso vive em
[`app/StationBuilder.cpp`](src/app/StationBuilder.cpp), isolado do `main.cpp`:

```cpp
obj = pair->object();  obj->ref();  pair->unref();   // o desembrulho canônico
```

O `ref()` antes do `unref()` não é paranoia: o `Pair` é dono do objeto, e destruí-lo sem
incrementar a contagem levaria a `Station` junto.

### 5.2 Factory encadeada por nome

Todo exemplo tem uma função `factory(const std::string&)` que tenta as suas classes e cai para as
do framework. **A primeira que retorna não-nulo vence**, então a factory própria vem sempre
antes. [`src/mixr_factory.cpp`](src/mixr_factory.cpp):

```
xnative → xtacview → xclock → simulation → models → terrain → recorder → base
```

`xclock` **tem** de vir antes de `simulation`, porque `ClockStation` é uma `Station` com nome de
fábrica próprio; e `terrain` tem de estar lá, porque `models::factory` não a encadeia.

### 5.3 Estrutura em EDL, comportamento em C++

Nenhum exemplo oficial constrói a hierarquia de simulação em código. Quais players existem, com
quais subsistemas, em que taxa, com quantas threads — é tudo `.epp`. Aqui isso vai ao extremo: o
número de threads T/C é um marcador `@NUM_TC_THREADS@` resolvido **antes do parse**
([`app/ScenarioTemplate`](src/app/ScenarioTemplate.cpp)), porque `setSlotNumTcThreads()` é privado
e não existe setter público — a única forma honesta de manter tudo instanciado via EDL.

### 5.4 O gancho de sensor é `transmit()`, e a biblioteca `shared/x<nome>`

O único exemplo de sensor novo em toda a árvore oficial (`mainGndMapRdr/RealBeamRadar`) engata em
`transmit(dt)`, chamando `BaseClass::transmit(dt)` primeiro, e busca recursos do mundo
preguiçosamente (`if (terrain == nullptr) ... getWorldModel()->getTerrain()`). Esta poc não
escreve sensor próprio, mas usa o **outro** padrão do mesmo exemplo: o de empacotar o que é
transversal como `shared/x<nome>` com factory própria — é o que são
[`shared/xtacview`](../../shared/xtacview/) e [`shared/xclock`](../../shared/xclock/).

### 5.5 `dataRecorder` com cadeia de `OutputHandler`

O padrão de gravação oficial é `DataRecorder` → `RecorderOutputHandler` → `components: { ... }`.
Ninguém abre socket no `main.cpp`. É exatamente onde o `TacviewOutput` se pendura — ver a
[seção 11](#11-tacview).

---

## 6. Cada peça nativa, uma a uma

### 6.1 `( Aircraft )` — o player

`models::Aircraft : AirVehicle : Player`. Traz posição/atitude/velocidade em três sistemas de
coordenadas, integração de estado, *ground clamping*, detecção de colisão com o solo, ciclo de
4 fases, tabela de eventos e a resolução por tipo dos subsistemas.

Não há slot para subsistema: tudo entra por `components:`. `AirVehicle` acrescenta os acessores
de aeronave — `getMach()`, `getGload()`, `getAngleOfAttack()`, `getFuelWt()`, `getEngThrust()`,
`setThrottles()` — que delegam ao `DynamicsModel`. É por eles que a percepção lê o 6-DOF sem
conhecer o JSBSim.

### 6.2 `( JSBSimModel )` — a dinâmica 6-DOF

`models::JSBSimModel : DynamicsModel : System`. É o adaptador nativo para a
[JSBSim](https://jsbsim.sourceforge.net/): monta uma `FGFDMExec`, carrega a aeronave de
`rootDir`/`model`, e a cada fase 0 roda um passo do FDM completo — aerodinâmica, propulsão,
massa, atmosfera, trem de pouso.

O comando não é por chamada de método: o `Autopilot` escreve nas **propriedades JSBSim**
`ap/heading_setpoint`, `ap/altitude_setpoint`, `ap/airspeed_setpoint` (mais os respectivos
`*_hold`), e é o autopilot **do modelo da aeronave** que fecha a malha até as superfícies.

> **Por que c310, e não o F4N das pocs 04/05/12.** Essa cadeia só funciona se o modelo JSBSim
> tiver autopilot próprio. O F4N não tem — foi por isso que aquelas pocs precisaram escrever
> controle à mão. O c310 tem (`<autopilot file="c310ap">`), então `Autopilot → JSBSimModel →
> superfícies` funciona sem uma linha de lei de controle nossa. O `aircraft/f16` distribuído com
> o JSBSim também não tem autopilot.

Três cicatrizes desta peça estão em [13.1](#131-modelsjsbsimmodel-é-final),
[13.2](#132-o-jsbsimmodel-nativo-nunca-liga-os-motores) e
[13.3](#133-o-autopilot-do-c310-não-fecha-malha-de-velocidade).

### 6.3 `( Autopilot )` — o controle

`models::Autopilot : Pilot : System`. Roda na fase 3 e converte comandos de alto nível (rumo,
altitude, velocidade) em chamadas ao dynamics model, respeitando os limites declarados no `.epp`:
`maxRateOfTurnDps`, `maxBankAngle`, `maxPitchAngle`, `maxClimbRateMps`, `maxAcceleration`.

Traz de graça `headingHoldMode`, `altitudeHoldMode`, `velocityHoldMode`, `navMode` (seguir uma
`Route`) e `followTheLeadMode` (voo em formação). É o que dispensa escrever à mão a malha
rumo→banco→aileron e a malha altitude→arfagem→profundor.

> **Unidade:** `setCommandedAltitudeFt()` é em **pés**; o resto da poc trabalha em metros. A
> conversão acontece na fronteira, em [`ubf/FlightAction.cpp`](src/ubf/FlightAction.cpp).

> **Limite que importa aqui:** `maxClimbRateMps: 8.0`. Combinado com `evadeHold: 30 s`, é o que
> define quanto o avião consegue descer numa manobra de evasão — ~330 m, medido. Esse número
> reaparece, decisivo, na [seção 10](#10-elevação-de-terreno).

### 6.4 O radar nativo — `Gimbal` + `Antenna` + `Tws` + `AirTrkMgr`

Quatro classes, quatro papéis, três fases:

| classe | papel | fase |
|---|---|---|
| `Gimbal` | aponta e **filtra** os *players of interest* (tipo, alcance, ângulo, horizonte, oclusão de terreno) | fundo |
| `Antenna` | padrão de ganho (`Func1`/`Table1`), volume de busca, ERP, entrega a emissão | 1 |
| `Tws` (*Track While Scan*) | modelo de radar: potência, frequência, PRF, largura de pulso, limiar S/I | 1 e 2 |
| `AirTrkMgr` | correlação e filtro alfa-beta; produz os `RfTrack` | 3 |

O que se herda aqui é a equação do radar inteira, com RCS do alvo, perdas de propagação de ida e
volta e limiar de detecção — e a correlação de pistas com *gates* de posição, alcance e
velocidade. Ver o percurso completo em [9.3](#93-canal-1--rf-emissão-eco-pista).

**Duas regras que o sensor nativo não implementa** e por isso moram em
[`xnative/TrackQuery`](src/xnative/TrackQuery.cpp): o radar **não filtra por lado**
(`playerOfInterestTypes` filtra por *tipo* de player, não por *side* — a esquadrilha inteira
aparece como pista), e a escolha do "contato mais próximo" precisa de desempate determinístico.

### 6.5 `xnative::AlertDatalink : models::Datalink` — a interação

Herda o transporte inteiro e acrescenta **só** o que o framework não tem como saber: o que fazer
com a mensagem recebida. Detalhes em [9.4](#94-canal-2--datalink) e no
[header da classe](include/xnative/AlertDatalink.hpp), que documenta o que vem de graça e os dois
enganos fáceis.

### 6.6 `( SimAgent )` + `( UbfArbiter )` — o agente e o árbitro

`models::SimAgent : base::ubf::Agent` roda o ciclo do UBF em `updateData()` e amarra o ator por
nome. `base::ubf::UbfArbiter` é o comportamento composto: ele mesmo é um `AbstractBehavior`, pede
uma ação a cada filho e devolve a de **maior voto**.

Aqui os dois filhos são `AltitudeSafetyBehavior` (voto 90) e `BtBehavior` (voto 50). Um
comportamento pode devolver `nullptr` — "não recomendo nada" —, que é como a regra dura fica em
silêncio enquanto a aeronave estiver acima do piso.

### 6.7 As peças que continuam nossas

As seis da [seção 1](#1-o-que-vem-do-framework-e-o-que-é-nosso), mais três utilitários de runtime
(`Log`, `ThreadTag`, `BehaviorBoard`) e a camada `domain/` inteira — que não é MIXR nem BT, e é
justamente por isso que ela é a parte testável sem simulação.

---

## 7. Dissecação: o repositório em ordem de dependência

Esta seção percorre **todos** os arquivos do subprojeto na ordem em que se pode lê-los sem
precisar saltar para frente: cada camada só depende das anteriores. É o roteiro para dissecar o
repositório.

```
CAMADA 1  domain/          C++ puro. Zero includes de MIXR, BT ou JSBSim.
   ↓
CAMADA 2  xnative/ (util)  Log, ThreadTag, BehaviorBoard. Só a biblioteca padrão.
   ↓
CAMADA 3  xnative/ (MIXR)  TacticalAlert, AlertDatalink, TrackQuery. Herdam do framework.
   ↓
CAMADA 4  ubf/             FlightState, BtTuning, BtBehavior, FlightAction, AltitudeSafety.
   ↕                       (ciclo controlado com bt/ — explicado em 7.4)
CAMADA 5  bt/              NodeContext, os 7 nós, bt_factory.
   ↓
CAMADA 6  factories        xnative/factory.cpp → mixr_factory.cpp
   ↓
CAMADA 7  app/             Options, TerrainData, ScenarioTemplate, StationBuilder, Fleet,
   ↓                       StatusReport, DeterministicDump, DeterministicRun, RealTimeRun
CAMADA 8  main.cpp         só orquestra
   ↓
CAMADA 9  configs/ e data/ o que não é código
```

### 7.1 Camada 1 — `domain/`: as regras puras

Seis arquivos, **nenhum** deles inclui um header do MIXR, do BehaviorTree.CPP ou do JSBSim. É a
camada que se pode compilar e testar sem levantar simulação nenhuma. Todas as unidades trazem a
unidade no **nome do campo** (`altitudeM`, `speedKts`, `headingDeg`) — a armadilha clássica deste
repositório é misturar pés, metros e nós.

**1. [`domain/FlightCommand.hpp`](include/domain/FlightCommand.hpp)** — a base de tudo. Um DTO de
três campos:

```cpp
struct FlightCommand {
   double headingDeg{};    // rumo verdadeiro comandado (graus)
   double altitudeM{};     // altitude comandada (metros)
   double speedKts{};      // velocidade comandada (nós)
};
```

Todo plano de voo e toda política produzem **isto**. É o contrato entre a decisão e a atuação, e
é o motivo de a árvore de comportamento nunca tocar num objeto MIXR.

**2. [`domain/geometry.*`](include/domain/geometry.hpp)** — matemática de plano tangente. Opera no
NED da *gaming area* do `WorldModel` (x = Norte, y = Leste, metros a partir do ponto de
referência do cenário):

| função | o que faz |
|---|---|
| `wrap180(deg)` | normaliza para `(-180, 180]` |
| `wrap360(deg)` | normaliza para `[0, 360)` |
| `headingToDeg(fromN, fromE, toN, toE)` | `atan2(ΔLeste, ΔNorte)` — zero aponta ao norte e cresce para leste |
| `distanceM(...)` | distância horizontal euclidiana |
| `relativeTo(...)` | devolve `RelativeGeometry{rangeM, bearingDeg, relBearingDeg, deltaAltM}` |

A ordem dos argumentos do `atan2` é a pegadinha: em navegação o ângulo é medido do Norte, então é
`atan2(E, N)`, e não o `atan2(y, x)` matemático.

**3. [`domain/PatrolPlan.*`](include/domain/PatrolPlan.hpp)** — circuito cíclico. `configure()`
recebe rumo inicial, duração da perna, curva por perna, altitude e velocidade; `advance(dt)`
integra o relógio da perna e devolve `true` quando trocou; `command()` devolve
`startHeading + turnPerLeg × leg`. Com `turnPerLegDeg=90` o circuito é um quadrado; com 120, um
triângulo; com 60, um hexágono.

> **Detalhe de projeto que vale copiar:** `advance(dt)` só é chamado **pelo nó `Patrol`**. Quando
> o avião está em RTB ou evadindo, o relógio da perna **não corre** — a patrulha é retomada
> exatamente de onde parou, em vez de "pular" o tempo em que esteve ocupado.

**4. [`domain/RtbPlan.*`](include/domain/RtbPlan.hpp)** — retorno à base (a origem da área de
jogo). `command(ownN, ownE, ownHeading)` aponta para a base; ao chegar dentro de
`arrivalRadiusM`, mantém o rumo e reduz a velocidade a 60 %. Não modela reabastecimento — isso é
estado do sistema de combustível, não do plano.

**5. [`domain/TerrainFloor.*`](include/domain/TerrainFloor.hpp)** — a regra de terreno, em duas
funções e um struct:

```cpp
struct GroundReference { bool valid{}; double elevationM{}; };

double terrainFloorM(const GroundReference&, double clearanceM, double absoluteFloorM);
double clampToTerrain(double commandedAltM, const GroundReference&,
                      double clearanceM, double absoluteFloorM);
```

Duas camadas, e a de baixo é a que importa: **terreno + folga** quando há dado, e **piso
absoluto** sempre, como mínimo. O piso absoluto não é redundância — o
`Player::updateElevation()` nativo ignora o retorno de `getElevation()`, então não existe forma
honesta de perguntar "estou coberto?" ([seção 10](#10-elevação-de-terreno)). `clearanceM <= 0`
desliga a camada de terreno sem desligar a rede, que é o que torna possível o controle negativo
do cenário sem recompilar.

**6. [`domain/ThreatPolicy.*`](include/domain/ThreatPolicy.hpp)** — a manobra de evasão, e o único
arquivo de `domain/` com estado interno de máquina. Depende de `FlightCommand`, `geometry` e
`TerrainFloor`.

```
livre        sem contato e sem histerese   → engaged() == false
em manobra   com contato                   → alvo FIXADO, timer cheio
em arrasto   sem contato, timer > 0        → mesmo alvo, timer caindo
```

`breakCommand()` calcula, **uma única vez, na entrada da manobra**:

- rumo: marcação **absoluta** do contato (`ownHeading + relBearing`) deslocada por `breakTurnDeg`
  para o lado oposto ao que ele ocupa;
- altitude: `ownAlt ∓ climbM`, no sentido contrário ao do contato, e então **passada pelo piso de
  terreno**;
- velocidade: `dashSpeedKts`.

As três correções que estão nesse arquivo vieram de observar o voo no Tacview e estão explicadas
na [seção 8](#8-a-cadeia-de-decisão-ubf--behaviortree) e na [seção 10](#10-elevação-de-terreno).

### 7.2 Camada 2 — `xnative/`, os utilitários de runtime

Três arquivos, um por questão, todos independentes entre si e do MIXR.

**[`xnative/Log.*`](include/xnative/Log.hpp)** — `logLine()` com mutex. `std::cout` não é
sincronizado e aqui vários players escrevem de threads T/C diferentes: sem o mutex as linhas se
entrelaçam. Só para eventos raros (troca de estado da árvore, falha ao carregar a árvore),
**nunca a cada frame**. `setLoggingEnabled(false)` desliga tudo no modo determinístico, porque
essas linhas carregam número de thread.

**[`xnative/ThreadTag.*`](include/xnative/ThreadTag.hpp)** — `threadTag()` devolve um índice
pequeno e estável (0, 1, 2…) para a thread chamadora, e `currentCpu()` chama `sched_getcpu()`.
Existe porque o MIXR **não expõe os handles do seu pool** (`tcThreads` é privado em
`simulation::Simulation`), mas o nosso código *roda* nessas threads — então dá para registrar, de
dentro, quem está processando cada player. É assim que o *round-robin* do pool fica observável.

A implementação tem um detalhe que vale copiar:

```cpp
static thread_local int cachedTag{-1};
if (cachedTag >= 0) return cachedTag;    // o mutex global é tocado UMA vez por thread
```

Sem o cache haveria um lock global no caminho quente (todo player, todo frame) — exatamente o
tipo de serialização que anularia o pool de threads do framework.

**[`xnative/BehaviorBoard.*`](include/xnative/BehaviorBoard.hpp)** — um `std::map<int,
std::string>` global sob mutex, com `setBehaviorLabel(id, label)` e `getBehaviorLabel(id)`.

> **Este arquivo é um bom exemplo do preço de herdar tudo.** Na poc/12 o rótulo do comportamento
> vencedor morava num campo do *nosso* player (`xair::Airplane`). Aqui o player é o
> `models::Aircraft` nativo, que obviamente não tem um campo para isso. Um quadro global por id
> resolve sem subclassear o `Player` só por causa de uma string — e escancara que, ao herdar
> tudo, **some o lugar natural para guardar o que é seu**. É escrito pela atuação (thread de
> tempo crítico) e lido pelo laço de background, daí o mutex.

### 7.3 Camada 3 — `xnative/`, as classes derivadas do MIXR

**[`xnative/TacticalAlert.*`](include/xnative/TacticalAlert.hpp)** — a carga útil do alerta. É um
`base::Object` de verdade (ref-contado, com a RTTI do framework), como a `Emission` do radar: é
assim que o MIXR transporta dados em eventos, e não com um struct solto. Campos deliberadamente
**crus** — posição NED em metros, alcance, nome do contato, id e nome do emissor. **O alerta não
carrega nenhuma ordem**: quem recebe decide o que fazer, o que mantém o emissor ignorante sobre o
comportamento do receptor.

> Não use `models::Message` para isso: ela existe no framework e **ninguém a usa**;
> `sendMessage()` recebe um `base::Object*` opaco.

**[`xnative/AlertDatalink.*`](include/xnative/AlertDatalink.hpp)** — depende de `TacticalAlert`.
Herda `models::Datalink` e acrescenta um único slot (`holdTime`) e três métodos:

| método | thread | o que faz |
|---|---|---|
| `broadcastAlert(...)` | do **emissor** (atuação) | monta o `TacticalAlert` e chama `sendMessage()` — o transporte é 100 % nativo |
| `onDatalinkMessageEvent(obj)` | do **emissor** (!) | **encena** o alerta em `staged`, com fusão comutativa, sob mutex curto |
| `receive(dt)` | do **receptor**, fase 2 | promove `staged → current` e envelhece com `holdTime` |

O `reset()` faz duas chamadas que não são óbvias:

```cpp
setNetworkQueueEnabled(false);   // sem NetIO no cenário, ninguém drenaria a fila de rede
setLocalSendEnabled(true);
```

Por que a disciplina encena/promove existe, e por que a fusão é comutativa, está em
[9.7](#97-o-caminho-desta-poc-fim-a-fim). É ela que sustenta o `make check-single-thread`.

**[`xnative/TrackQuery.*`](include/xnative/TrackQuery.hpp)** — uma função livre,
`nearestHostileTrack(air)`, que percorre
`AirVehicle → OnboardComputer → TrackManager("twsTrkMgr") → Track` e devolve um `TrackInfo`. Está
num arquivo só porque é consultada em **dois lugares muito diferentes** — a percepção do UBF e o
status/dump da aplicação — e os dois precisam dizer a mesma coisa.

As duas regras que ela acrescenta ao sensor nativo (filtro por lado e desempate determinístico)
estão em [6.4](#64-o-radar-nativo--gimbal--antenna--tws--airtrkmgr).

### 7.4 Camada 4 — `ubf/`: percepção, decisão, atuação

Aqui aparece o único ciclo de dependência do subprojeto, e ele é **deliberado e controlado**:

```
ubf/BtBehavior.hpp  ──inclui──→  bt/NodeContext.hpp   (precisa do struct FlightDecision)
bt/nodes/*.cpp      ──inclui──→  ubf/BtBehavior.hpp   (os nós leem o snapshot pelo behavior)
```

O ciclo não fecha porque **`bt/NodeContext.hpp` só faz *forward declaration*** de
`mixr::xnative::BtBehavior` e guarda um ponteiro. Ou seja: `bt/` conhece o *nome* do
comportamento, mas não a definição — quem inclui a definição são os `.cpp` dos nós. É o padrão
clássico de quebra de ciclo, e é a razão de `NodeContext` ser um struct de **um ponteiro só**.

**1. [`ubf/FlightState.*`](include/ubf/FlightState.hpp)** — a **percepção**. Herda
`base::ubf::AbstractState`. Um único método útil, `updateState(const base::Component* actor)`, que
lê o ator e monta um `Snapshot` de números crus.

Repare na assinatura: **o ator chega como `const Component*`** — percepção lê, não atua. O
`Snapshot` tem quatro grupos de campos:

| grupo | campos | de onde vem |
|---|---|---|
| próprio | `northM`, `eastM`, `altitudeM`, `headingDeg`, `speedKts`, `rollDeg`, `pitchDeg` | `AirVehicle` |
| telemetria 6-DOF | `fuelFraction`, `mach`, `gLoad`, `alphaDeg` | `AirVehicle` → `DynamicsModel` |
| **solo** | `terrainValid`, `terrainElevM`, `altitudeAglM` | `Player::updateElevation()` (background) |
| contato | `hasContact`, `contactName`, `contactRangeM`, `contactRelBearingDeg`, `contactDeltaAltM`, `contactNorth/East/AltitudeM` | `xnative::nearestHostileTrack()` |
| alerta | `hasAlert`, `alertSender`, `alertContactName`, `alertNorth/East/AltitudeM`, `alertRangeM` | `AlertDatalink::getAlert()` |

> **Armadilha do framework documentada no próprio header:** um `Agent` **não propaga**
> `updateTC()`/`updateData()` aos filhos, e o `state` é filho do agente. Este objeto **nunca**
> recebe o ciclo normal de componentes — tudo o que ele precisa fazer tem de estar dentro de
> `updateState()`.

**2. [`ubf/BtTuning.hpp`](include/ubf/BtTuning.hpp)** — um struct sem lógica e sem tipo MIXR, com
os 16 números que o EDL ajusta e os *defaults* visíveis lado a lado. Existe por dois motivos
práticos: `BtBehavior::copyData()` copia **um** membro em vez de dezesseis, e acrescentar um
parâmetro passa a ser uma linha aqui e uma no slot — sem risco de esquecer a cópia.

**3. [`ubf/BtBehavior.*`](include/ubf/BtBehavior.hpp) + [`ubf/BtBehaviorSlots.cpp`](src/ubf/BtBehaviorSlots.cpp)**
— a **decisão**. Herda `base::ubf::AbstractBehavior`. São dois arquivos `.cpp` para a **mesma
classe**, separados por questão:

- `BtBehavior.cpp` — ciclo de vida da árvore e o tick que produz a ação;
- `BtBehaviorSlots.cpp` — a **fronteira com o EDL**: `BEGIN_SLOTTABLE`, `BEGIN_SLOT_MAP` e os 16
  setters, cada um fazendo as mesmas três coisas (recusa nulo, converte para a unidade interna,
  valida a faixa) e escrevendo no `BtTuning`.

O `genAction()` é o coração:

```cpp
if (!plansReady) { configurePlans(); patrol.reset(); plansReady = true; }  // LAZY — ver abaixo
snap = flightState->snapshot();
frameDt = dt;
feedThreatPolicy(dt);                 // Snapshot → domain::ThreatPolicy (e a histerese envelhece)
if (!treeBuilt) buildTree();
currentDecision.reset();
tree.tickRoot();                      // ← a árvore roda aqui
if (!currentDecision.taken) return nullptr;
... monta o FlightAction, setVote(getVote()) ...
```

Duas decisões de implementação que existem por causa de armadilhas do framework:

- **`configurePlans()` é preguiçoso.** `reset()` nunca chega a um comportamento aninhado num
  `UbfArbiter` dentro de um `Agent` ([13.7](#137-o-agent-não-propaga-o-ciclo-de-componentes)), então a
  configuração vinda dos slots é aplicada na primeira decisão.
- **`buildTree()` é protegido por mutex global.** `BT::BehaviorTreeFactory::createTreeFromFile()`
  **não é reentrante**, e os quatro aviões chegam ao primeiro `genAction()` ao mesmo tempo, em
  threads T/C diferentes.

Cada `BtBehavior` — isto é, **cada aeronave** — tem a sua própria `BT::BehaviorTreeFactory` e a
sua própria `BT::Tree`. Não há árvore compartilhada.

**4. [`ubf/FlightAction.*`](include/ubf/FlightAction.hpp)** — a **atuação**. Herda
`base::ubf::AbstractAction`. É o **único** ponto da poc que escreve nos subsistemas a partir da
decisão.

```cpp
bool execute(base::Component* actor) override;   // o ator chega como PARÂMETRO
```

Essa assinatura é o que desacopla decisão de atuação no UBF: a ação **não guarda ponteiro para o
ator**, não o conhece na construção, e pode ser gerada por um comportamento que nem sabe de quem
é a aeronave. O corpo acha o autopilot por tipo (`getPilotByType(typeid(models::Autopilot))`),
liga os três *hold modes*, comanda rumo/altitude/velocidade — **convertendo metros para pés aqui,
na fronteira** — grava o rótulo no `BehaviorBoard` e, se pedido, chama
`AlertDatalink::broadcastAlert()`.

**5. [`ubf/AltitudeSafetyBehavior.*`](include/ubf/AltitudeSafetyBehavior.hpp)** — o segundo
comportamento, com voto **90** (maior que o 50 da árvore). Depende de `FlightState`,
`FlightAction` e `domain/TerrainFloor`.

Existe para mostrar **composição do UBF que não passa pela árvore**: uma regra dura fica fora da
política tática, e quando a aeronave fura o piso a ação dele vence sem que a árvore precise saber
que ele existe. Devolver `nullptr` — "não recomendo nada" — é legítimo e é o caso normal.

Tem **dois pisos**, e o de cima é quem manda:

```cpp
const bool belowAbsolute{snap.altitudeM < minAltitudeM};
const bool belowTerrain{ground.valid && minClearanceM > 0.0 && snap.altitudeAglM < minClearanceM};
```

Os dois *defaults* AGL são **zero** (desligado) de propósito: sem cenário que os declare, o
comportamento é exatamente o de antes do terreno.

### 7.5 Camada 5 — `bt/`: a árvore de comportamento

**1. [`bt/NodeContext.hpp`](include/bt/NodeContext.hpp)** — dois tipos, e nenhum inclui MIXR.

`FlightDecision` é o que a árvore **produz** num tick: um `FlightCommand`, um rótulo, e o pedido
de transmissão do alerta. Os nós não tocam em objeto MIXR nenhum — eles só preenchem esta
estrutura. Quem a transforma em atuação é o `FlightAction`. Assim o mesmo conjunto de nós serviria
a outra aeronave, outro atuador, ou a um teste unitário sem simulação.

`NodeContext` é a dependência fixa dos nós: **um ponteiro** para o `BtBehavior` que os hospeda.

> **Por que injeção por construtor e não pelo blackboard.** O blackboard do BehaviorTree.CPP é
> para dados que fluem **entre nós**, não para injeção de dependência. E há uma armadilha
> registrada: `Blackboard::create(parent)` **não** compartilha entradas automaticamente via
> `get`/`set`. Aqui a árvore é criada com um blackboard vazio, que ninguém usa para estado.

**2. Os sete nós** ([`src/bt/nodes/`](src/bt/nodes/)) — cada um guarda o `NodeContext` **por
valor** e não faz nada além de ler o comportamento e preencher a decisão:

| nó | tipo BT | lê | escreve |
|---|---|---|---|
| `FuelLow` | `ConditionNode` | `snapshot().fuelFraction`, `getFuelReserve()`, porta XML `margin` | — |
| `ReturnToBase` | `SyncActionNode` | `snap.northM/eastM/headingDeg` → `rtbPlan()` | decisão `RTB` ou `HOME` |
| `ContactDetected` | `ConditionNode` | **só** `threatPolicy().engaged()` | — |
| `ReportAndEvade` | `SyncActionNode` | `threatPolicy()`, `snap.contact*` | decisão `EVADE`/`BREAK` + `broadcastAlert` |
| `AlertReceived` | `ConditionNode` | **só** `snapshot().hasAlert` | — |
| `SupportAlert` | `SyncActionNode` | `snap.alert*`, `getSupportSpeedKts()` | decisão `SUPPORT` |
| `Patrol` | `SyncActionNode` | `patrolPlan()`, `getFrameDt()` | decisão `PATROL` |

Repare no `ContactDetected`: ele **não** pergunta "estou vendo o intruso agora?". Ele pergunta se
a manobra de evasão está valendo — que continua verdadeiro por `evadeHold` segundos depois de a
pista sumir. **A histerese é do modelo, não da árvore**: trocar o XML não a desliga.

**3. [`bt/bt_factory.*`](include/bt/bt_factory.hpp)** — registra os sete nós. O ponto de extensão
do BehaviorTree.CPP **v3** para construtores com argumentos extras é `registerBuilder<T>(ID,
builder)`, com um lambda capturando o contexto:

```cpp
BT::NodeBuilder builder{
   [context](const std::string& name, const BT::NodeConfiguration& config) {
      return std::make_unique<NodeType>(name, config, context);
   }};
factory.registerBuilder<NodeType>(id, builder);
```

> A sobrecarga variádica de `registerNodeType` **só existe em versões posteriores**. Aqui é a v3.5.6.

### 7.6 Camada 6 — as duas factories

**1. [`xnative/factory.cpp`](src/xnative/factory.cpp)** — uma cadeia de `else if` sobre
`getFactoryName()`, registrando exatamente as **seis** classes próprias. O comentário no topo do
arquivo é a métrica do subprojeto: a poc/12 registrava **onze**.

**2. [`mixr_factory.cpp`](src/mixr_factory.cpp)** — a factory que o `edl_parser` recebe. Oito
tentativas em ordem:

```cpp
xnative → xtacview → xclock → simulation → models → terrain → recorder → base
```

**A primeira que retorna não-nulo vence**, e é daí que vêm as duas regras da ordem:

- **`terrain` tem de estar presente.** `models::factory` **não** a encadeia — é a única das nove
  bibliotecas do pacote que precisa ser pedida explicitamente. Sem essa linha, `( SrtmHgtFile )`
  não constrói nada e o mundo fica sem chão, em silêncio.
- **As factories próprias vêm antes das do framework.** Aqui isso não é *load-bearing*:
  `ClockStation` tem nome de fábrica próprio (`"ClockStation"`, e não `"Station"`), então
  `simulation::factory` devolveria `nullptr` e a cadeia cairia no `xclock` de qualquer jeito. A
  ordem é disciplina — ela passa a importar no dia em que uma classe local **reusar** um nome do
  framework para substituí-lo, que é exatamente o caso que o padrão protege.

### 7.7 Camada 7 — `app/`: a aplicação, uma questão por arquivo

Nove módulos, na ordem em que o `main.cpp` os chama.

**1. [`app/Options.*`](include/app/Options.hpp)** — `argv` → struct. Não abre arquivo, não
constrói nada, não decide nada. Argumentos desconhecidos são **ignorados**, com a mesma tolerância
dos exemplos do framework.

**2. [`app/TerrainData.*`](include/app/TerrainData.hpp)** — garante que o `.hgt` existe em disco
**com o tamanho exato** que o `SrtmHgtFile` aceita. Duas armadilhas do framework, as duas
silenciosas, justificam este arquivo: o `SrtmHgtFile` **não lê `.gz`** (abre um `ifstream` cru), e
`determineSrtmInfo()` decide a resolução por um `switch` sobre o tamanho **em bytes**, sem
tolerância, cujo `default` só produz *"ERROR in determining SRTM type"* — sem dizer qual arquivo
nem por quê. Conferir o tamanho aqui é o antídoto.

**3. [`app/ScenarioTemplate.*`](include/app/ScenarioTemplate.hpp)** — `.epp.in` → `.epp`,
substituindo `@NUM_TC_THREADS@`. `resolveTcThreadCount()` usa `hardware_concurrency() - 1`
(reservando a thread do laço de background), limitado a 8 por padrão, e o `-threads N` do usuário
ainda é limitado pelo número de núcleos. Existe porque `setSlotNumTcThreads()` é **privado** e não
há setter público — a única forma honesta de manter tudo instanciado via EDL.

**4. [`app/StationBuilder.*`](include/app/StationBuilder.hpp)** — quatro funções:

| função | o que faz | falha |
|---|---|---|
| `buildStation(arquivo)` | `edl_parser` + desembrulho do `Pair` de topo + `dynamic_cast` | **fatal** |
| `primeStation(station)` | `RESET_EVENT` + **um frame de partida** | — |
| `worldModelOf(station)` | `dynamic_cast<models::WorldModel*>(getSimulation())` | **fatal** |
| `clockStationOf(station)` | `dynamic_cast<xclock::ClockStation*>` | **aviso** — trocar por `( Station )` continua rodando, só sem as teclas |

O frame de partida em `primeStation()` não é enfeite: é ele que faz os subsistemas resolverem os
ponteiros entre si antes do primeiro uso. E é durante esse `RESET_EVENT` que
`WorldModel::reset()` chama `terrain->reset()`, que carrega os 25 MB do tile.

**5. [`app/Fleet.*`](include/app/Fleet.hpp)** — `using Fleet = std::vector<AirVehicle*>`.
`collectFleet()` sai da árvore de objetos com ponteiros diretos; depois disso ninguém mais
precisa varrer o `PairStream`. Um nome que não existe é **erro fatal** — seguir sem ele só
adiaria a falha.

`applyCruiseThrottle()` fixa a potência com `AirVehicle::setThrottles()` (método do próprio
framework). É a correção da cicatriz [13.3](#133-o-autopilot-do-c310-não-fecha-malha-de-velocidade):
**a velocidade passa a ser resultado (potência fixa + arrasto), não comando.**

**6. [`app/StatusReport.*`](include/app/StatusReport.hpp)** — só **formato**. Não lê nada que
outros módulos não exponham e não muda estado. Quatro blocos por aeronave: voo
(`alt`/`elev`/`agl`/`hdg`/`roll`/`spd`/`thrust`/`mach`/`g`/`fuel`), decisão (`bt=` + o que o
`Autopilot` está comandando), sensor (`pista=`, `alerta<-`).

O cabeçalho traz `[t=<parede>s sim=<simulado>s <rótulo do relógio>]` — e é a **diferença entre os
dois tempos** que prova que o controle de velocidade está agindo.

**7. [`app/DeterministicDump.*`](include/app/DeterministicDump.hpp)** — um **contrato**, não um
relatório. As linhas produzidas aqui são o que os `make check-*` comparam. Duas regras seguem
disso:

- só entram grandezas que dependem do **estado da simulação**. Número de thread, tempo de parede
  e uso de CPU dependem do escalonador e ficam de fora;
- precisão fixa e alta (**9 casas**), para que uma divergência numérica mínima apareça em vez de
  ser arredondada.

**8. [`app/DeterministicRun.*`](include/app/DeterministicRun.hpp)** — o laço de passo fixo. Chama
`station->tcFrame(dt)` **direto**, sem passar por `processTimeCriticalTasks()`, e em seguida
`station->updateData(dt)` **no mesmo passo**. O que fica de fora ao pular o
`processTimeCriticalTasks()` é a **thread periódica** e o controle de velocidade do tempo — o
**pool T/C continua ligado**, com `numTcThreads` threads e as barreiras por fase. É esse
*lockstep* entre física e decisão que compra o determinismo — ver
[12.4](#124-o-artifício-colapsar-as-duas-threads-numa-só).

**9. [`app/RealTimeRun.*`](include/app/RealTimeRun.hpp)** — o laço normal, a 10 Hz. Cuida de três
coisas e só delas: um `poll()` não bloqueante do teclado, o `station->updateData(dt)` que drena o
gravador (e, aqui, roda os agentes), e o `msleep` que acerta o passo com o relógio de parede. **O
frame de tempo crítico não acontece aqui** — quem o roda é o pool nativo criado por
`createTimeCriticalProcess()`. O handler de `SIGINT` só marca uma flag.

### 7.8 Camada 8 — `main.cpp`

Fino de propósito: **não pilota, não tica árvore e não monta ACMI.** A sequência inteira:

```cpp
parseCommandLine                                     // app/Options
if (deterministic) setLoggingEnabled(false)          // xnative/Log
ensureTerrainData(terrainDir, terrainTile)           // app/TerrainData
generateScenario(templatePath, generatedPath, ...)   // app/ScenarioTemplate
buildStation(generatedPath)                          // app/StationBuilder
clockStationOf(station)                              //   "
primeStation(station)                                //   "  ← RESET: aqui o tile é carregado
worldModelOf(station)                                //   "
collectFleet(worldModel, playerNames)                // app/Fleet
applyCruiseThrottle(fleet, 0.95)                     //   "
runDeterministic(...) | printBanner + runRealTime(...)
station->event(SHUTDOWN_EVENT);  station->unref();
```

**A ordem importa.** `ensureTerrainData` antes de `generateScenario` porque o `.epp` nomeia o
`.hgt`; `primeStation` antes de `collectFleet` porque os players só existem depois do parse; e
`applyCruiseThrottle` depois do frame de partida porque o `DynamicsModel` precisa estar resolvido.

### 7.9 Camada 9 — `configs/` e `data/`

**[`configs/scenario.epp.in`](configs/scenario.epp.in)** — o cenário em EDL, dissecado na
[seção 4](#4-a-árvore-de-objetos-do-cenário). É `.in` e não `.epp` por causa do
`@NUM_TC_THREADS@`. O `.epp` gerado é *gitignored*.

**[`configs/flight_tree.xml`](configs/flight_tree.xml)** — a árvore, um `Fallback` de quatro
ramos em ordem de prioridade:

```xml
<Fallback name="root">
  <Sequence name="rtb_sequence">     <FuelLow margin="0.05"/>  <ReturnToBase/>  </Sequence>
  <Sequence name="engage_sequence">  <ContactDetected/>        <ReportAndEvade/></Sequence>
  <Sequence name="support_sequence"> <AlertReceived/>          <SupportAlert/>  </Sequence>
  <Patrol/>
</Fallback>
```

Os itens 2 e 3 são os dois lados da interação entre players: quem viu transmite, quem recebeu
reage. **Nenhum nó fala diretamente com outro avião.**

**`data/jsbsim/`** — a aeronave, vendorizada do pacote JSBSim com **duas** alterações nossas, as
duas em *dados* e não em C++:

| arquivo | alteração | por quê |
|---|---|---|
| `systems/engine-autostart.xml` | magnetos + partida + `propulsion/engine[i]/set-running` | o `JSBSimModel` nativo nunca liga os motores ([13.2](#132-o-jsbsimmodel-nativo-nunca-liga-os-motores)) |
| `aircraft/c310/c310ap.xml` | canal `AP Autothrottle` (PID → manete) | o autopilot original só **declara** `ap/airspeed_hold` ([13.3](#133-o-autopilot-do-c310-não-fecha-malha-de-velocidade)) |

**`data/recordings/`** — saída: `mission.acmi`, *gitignored*.

**`../../shared/data/terrain/srtm/S23W043.hgt.gz`** — o tile, fora do subprojeto porque é
compartilhado com a gêmea.

---

## 8. A cadeia de decisão: UBF + BehaviorTree

O **UBF** (*Unified Behavior Framework*) define **três papéis** e nada mais:

| papel | interface do MIXR | nossa implementação |
|---|---|---|
| percepção | `base::ubf::AbstractState` | `xnative::FlightState` |
| decisão | `base::ubf::AbstractBehavior` | `xnative::BtBehavior`, `xnative::AltitudeSafetyBehavior` |
| atuação | `base::ubf::AbstractAction` | `xnative::FlightAction` |

O que o UBF **não** diz é *como* decidir. É aí que entra a árvore: `BtBehavior` é um
comportamento do UBF cuja política interna é uma `BT::Tree` do BehaviorTree.CPP v3.

```
SimAgent (nativo)
 └─ Agent::controller(dt)                          o ciclo, do framework
      ├─ FlightState::updateState(ator)            PERCEPÇÃO → Snapshot (números crus)
      ├─ UbfArbiter::genAction(state, dt)          DECISÃO por voto
      │    ├─ AltitudeSafetyBehavior  vote 90  → ação só se furar o piso (senão nullptr)
      │    └─ BtBehavior              vote 50  → Snapshot → ThreatPolicy → tick da árvore
      │                                            └─ FlightDecision (comando + rótulo)
      └─ FlightAction::execute(ator)               ATUAÇÃO → Autopilot + AlertDatalink
```

**Os dois encaixes são independentes, e isso é o ponto.** O UBF não sabe que existe uma árvore; a
árvore não sabe que existe um UBF. `BtBehavior` é o adaptador entre os dois, e é por isso que ele
concentra as três responsabilidades feias: construir a árvore (uma vez, sob mutex), traduzir o
`Snapshot` para `domain::ThreatPolicy`, e empacotar a `FlightDecision` num `FlightAction` com o
voto certo.

Quatro lições de projeto que a poc paga para aprender:

- **Uma regra dura não precisa virar ramo da árvore.** O `AltitudeSafetyBehavior` é irmão do
  `BtBehavior` no árbitro, não um nó dentro dele. Ele vence por **voto**, sem que a árvore saiba
  que ele existe — e sem contaminar a política tática com uma condição de segurança.
- **A histerese é do modelo, não da árvore.** `ContactDetected` consulta
  `domain::ThreatPolicy::engaged()`, não `snapshot().hasContact`.
- **Os nós são burros de propósito.** Nenhum nó toca em objeto MIXR; todos leem o `BtBehavior` e
  preenchem a `FlightDecision`. O conjunto inteiro de nós é reutilizável fora de simulação.
- **O `dt` da árvore vem do UBF.** `getFrameDt()` devolve o `dt` que o `genAction()` recebeu — é
  ele que o `Patrol` integra. A árvore não tem relógio próprio.

### O ciclo-limite que ensinou tudo isso

> **Observado no Tacview: as aeronaves voavam "batendo asa".** Os ramos 2 e 3 comandam sentidos
> opostos sobre o **mesmo** objeto (fugir do intruso / ir até o ponto avisado). A quebra de 110°
> tira o intruso do setor do radar (`searchVolume` = ±30° em azimute), a pista some no mesmo
> instante, o ramo de apoio assume e traz a aeronave de volta — que reaquisita e quebra de novo.
> Medido no modo determinístico: **±25° de banco** (saturando o `maxBankAngle: 30`) com período de
> **~24 s**, indefinidamente.
>
> Duas correções, ambas em `domain/ThreatPolicy`:
>
> 1. **histerese** — `engaged()` vale por `evadeHold` (30 s) após perder o contato, então o ramo
>    de apoio não assume no piscar da pista;
> 2. **alvo fixado na entrada** — o comando era recalculado a cada tick como `meu_rumo + 110°`
>    (e `minha_altitude − 400 m`), um setpoint que fugia na mesma velocidade em que a aeronave
>    girava: a curva nunca terminava. Agora o alvo sai da **marcação absoluta do contato**,
>    calculado uma vez, e o piloto automático tem para onde convergir.
>
> Resultado, inversões de banco acima de 15° em 400 s de voo:
>
> | | falcon1 | falcon2 | falcon3 | falcon4 |
> |---|---|---|---|---|
> | antes | 26 | 26 | 24 | 15 |
> | depois | 3 | 4 | 8 | 6 |
>
> As que sobram são manobras legítimas, cada uma uma curva única que termina nivelada. Um rótulo
> novo separa os dois estados no status: `EVADE` (quebrando com a pista na tela) e `BREAK`
> (terminando a quebra no arrasto da histerese).

---

## 9. Interação entre players

Esta é a seção que responde "como dois players conversam" — não só nesta poc, mas no MIXR em
geral. A resposta cabe em uma frase:

> **Um player nunca chama método de outro player.** Ele entrega um `base::Object*` ao outro
> chamando `event()` nele — síncrono, dentro da própria pilha de chamada, **na thread de quem
> emitiu**.

Radar, datalink, colisão, IR, kill: **tudo** termina na mesma linha. O que muda entre os canais é
só (a) quem varreu a lista de players para achar o destinatário e (b) o que vai no payload.

### 9.1 A primitiva: `Component::event(token, Object*)`

```cpp
// base/Component.hpp -- a primitiva única de interação no MIXR inteiro
virtual bool event(const int event, base::Object* const obj = nullptr);
```

Não existe broker, fila global, pub/sub, nem roteamento declarativo no `.epp`. A prova mais curta
é o próprio datalink nativo sem rádio (`Datalink.cpp:332-360`) — literalmente um `for` na lista:

```cpp
base::PairStream* players{sim->getPlayers()};
while (playerItem != nullptr) {
   Player* player{static_cast<Player*>(playerPair->object())};
   if (player->isLocalPlayer()) {
      if ((player->isActive() || player->isMode(Player::PRE_RELEASE)) && player != getOwnship())
         player->event(DATALINK_MESSAGE, msg);   // Datalink.cpp:348  <- a interação inteira
      playerItem = playerItem->getNext();
   } else playerItem = nullptr;   // networked ficam no fim da lista; para aqui
}
```

O radar faz exatamente o mesmo, com outro token: `targets[i]->event(RF_EMISSION, em)`
(`Antenna.cpp:529`).

A lista é sempre a mesma: `Simulation::getPlayers()` (`Simulation.cpp:711`, devolve
**pré-`ref()`'d** — quem chama tem que `unref()`). Há três formas de achar alguém nela:
`findPlayer(id)`, `findPlayerByName(nome)` e `PairStream::findByName(nome)` sobre o retorno de
`getPlayers()`. Os `main.cpp` deste repo usam a terceira, inclusive o
[nosso](src/app/Fleet.cpp).

Do lado do receptor, o despacho é a macro (`base/macros.hpp:327-347`): uma cadeia de `if`s sobre
`_used`, **o primeiro que casa vence** — e por isso `ON_EVENT_OBJ` vem sempre antes de `ON_EVENT`
para o mesmo token.

### 9.2 Por que não é ponteiro direto

Você *pode* pegar o ponteiro do outro player — `getPlayers()` é público, e o `Autopilot` nativo
guarda um (`Autopilot.hpp:299`, `const Player* lead`, **sem `ref()`**). O que você não pode é
**agir** através dele, e o motivo é a [anatomia do frame](#2-anatomia-de-um-frame).

A lista de players é varrida **4 vezes por frame**, uma por fase (`Simulation.cpp:544-568`), e os
players podem estar em **threads diferentes** do pool T/C. Então, do seu componente:

- **ler** o outro player é corrida — ele pode estar no meio de `dynamics()` em outra thread, com
  a posição meio escrita;
- **escrever** nele é pior — você escreve fora da ordem determinística do frame, e o resultado
  passa a depender do escalonador.

`event()` sozinho **não** resolve a corrida: ele também roda na thread do emissor. O que ele dá é
um **ponto único de entrada**, que o receptor pode disciplinar. É exatamente o que o
`AlertDatalink` faz, e é o que sustenta o `make check-single-thread`:

| passo | onde roda | por quê |
|---|---|---|
| `onDatalinkMessageEvent()` só **encena** (`staged`), sob mutex curto | thread do **emissor** | seção crítica mínima; nenhuma decisão aqui |
| a fusão é **comutativa** (vence o mais próximo; empate → menor `senderId`) | idem | o resultado independe da **ordem de chegada**, que é do escalonador |
| `receive()` (fase 2) promove `staged → current` | thread do **receptor**, na fronteira de fase | latência **fixa** de 1 frame para todos |

Com ponteiro cru e escrita imediata, nenhuma das três propriedades existiria. E há o problema
banal por cima: ponteiro cru para um player que sai da lista fica pendurado — só `isActive()` e
`getDamage()` protegem o `lead` do `Autopilot`.

### 9.3 Canal 1 — RF: emissão, eco, pista

O canal mais elaborado, e o único em que o framework **modela física**. São **dois** percursos de
lista, em threads diferentes — essa é a parte não óbvia.

**Filtro (thread de fundo, fora do frame).** `RfSystem::updateData` (`:165`) pega
`sim->getPlayers()` (`:195`) e desce até `Tdb::processPlayers` — **o laço**, em `Tdb.cpp:280`.
Ali são aplicados os filtros que você declarou no `.epp`: `maxPlayersOfInterest`,
`playerOfInterestTypes`, `maxRange2PlayersOfInterest`, `maxAngle2PlayersOfInterest`, horizonte e
**oclusão de terreno** (esta última só se o `Gimbal` declarar `terrainOcculting: true` — ver
[10.5](#105-o-que-mais-o-terreno-destrava)). Quem sobrevive vira `targets[]`.

> **Armadilha:** o `Tdb` é montado numa taxa **não sincronizada** com o frame (`Gimbal` sequer
> sobrescreve `updateTC()`). A fase 1 pode estar usando um `Tdb` de um ou dois frames atrás.
> `Tdb::processPlayers()` **filtra**; não mede.

**Ida, volta e detecção (dentro do frame).** A equação do radar vem partida em duas metades —
ida na fase 1, volta na fase 2:

| # | fase | o que acontece | onde |
|---|---|---|---|
| 1 | 1 | `Radar::transmit()` monta a `Emission`; marca `setReturnRequest(...)` e `setTransmitter(this)` | `Radar.cpp:160-175` |
| 2 | 1 | `Antenna::rfTransmit()` recalcula a geometria **agora**, aplica ganho/ERP e pré-calcula `lossRng = 1/(4πr²)` | `Antenna.cpp:393`, `Emission.cpp:58` |
| 3 | 1 | **`targets[i]->event(RF_EMISSION, em)`** — chega no outro player | `Antenna.cpp:529` |
| 4 | 1 | o alvo calcula o próprio RCS: `signature->getRCS(em)`. **Sem `signature:` no `.epp`, vira 0** | `Player.cpp:2593` / `:2596` |
| 5 | 1 | o eco volta **direto pelo ponteiro** que veio no pacote — não passa pela lista de players | `Player.cpp:2600` |
| 6 | 1 | antena do emissor → sensor; sinal de **ida** = `power × rangeLoss × gain / losses`, enfileirado | `Antenna.cpp:707`, `RfSystem.cpp:230` |
| 7 | 2 | `Radar::receive()` filtra o próprio eco (`em->getTransmitter() == this`), aplica `rcs × rangeLoss` de **volta** e testa o limiar S/I | `Radar.cpp:224`, `:233`, `:277` |
| 8 | 3 | `Radar::process()` correlaciona e — **só no fim da varredura** (`endOfScanFlg`) — chama `tm->newReport(...)` | `Radar.cpp:344-351` |
| 9 | 3 | `AirTrkMgr::processTrackList()` drena, correlaciona, roda alfa-beta, cria o `RfTrack` | `AirTrkMgr.cpp:334-338` |
| 10 | — | você lê: `trkMgr->getTrackList(...)` | [`FlightState`](src/ubf/FlightState.cpp) |

Duas consequências práticas para quem escreve o `.epp`:

- **`signature:` não é enfeite.** É o passo 4. Sem ele o alvo devolve RCS 0, o pacote morre no
  limiar do passo 7 e o avião simplesmente não existe para o radar.
- **A ordem dos componentes é ordem de execução** — ver [seção 4](#4-a-árvore-de-objetos-do-cenário).

### 9.4 Canal 2 — datalink

`Datalink` estende `System` (não `Radio`). `sendMessage()` tem dois modos, mutuamente exclusivos:

- **com `radioName:`** → a mensagem vira payload de uma `Emission` e desce a cadeia RF inteira,
  com potência, alcance e tudo o mais;
- **sem rádio** → o `for` da lista mostrado em [9.1](#91-a-primitiva-componenteventtoken-object).

Do lado de quem recebe, são **três saltos**, todos do framework:

```
player->event(DATALINK_MESSAGE, msg)                    Player.cpp:202  (tabela de eventos)
   └─ Player::onDatalinkMessageEventPlayer()            Player.cpp:2746
        └─ getDatalink()->event(DATALINK_MESSAGE, msg)
             └─ Datalink::onDatalinkMessageEvent()      <- o nosso override
                  └─ BaseClass:: ... repassa aos subcomponentes
```

O `Player` acha o próprio datalink **por tipo**, em `updateSystemPointers()` (`Player.cpp:3148`)
— é por isso que uma subclasse nossa declarada no slot `datalink:` é encontrada sozinha, sem uma
linha de fiação.

> **Armadilha confirmada no fonte — o que `sendMessage()` NÃO faz.** Sem `radioName:`, ele **não
> filtra alcance nem lado**. O slot `maxRange` existe e o setter grava em `noRadioMaxRange`, mas
> **`sendMessage()` nunca lê essa variável**: não há teste de distância nenhum entre as linhas
> 332 e 360 de `Datalink.cpp`. A entrega é **broadcast global** para todo player local ativo —
> **inclusive o `bandit1`**, que é `red`. Nesta poc a mensagem morre lá porque o intruso não
> declara nenhum `datalink:`, e `Player::onDatalinkMessageEventPlayer()` só repassa se
> `getDatalink() != nullptr` (`Player.cpp:2746-2752`). Basta dar um datalink ao inimigo para ele
> passar a escutar a esquadrilha inteira. A poc/12 fazia os dois filtros à mão
> (`xair::AlertRadio.cpp:135` para o lado, `:143` para o alcance) e eles **não** foram herdados
> na migração para cá.

### 9.5 Canal 3 — evento próprio

Para um canal que não é radar nem datalink, você usa um token `>= USER_EVENTS (2000)` e chama
`event()`/`send()` direto. Duas armadilhas, ambas já pagas neste repo:

**(a) Eventos de aplicação NÃO sobem para o container.** `Component::event()` é escrito à mão, e
não com as macros, por causa exatamente do final (`Component.cpp:56-62`):

```cpp
// *** Special handling of the end of the EVENT table ***
// Pass only key events up to our container
if (_event <= MAX_KEY_EVENT && container() != nullptr)   // MAX_KEY_EVENT == 999
    _used = container()->event(_event,_obj);
```

Seu token 2001 **morre em silêncio** se ninguém tratar naquele nível exato. Só teclas sobem. Não
adianta "jogar na árvore" e esperar que alguém pegue: você tem que endereçar o destinatário.

**(b) `send(nome, ...)` resolve o nome entre os FILHOS de quem chama** — não em si mesmo, não na
árvore toda (`Component.cpp:1071`, `gobj->findByName(id)`, com o ponteiro cacheado depois da
primeira vez). A correção é chamar o `send()` **no ownship**, ou no player achado por nome, e não
no `this`.

Bônus de `send()` que quase ninguém nota: as sobrecargas escalares (`int`, `double`, `bool`…) só
disparam se o valor **mudou** — é um filtro de mudança embutido. A sobrecarga com `Object*` não
filtra.

### 9.6 Player que nasce em runtime

Interação também acontece com quem ainda não existia. Só **três** lugares no framework inteiro
chamam `addNewPlayer()`, e dois são `AbstractWeapon` — é assim que chaff, flare e mísseis viram
players:

```
storesMgr->releaseOneChaff()
  └─ Stores::releaseWeapon()                  Stores.cpp:313
       └─ AbstractWeapon::release()
            flyout = this->clone()            <- CLONA o store
            setSide(lplayer->getSide())       <- herda o lado do lançador
            sprintf(pname,"W%05d", getID())   <- nome sintético
            sim->addNewPlayer(pname, flyout)  AbstractWeapon.cpp:629
```

`addNewPlayer()` só **enfileira** (`Simulation.cpp:1045`). Quem materializa é `updatePlayerList()`
(`:953`), chamado de `Simulation::updateData()` (`:631`) — **thread de fundo, não o frame**. Ele
faz swap de lista inteiro (copy-on-write) e insere ordenado: locais primeiro, networked depois — e
é essa ordenação que autoriza o `break` antecipado do `Datalink` em 9.1.

Consequência: **um player criado só passa a existir para os outros no próximo `updateData()`**.

### 9.7 O caminho desta poc, fim a fim

```
falcon3 detecta bandit1 (radar nativo -- 9.3)
   └─ árvore: ContactDetected → ReportAndEvade
        └─ FlightDecision.broadcastAlert = true (+ posição absoluta do contato)
             └─ FlightAction::execute()
                  └─ AlertDatalink::broadcastAlert()
                       └─ Datalink::sendMessage(TacticalAlert*)     [NATIVO -- 9.4]
                            └─ ... → player->event(DATALINK_MESSAGE, msg)
                                 └─ AlertDatalink::onDatalinkMessageEvent()   ← nosso gancho
                                      └─ ENCENA o alerta (fusão comutativa)
   ... fronteira de fase ...
   fase 2 do frame seguinte: AlertDatalink::receive() promove encenado → corrente
   fase 3/background:        FlightState vê hasAlert → árvore: AlertReceived → SupportAlert
```

Três decisões de modelagem que valem mais que o transporte:

1. **O handler roda na thread do EMISSOR.** O `sendMessage()` nativo chama `event()` direto no
   destino. Por isso o handler só *encena* o alerta, sob um mutex curto, e nunca mexe no estado
   corrente.
2. **A entrada não é fila FIFO.** Se dois aviões avisam no mesmo frame, a ordem de chegada
   depende do escalonador. A fusão é **comutativa** — vence o contato de menor distância; em
   empate exato, o emissor de menor id — então o resultado independe da ordem.
3. **A promoção acontece numa fronteira de fase.** O alerta encenado só passa a valer na fase 2
   do frame seguinte, dando **latência fixa de um frame** para todos.

É a mesma disciplina que o framework usa entre as suas 4 fases: escreve numa fase, publica na
fronteira, lê na fase seguinte.

### 9.8 Receita: como escrever a sua

Escolha primeiro **de qual canal** você precisa:

| você quer… | use | custo em C++ |
|---|---|---|
| detecção física realista (RCS, potência, ruído, pistas filtradas) | RF nativo: `Antenna` + `Radar`/`Tws` + `AirTrkMgr` no `.epp` | **zero** |
| trocar uma **carga própria** entre players, sem modelar propagação | subclasse de `models::Datalink` | ~100 linhas — molde: [`AlertDatalink`](src/xnative/AlertDatalink.cpp) |
| um canal com regra própria (alcance, lado, quem ouve quem) | componente próprio + evento `>= 2000` | tudo seu |

O caminho do meio, que é o mais barato, em cinco passos:

1. **A carga** — um `base::Object` com `DECLARE_SUBCLASS`/`IMPLEMENT_SUBCLASS`.
2. **Emitir** — montar, `sendMessage(msg)`, `msg->unref()`. E, sem `NetIO` no cenário, forçar o
   caminho local no `reset()`:
   ```cpp
   setNetworkQueueEnabled(false);   // sem NetIO, ninguém drena a fila de rede
   setLocalSendEnabled(true);
   ```
3. **Receber** — sobrescrever `onDatalinkMessageEvent()` tratando como código que roda **na
   thread do outro**: `dynamic_cast` para a sua carga (pode chegar outro tipo), mutex **curto**,
   escrever num `staged` — nunca no estado que a decisão lê —, fusão **comutativa** se puderem
   chegar várias no mesmo frame, e terminar com `return BaseClass::onDatalinkMessageEvent(msg)`
   para não quebrar o repasse aos subcomponentes.
4. **Publicar numa fronteira de fase** — `receive(dt)` (fase 2) promove `staged → current` e
   envelhece com `holdTime`.
5. **Declarar** — `datalink: ( SuaClasse ... )` na lista de componentes do player (achado por
   tipo, automaticamente) e registrar a classe na factory local.

E as regras que valem para os **três** canais:

- `event()` roda na **thread do emissor**. Se o handler escreve, ou é comutativo, ou é
  determinístico só por sorte.
- Escreva numa fase, **publique na fronteira**, leia na fase seguinte.
- Ordem dos componentes no `.epp` é ordem de execução dentro da fase. Não é decoração.
- Nome errado em qualquer slot `*Name:` = componente inerte, **sem diagnóstico**.

---

## 10. Elevação de terreno

O banco de elevação é **100 % nativo**. `libmixr_terrain.so` já vinha linkado (o `mixr.pc` do
Conan lista `mixr-terrain` em `Requires:`), o `WorldModel` já tinha o slot `terrain`, e o `Player`
já tinha `getTerrainElevationM()`/`getAltitudeAglM()`/`updateElevation()`. **Nada foi escrito do
lado do framework e nenhuma linha de meson mudou.**

### 10.1 As três coisas que faltavam

| # | o quê | onde |
|---|---|---|
| 1 | **a factory** — `models::factory` não encadeia a de terreno | [`mixr_factory.cpp`](src/mixr_factory.cpp) |
| 2 | **o dado** — tile SRTM1 `S23W043` da Serra do Mar, descomprimido e conferido | [`app/TerrainData`](src/app/TerrainData.cpp) + `shared/data/terrain/srtm/` |
| 3 | **a ponte até a decisão** — três campos no `Snapshot`, uma regra pura, dois consumidores | [`FlightState`](src/ubf/FlightState.cpp), [`domain/TerrainFloor`](src/domain/TerrainFloor.cpp) |

O caminho completo do dado:

```
configs/scenario.epp.in     terrain: ( SrtmHgtFile path/file )
   └─ mixr::terrain::factory  constrói o SrtmHgtFile
        └─ WorldModel::setSlotTerrain()
             └─ RESET_EVENT → WorldModel::reset() → Terrain::reset() → loadData()
                                                     (25 MB, 3601×3601 posts de 2 bytes)
   ... a cada frame de BACKGROUND ...
   Player::updateData() → updateElevation() → terrain->getElevation(lat, lon, interp)
                                            → setTerrainElevation(el)  [tElev, tElevValid]
   ... a cada ciclo de decisão ...
   FlightState::updateState()  → snap.terrainElevM / altitudeAglM / terrainValid
        └─ BtBehavior::feedThreatPolicy()  → domain::GroundReference
             └─ domain::ThreatPolicy::breakCommand() → clampToTerrain(...)
        └─ AltitudeSafetyBehavior::genAction()      → terrainFloorM(...)
```

### 10.2 O que a elevação faz com o comportamento

**Piso anti-CFIT da evasão.** "Desconflitar para baixo" só faz sentido enquanto houver espaço
embaixo. A regra descia `evadeClimb` a partir da altitude corrente contra um piso absoluto de
200 m — que sobre esta serra fica ~1500 m *dentro* da montanha. O alvo agora passa por
`clampToTerrain()`: nunca abaixo de `elevação + terrainClearance` (800 m), com o piso absoluto
sobrando como rede. Como o alvo é fixado **uma vez, na entrada da manobra**, o piso também é
avaliado uma vez.

**Piso AGL do comportamento de segurança.** O `AltitudeSafetyBehavior` ganhou dois slots
(`minClearance`, `recoverClearance`) e passou a disparar por AGL, não só por altitude absoluta. O
piso absoluto de 1200 m continua como rede; sobre este terreno quem manda é o AGL.

> **Regra de ajuste:** `minClearance` (400 m) tem de ficar **abaixo** de `terrainClearance`
> (800 m). Senão o comportamento de segurança (voto 90) passa a brigar com o piso de evasão da
> árvore (voto 50) que ele mesmo deveria estar respeitando.

### 10.3 O que foi medido

Controle negativo em 12 000 frames, comparando `terrainClearance: ( Meters 800 )` com
`( Meters 0 )` — o controle que o próprio slot permite, sem recompilar:

| | altitude mínima do falcon1 | AGL correspondente |
|---|---|---|
| `terrainClearance: 800` | 1497 m | 831 m |
| `terrainClearance: 0` | 1430 m | 786 m |

**Cuidado ao mexer nesses números: quem limita a manobra é a *aeronave*, não o terreno.** Com
`maxClimbRateMps: 8.0` e `evadeHold: 30 s`, o C310 desce ~330 m por engajamento. Para o piso ser
*alcançado*, `terrainClearance` tem de ficar **dentro de ~330 m do AGL de cruzeiro** — daí os
800 m contra um cruzeiro de ~930 m AGL. Com uma folga "bonita" de 300–500 m o piso continua
correto e roda, mas recorta um alvo que a aeronave nunca alcançaria: **o dump sai idêntico ao do
controle negativo.** Foi o que aconteceu nas primeiras três tentativas de ajuste.

### 10.4 Armadilhas confirmadas no fonte

1. **`Player::updateElevation()` ignora o retorno de `getElevation()`** (`Player.cpp:3205-3206`).
   Fora da célula do tile, `el` fica `0.0` e `setTerrainElevation(0.0)` liga `tElevValid = true`.
   **`isTerrainElevationValid()` não é guarda de cobertura.** Daí o piso absoluto em
   `domain/TerrainFloor.hpp` e a referência do cenário no miolo da célula.
2. **`getAltitudeAgl()` não consulta `tElevValid`** (`Player.inl:262-266`): sem banco carregado,
   AGL == altitude HAE, silenciosamente.
3. **Os slots são `path` e `file`** — não `pathname`/`filename`, que é como se chamam os
   *setters*. Nome errado dá `slot not found` e o tile não carrega.
4. **`SrtmHgtFile` não lê `.gz`** e valida o **tamanho exato em bytes** (2 884 802 = SRTM3,
   25 934 402 = SRTM1). Qualquer outro tamanho falha com *"ERROR in determining SRTM type"*, sem
   dizer qual arquivo. O nome é lido por **posição fixa nos últimos 11 caracteres**.
5. **`CRASH_EVENT` deixa de ser letra morta.** `Player.cpp:2811` dispara com `AGL < 0`, e
   `crashNotification()` faz `setMode(CRASHED)` e manda `KILL_EVENT` a todos os subcomponentes —
   o avião **congela e para de decidir**, porque `updateTC`/`updateData` só rodam com
   `mode == ACTIVE`. Antes do terreno isso nunca acontecia porque `tElev` era sempre 0. É a razão
   de a altitude de cada falcon sair do **pico do próprio circuito + 300 m**. Escape hatch:
   `crashOverride: true` no player (slot nativo 26).
6. **`terrainElevReq` tem de continuar `false`** (default). Com `true`, `updateElevation()`
   **pula** a consulta ao banco e fica esperando um gerador de imagem externo empurrar o valor.
7. `getMinElevation()`/`getMaxElevation()` do `SrtmHgtFile` estão **errados** — refletem só a
   última linha lida. O `DtedFile` tem o mesmo defeito por coluna.
8. `QuadMap` aceita **no máximo 4** filhos e descarta o excedente **em silêncio**; e
   `QuadMap::clone()` devolve `nullptr`, o que faz `WorldModel::copyData()` estourar. Aqui se usa
   um `SrtmHgtFile` direto — nenhum dos dois se aplica.
9. `WorldModel::reset()` imprime `"Loading Terrain Data..."` em `stdout`, incondicionalmente.
   Inofensivo: os `check-*` filtram por `grep '^frame='`.
10. **Não acrescentar tokens ao `enabledList: [ 43 42 ]`.** `crashNotification()` grava
    `REID_PLAYER_CRASH`; mantê-lo fora da lista mantém o handler nativo fora do caminho — mesma
    família do defeito de [13.4](#134-o-gravador-nativo-segfalta-ao-gravar-pista-nova).

### 10.5 O que mais o terreno destrava

Duas coisas que existem no framework e que este cenário **não** liga, mas que passam a estar ao
alcance de uma linha de EDL agora que há banco de elevação:

- **Mascaramento de sensor por terreno.** O `Gimbal` tem o slot `terrainOcculting` (índice 28,
  **default `false`**). Ligado, o `Tdb` chama `Terrain::targetOcculting()` por alvo e **remove da
  lista de *players of interest*** quem estiver atrás de uma serra. Todo sensor construído sobre
  `Gimbal` — radares RF, buscadores IR, RWR, designadores — herda isso de graça.
- **Elevação de *steerpoint*.** `Steerpoint::setElevation(terrain, interp)` existe e preenche a
  elevação de um ponto de rota — mas **nada no framework a chama**. É código à espera da
  aplicação.

E uma que já está ligada sem ninguém pedir: o **radar-altímetro**.
`AirVehicle::getRadarAltitude()` devolve `getAltitudeAglFt()`, com `isRadarAltValid()` limitando a
0–5000 ft.

---

## 11. Tacview

A exportação é a de [shared/xtacview](../../shared/xtacview/), ligada na cadeia nativa do
`dataRecorder` (nenhum código de stream no `main.cpp`). Cada player precisa de
`dataLogTime: ( Seconds 0.1 )` — o slot nasce zero e, sem ele, o player nunca aparece.

Semântica ACMI (é onde quase todo mundo erra):

| campo | conteúdo | slot |
|---|---|---|
| `Name` | **modelo** em notação ICAO/OTAN — é por ele que o Tacview acha a aeronave na base e escolhe o ícone/modelo 3D | `modelMap` |
| `Type` | taxonomia (`Air+FixedWing`) | `typeMap` |
| `CallSign` / `Pilot` | **nome do player** — é o que aparece no rótulo | automático |
| `Color` | lado | `colorMap` |

A poc declara `modelMap: { falcon1: "F-16C" ... }`, então o replay desenha caças F-16:

```
65,T=...,Name=F-16C,Type=Air+FixedWing,Color=Blue,CallSign=falcon1,Pilot=falcon1
```

> **O ícone é apresentação, não física.** A dinâmica continua sendo a do c310 — a única aeronave
> com autopilot `ap/*` entre as distribuídas com o JSBSim. Trocar o ícone é uma linha no `.epp`;
> trocar a aeronave de verdade exigiria escrever as leis de controle à mão.

**Posição:** o registro do gravador carrega ECEF, convertido com `base::nav::convertEcef2Geod()`
antes de virar linha ACMI. A altitude no `.acmi` é **MSL/HAE — não há referência de solo no
stream**, mesmo com terreno carregado. Com a nova referência geográfica, as aeronaves aparecem
voando ~900 m acima do relevo da Serra do Mar.

---

## 12. Determinismo

**Determinístico** aqui quer dizer: rodar o mesmo cenário duas vezes e obter o mesmo estado, no
mesmo frame, até o último decimal. É exatamente o que `make check-single-thread` compara — e não é
de graça, porque a poc roda **em paralelo** em dois sentidos diferentes.

### 12.1 Dois paralelismos, um problema

| paralelismo | o que é | quebra o determinismo? |
|---|---|---|
| **pool T/C** (`numTcThreads: N`) | a lista de players é fatiada entre N threads dentro de `Simulation::updateTC()` | **não** — ver 12.2 |
| **decisão × física** | `SimAgent` decide em `updateData()` (laço do `main`, 10 Hz) enquanto `tcFrame()` roda na thread periódica (50 Hz) | **sim** — ver 12.3 |

O primeiro é o paralelismo que a poc *quer* (é para isso que existe o `-threads N`). O segundo é
um efeito colateral de herdar o `SimAgent` nativo.

### 12.2 Por que o pool T/C **não** quebra nada

O paralelismo do MIXR é de fork/join com **barreira em cada fase**, não uma corrida livre
(`Simulation.cpp:541-577`):

```
para cada uma das 4 fases do frame:
     fatia a lista de players entre as N threads      ← fork
     cada thread roda SÓ a fase corrente dos SEUS players
     SyncThread::waitForAllCompleted()                ← join, barreira
```

Duas consequências que juntas dão o determinismo:

1. **Dentro de uma fase, os players são independentes.** Cada thread mexe nos seus próprios
   players e nada que uma escreve é lido por outra na mesma fase — é a própria divisão em 4 fases
   que garante isso.
2. **A barreira fecha a fase para todos ao mesmo tempo.** Nenhum player pode "adiantar" a fase 2
   enquanto outro ainda está na fase 1.

Do nosso lado, três decisões conscientes fecham as brechas que sobrariam:

- **Passo fixo.** No Linux, `PeriodicThread::mainThreadFunc()` calcula `dt = 1/rate` uma vez e
  passa esse mesmo valor todo frame (o flag de delta variável é só do Windows e nasce desligado).
  O relógio de parede decide apenas *quando* o frame roda, nunca *quanto* ele avança.
- **Fusão comutativa dos alertas** ([9.7](#97-o-caminho-desta-poc-fim-a-fim)).
- **Escolha da pista sem depender da lista** — menor distância e, em empate exato, menor id de
  pista ([`TrackQuery`](src/xnative/TrackQuery.cpp)).

E, com o terreno: **a consulta de elevação é uma leitura de um banco imutável depois de
carregado**, feita sempre no mesmo ponto do passo. Não introduz dependência de ordem — o que o
`check-*` confirma, agora com os campos `elev=`/`agl=` dentro do dump.

Nada na poc usa relógio, sorteio ou identidade de thread para decidir.

### 12.3 O que **de fato** quebraria: a decisão fora do frame

O `SimAgent` é nativo, e o ciclo do `ubf::Agent` roda em `updateData()` — não numa fase do
`tcFrame()`. No modo de tempo real isso vira duas linhas de execução com relógios próprios:

```
thread T/C (periódica, 50 Hz)      laço do main (10 Hz, com msleep de relógio)
   tcFrame → física do avião          station->updateData → SimAgent decide
   ────────────────────────────────────────────────────────────────────────
                    sem nenhuma sincronização entre as duas
```

Três defeitos, todos de tempo:

1. **O agente amostra o player num ponto arbitrário do frame** — pode ler o avião entre a fase 0
   e a fase 3, com o estado meio atualizado.
2. **Quantas decisões acontecem por segundo simulado varia** com jitter e drift do `msleep`.
3. **Leitura sem sincronização**: os acessores do `Player` não têm lock.

Nenhum desses três se repete igual na execução seguinte.

### 12.4 O artifício: colapsar as duas threads numa só

O modo `-deterministic N` ([`app/DeterministicRun.cpp`](src/app/DeterministicRun.cpp)) **não sobe
a thread periódica**. A mesma thread chama as duas coisas, em ordem fixa, com passo fixo:

```cpp
const double dt{1.0 / station->getTimeCriticalRate()};   // 0.02 s — o MESMO passo dos dois lados
for (long frame = 1; frame <= frames; ++frame) {
   station->tcFrame(dt);      // 1) frame inteiro: 4 fases, pool T/C com suas barreiras
   station->updateData(dt);   // 2) SÓ ENTÃO os SimAgents decidem, sobre estado assentado
}
```

O que essa troca de três linhas compra, ponto a ponto contra a 12.3:

| defeito | como o artifício o remove |
|---|---|
| ponto de amostragem arbitrário | a decisão só roda **depois** do frame inteiro fechado |
| número variável de decisões | uma decisão por frame, sempre; `dt` de `updateData` = `dt` de `tcFrame` |
| leitura concorrente do player | não há concorrência: a física já terminou quando o agente lê |

Repare que o pool T/C **continua ligado** — `-threads N` funciona igual nos dois modos. O
artifício não serializa a simulação; ele serializa apenas a fronteira **física → decisão**.

Ordem entre os quatro agentes também é fixa: eles são componentes da `Station` e o
`BaseClass::updateData()` percorre o `PairStream` na ordem de declaração do `.epp`.

### 12.5 O que o `check` prova (e o que não prova)

`make check-single-thread` roda 2000 frames com **1, 2 e 4 threads** (mais uma repetição de 4) e
compara: os quatro dumps são **byte a byte idênticos**.

- **Prova**: que o paralelismo do pool T/C e as regras de fusão/desempate da poc não introduzem
  dependência de ordem. Trocar 1 por 4 threads não muda um decimal.
- **Não prova**: que o modo de tempo real é reprodutível. Ele não é, pelos motivos de 12.3.

> **Onde isso deixa a poc:** o determinismo aqui é propriedade do **harness**, não do modelo —
> vale enquanto o laço mantiver `tcFrame()` e `updateData()` em lockstep. A
> **[poc/multi-thread](../multi-thread/)** é exatamente este subprojeto com a decisão movida para
> dentro da fase 3, para medir a diferença em vez de argumentar sobre ela. Resultado medido: em
> passo fixo as duas produzem estado **idêntico**; em tempo real, a poc/multi-thread decide a
> 50 Hz em vez de 10 Hz e dispensa o artifício.

---

## 13. Armadilhas encontradas rodando

Tudo abaixo foi medido com o binário, não deduzido. As específicas de terreno estão em
[10.4](#104-armadilhas-confirmadas-no-fonte).

### 13.1 `models::JSBSimModel` é `final`

Não dá para estender para corrigir nada. Ou se usa como está, ou se escreve outro adaptador
falando direto com a `JSBSim::FGFDMExec`.

### 13.2 O `JSBSimModel` nativo nunca liga os motores

Ele não escreve `propulsion/set-running` (nem magneto/mistura/partida). Numa **turbina** isso
passa despercebido — o `FGTurbine` parte com o vento relativo em voo (medido: 14 230 lb de
empuxo sem nenhum comando de partida). Num motor a **pistão** a aeronave nasce planando:

```
thrust = 0 lb, velocidade 160 → 80 kts em 40 s,
altitude hold empinando o nariz até a perda de sustentação
```

Como a classe é `final`, a correção teve que ir para a **data da aeronave**:
`data/jsbsim/systems/engine-autostart.xml`.

> Cuidado documentado no próprio arquivo: escrever `propulsion/set-running` (o global) por um
> canal do FCS **reinicializa o motor a cada frame** — empuxo oscilando entre valores negativos e
> 900 lb.

### 13.3 O autopilot do c310 não fecha malha de velocidade

O `c310ap.xml` original apenas **declara** `ap/airspeed_hold` e `ap/throttle-cmd-norm`, sem canal
que os implemente. Como o MIXR já escreve `ap/airspeed_setpoint`, bastou acrescentar o canal
`AP Autothrottle` (PID → manete) à cópia vendorizada — de novo em **dados**, não em C++.

[`app/Fleet.cpp`](src/app/Fleet.cpp) complementa fixando a potência de cruzeiro por
`AirVehicle::setThrottles()`: a velocidade passa a ser **resultado**, não comando.

### 13.4 O gravador nativo **segfalta** ao gravar pista nova

Assim que o radar cria a primeira pista:

```
AirTrkMgr::processTrackList()            (AirTrkMgr.cpp:351)
  → AbstractDataRecorder::recordData(REID_NEW_TRACK)
    → DataRecorder::recordNewTrack()     (DataRecorder.cpp:654)
      → __dynamic_cast                   → SEGV
```

Stack obtido com AddressSanitizer. É a mesma família de defeito do `REID_WEAPON_RELEASED`, e o
contorno é o mesmo: habilitar apenas os tokens que o `TacviewOutput` usa, já que `isDataEnabled()`
é testado **antes** de chamar o handler quebrado:

```
enabledList: [ 43 42 ]      // 43 = REID_PLAYER_DATA, 42 = REID_PLAYER_REMOVED
```

### 13.5 O handler default de `DATALINK_MESSAGE` não enfileira nada

Duas tentativas erradas antes de achar o gancho certo:

| tentativa | resultado medido |
|---|---|
| drenar `receiveMessage()` na fase 2 | 0 alertas em 90 s, com 1113 transmissões |
| sobrescrever `queueIncomingMessage()` | 2016 chamadas de `onDatalinkMessageEvent` contra **0** de `queueIncomingMessage` |

O cabeçalho do `Datalink` já avisava: o handler *"passa as mensagens aos subcomponentes"*. A
`inQueue` é do caminho de **rádio/rede**, não da entrega local. O gancho correto é
**`onDatalinkMessageEvent()`**.

### 13.6 `UbfAgentTC` não é registrado por nenhuma factory do MIXR

`base/factory.cpp` registra apenas `UbfAgent` e `UbfArbiter`. Um agente de tempo crítico só
existe em EDL se a aplicação registrar o seu. Esta poc usa o `SimAgent` — e é por isso que a
decisão roda no laço de background. A [poc/multi-thread](../multi-thread/) paga esse preço.

### 13.7 O `Agent` não propaga o ciclo de componentes

Nem `updateData()` nem `reset()` chegam ao `state`/`behavior` (e um comportamento dentro do
`UbfArbiter` está dois níveis abaixo). Sintoma medido: os planos de voo ficavam com os
**defaults** do `domain::PatrolPlan` em vez dos valores dos slots, sem erro nenhum. Por isso o
`BtBehavior` configura os planos **preguiçosamente**, no primeiro `genAction()`.

### 13.8 `BT::BehaviorTreeFactory::createTreeFromFile()` não é reentrante

Os quatro aviões chegam ao primeiro `genAction()` ao mesmo tempo, em threads T/C diferentes.
`BtBehavior.cpp` protege a construção com um mutex de arquivo.

---

## 14. Controle de tempo — acelerar, frear, pausar

O cenário declara **`( ClockStation )` no lugar de `( Station )`** — uma `simulation::Station` com
um único *override*, vinda de [shared/xclock](../../shared/xclock/). Trocar de volta para
`( Station )` continua rodando, só sem as teclas (o `main.cpp` avisa e segue).

Teclas: `+`/`=` acelera, `-`/`_` freia, `espaço`/`p` pausa, `1` volta a tempo real, `h` ajuda.
Escala em degraus `0.10x … 64x`.

**A divisão entre nativo e próprio é deliberada:**

- **Acelerar é 100 % nativo.** `Station::processTimeCriticalTasks()` já faz
  `for (jj=0; jj < getFastForwardRate(); jj++) tcFrame(dt)` (`Station.cpp:506-511`), e
  `setFastForwardRate()` é público e virtual. Nada foi escrito para isso.
- **Frear não existe no framework** — `fastForwardRate` é `unsigned int` (só multiplica) e não há
  setter público de `tcRate` em runtime. É a **única** coisa acrescentada: abaixo de `1x`, um
  `tcFrame(dt * fator)` com o `dt` encurtado. Passo de integração menor, nunca maior.
- **Pausar é nativo, por um caminho não óbvio.** Não existe `Simulation::pause()`; o que existe é
  o flag de freeze do `base::Component`, e **ele não se propaga para os filhos** — a cascata é por
  *consulta*, no sentido inverso: `Player::isFrozen()` testa o próprio flag **ou** o da simulação
  (`Player.cpp:445-448`), `System::isFrozen()` testa o próprio **ou** o do ownship
  (`System.cpp:52-56`), e `Player::dynamics()` repassa ao `DynamicsModel` (`Player.cpp:2773`), que
  põe a JSBSim em hold (`JSBSimModel.cpp:657`). Por isso `setPaused()` age em `getSimulation()`,
  **não** na `Station`.

> **Armadilha confirmada rodando:** marcar o freeze **não para o relógio de execução**.
> `Simulation::updateTC()` faz `execTime += dt` na **linha 462**, *antes* do
> `if (isFrozen()) dt0 = 0.0` da linha 498, e com o `dt` cru. Medido: mundo parado, `sim=` ainda
> subindo — e isso vazaria para o Tacview, que data cada linha ACMI com `exec_time`. Correção:
> quando pausado, **não chamar `tcFrame()`**. O flag continua marcado porque é ele que congela o
> *outro* caminho, o de background.

> **Limite conhecido:** `ubf::Agent::updateData()` chama `controller(dt)` sem consultar
> `isFrozen()` (`Agent.cpp:59-62`) — com a simulação pausada, o `SimAgent` continua avaliando
> sobre um mundo estático (nada se move; a decisão só não para). O `FlightAgentTC` da
> [poc/multi-thread](../multi-thread/) decide na fase 3, dentro do frame, então para junto.

`-deterministic` **não é afetado**: chama `station->tcFrame(dt)` direto, sem passar por
`processTimeCriticalTasks()`.

Sem TTY (pipe, redirecionamento, CI) o `tcgetattr()` falha, `isActive()` fica `false` e a
simulação roda normalmente, só sem teclado.

---

## 15. Como verificar tudo

```bash
# build + execução (Ctrl+C encerra)
make build && make run-single-thread

# determinismo: 1, 2 e 4 threads produzem o mesmo estado
make check-single-thread

# o terreno chegou? elev= e agl= têm de ser plausíveis e NÃO-ZERO
./build/src/single-thread/src/single-thread -deterministic 300 | grep "^frame=300 "

# o piso de terreno está vivo? (controle negativo, sem recompilar)
sed 's/terrainClearance: ( Meters 800 )/terrainClearance: ( Meters 0 )/' \
    src/single-thread/configs/scenario.epp.in > /tmp/sem-piso.epp.in
./build/src/single-thread/src/single-thread -deterministic 12000 | grep '^frame=' > /tmp/com.txt
./build/src/single-thread/src/single-thread -deterministic 12000 -f /tmp/sem-piso.epp.in \
    | grep '^frame=' > /tmp/sem.txt
diff -q /tmp/com.txt /tmp/sem.txt      # DEVEM diferir

# o que o replay recebeu
grep -o "Name=[^,]*,Type=[^,]*,Color=[^,]*,CallSign=[^,]*" \
     src/single-thread/data/recordings/mission.acmi | sort -u

# depuração com AddressSanitizer só neste alvo
meson configure build -Dasan=true && meson compile -C build single-thread
```

O status impresso a cada 2 s traz, por aeronave: altitude, **elevação do terreno**, **AGL**,
rumo, banco, velocidade, empuxo, mach, G, combustível, o rótulo do comportamento vencedor, o que
o `Autopilot` está comandando, a pista mais próxima (já filtrada por lado) e o alerta recebido:

```
falcon2  alt= 2022m elev= 906m agl= 1117m hdg= 51deg roll=-30deg spd=141kt ...
         bt=EVADE    ap(hdg=132,alt=5595ft,spd=185) pista=bandit1@12.5NM
```

> Nessa linha dá para ver o piso de terreno atuando: `ap(alt=5595ft)` = 1705 m ≈ elevação de
> entrada + 800. Sem o piso, o alvo cru seria 1322 m (4338 ft).

---

## 16. O que a poc responde

- **O framework dirige pela herança e encontra pela árvore de componentes**: o `.epp` diz *onde*
  o objeto está, a classe-base diz *quando* ele é chamado, e o `virtual` diz *o que* roda.
- **Herdar tudo o que dá deixa a aplicação com 6 classes próprias** — nenhuma delas player,
  dinâmica, controle ou sensor — e traz junto coisas que ninguém escreve por gosto: equação do
  radar, correlação de pistas, transporte de datalink, limites de autopilot, banco de elevação.
- **O preço aparece nas bordas**: a aeronave tem que ser uma que o autopilot nativo consiga
  comandar, a classe da dinâmica é `final`, os motores não ligam sozinhos, o radar não filtra
  lado, o datalink não filtra alcance, a consulta de terreno mente quando está fora da célula, a
  decisão sai do tempo crítico e não sobra lugar natural para guardar o estado que é da aplicação.
- **O que é decisão continua sendo seu**: percepção, política, atuação e as regras de
  fusão/determinismo. O UBF define os papéis; ele não os preenche.
- **E o mundo físico impõe limites que a política não pode ignorar**: o piso anti-CFIT é correto
  desde a primeira versão, mas só vira comportamento observável quando a razão de subida da
  aeronave e a duração da manobra permitem alcançá-lo. Medir foi o que separou "implementado" de
  "funcionando".
