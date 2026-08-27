# 13-native-stack

Quatro aviões patrulham quadrantes distintos; um intruso cruza a área; **quem detecta avisa os
outros** pelo datalink, e quem recebe o aviso muda de comportamento e vai apoiar.

A regra de projeto do subprojeto é uma só: **herdar do MIXR tudo o que o framework já tem
pronto**. Player, dinâmica 6-DOF, controle de voo, radar, datalink e agente são todos nativos.
O que continua sendo nosso é o que o framework, por definição, não fornece: a **política** —
percepção, decisão, atuação — e a carga útil da mensagem trocada entre os aviões.

```bash
make build
make run-native-stack        # Tacview Real-Time Telemetry na porta 1234; Ctrl+C encerra
make check-native-stack      # verifica o determinismo (1, 2 e 4 threads T/C)
```

> **Rode sempre a partir da raiz do repositório**: o cenário, os dados do JSBSim e a gravação
> `.acmi` são resolvidos por caminho relativo (`./src/13-native-stack/...`).

---

## Índice

1. [O que vem do framework e o que é nosso](#1-o-que-vem-do-framework-e-o-que-é-nosso)
2. [Como o framework chama o nosso código](#2-como-o-framework-chama-o-nosso-código)
3. [Anatomia de um frame](#3-anatomia-de-um-frame)
4. [A árvore de objetos do cenário](#4-a-árvore-de-objetos-do-cenário)
5. [Cada peça, uma a uma](#5-cada-peça-uma-a-uma)
6. [A cadeia de decisão: UBF + BehaviorTree](#6-a-cadeia-de-decisão-ubf--behaviortree)
7. [Interação entre players](#7-interação-entre-players)
8. [Determinismo](#8-determinismo)
9. [Tacview](#9-tacview)
10. [Armadilhas encontradas rodando](#10-armadilhas-encontradas-rodando)
11. [Estrutura de arquivos](#11-estrutura-de-arquivos)
12. [Como verificar tudo](#12-como-verificar-tudo)
13. [O que a poc responde](#13-o-que-a-poc-responde)
14. [Controle de tempo — acelerar, frear, pausar](#14-controle-de-tempo--acelerar-frear-pausar)

---

## 1. O que vem do framework e o que é nosso

| peça | de onde vem |
|---|---|
| player | **`( Aircraft )`** — nativo |
| dinâmica 6-DOF | **`( JSBSimModel )`** — nativo (adaptador do JSBSim) |
| controle de voo | **`( Autopilot )`** — nativo |
| sensor | **`( Gimbal/Antenna + Tws + AirTrkMgr )`** — nativo |
| transporte da interação | `models::Datalink` — nativo; só o gancho de recepção é nosso (`xnative::AlertDatalink`) |
| agente do UBF | **`( SimAgent )`** — nativo |
| árbitro do UBF | **`( UbfArbiter )`** — nativo |
| percepção / decisão / atuação | **nossas** (`FlightState` / `BtBehavior` / `FlightAction`) |

A factory própria tem **6 classes**, e nenhuma delas é player, dinâmica, controle ou sensor:

```cpp
// src/xnative/factory.cpp
AlertDatalink   TacticalAlert                    // datalink derivado + carga útil
FlightState     BtBehavior                       // percepção + decisão
AltitudeSafetyBehavior  FlightAction             // regra dura + atuação
```

O que **não** dá para herdar, e por quê: `models/` estende o UBF apenas com `SimAgent` e
`MultiActorAgent`. Não existem `AbstractState`, `AbstractBehavior` ou `AbstractAction` concretos
no framework — a política é, por definição, da aplicação.

---

## 2. Como o framework chama o nosso código

Não existe registro nem callback. O MIXR dirige o nosso código porque **duas** coisas são
verdade ao mesmo tempo:

1. **o objeto está na árvore de componentes** (foi o `.epp` que o colocou lá), e
2. **a classe herda de algo que o framework já sabe dirigir**.

O despacho é virtual puro e simples:

```
Component::updateTC(dt)          percorre a lista de componentes e chama tcFrame()
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
| `SimAgent` | componente da **Station**; o ciclo do UBF roda em `updateData()` (background) |
| `FlightState`/`BtBehavior`/`FlightAction` | **não** são chamados pelo ciclo de componentes: quem os chama é `Agent::controller()` |
| `TacviewOutput` | elo da cadeia de `OutputHandler`s do `DataRecorder`, drenada por `Station::updateData()` |

> **Detalhe do `dt`:** a lista de players é percorrida 4× por frame, cada vez com `dt/4`. Como
> cada método de fase roda em **uma** dessas passagens, `Player::updateTC()` e
> `System::updateTC()` recompõem `dt*4` no ponto do despacho. Um `dynamics()` recebe o dt do
> frame inteiro.

---

## 3. Anatomia de um frame

```
thread de tempo crítico (PeriodicThread, dt = 1/tcRate FIXO)
└─ Station::tcFrame(dt) → Simulation::updateTC(dt)
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
   │
   ├─ FASE 2  "sensores recebem"
   │    Radar::receive()          → detecções
   │    AlertDatalink::receive()  → PROMOVE o alerta encenado (nosso)
   │
   └─ FASE 3  "lógica e controle"
        AirTrkMgr::process()  → cria/atualiza as pistas
        Autopilot::process()  → erro de rumo/altitude/velocidade → setCommanded*()
                                 no dynamics model

thread de background (laço de `app/RealTimeRun.cpp`, bgRate = 10 Hz)
└─ Station::updateData(dt)
   ├─ SimAgent::updateData() → Agent::controller()
   │     ├─ FlightState::updateState(ator)      PERCEPÇÃO
   │     ├─ UbfArbiter::genAction()             DECISÃO (por voto)
   │     │     ├─ AltitudeSafetyBehavior  (voto 90)
   │     │     └─ BtBehavior              (voto 50) → tick da árvore
   │     └─ FlightAction::execute(ator)         ATUAÇÃO → Autopilot + Datalink
   └─ DataRecorder::processRecords() → TacviewOutput → stream/arquivo ACMI
```

Repare onde a decisão foi parar: **na thread de background**. `SimAgent` deriva de `ubf::Agent`,
cujo ciclo roda em `updateData()`. Manter a decisão dentro da fase 3 do tempo crítico exigiria um
agente próprio — o framework não traz um pronto (`UbfAgentTC` existe como classe, mas
**nenhuma factory do MIXR o constrói**).

> Essa é a decisão de projeto com mais consequência na poc: as duas caixas acima são **threads
> diferentes, com relógios diferentes**, e é daí que sai a única ameaça real ao determinismo — e o
> artifício que a neutraliza. Ver [seção 8](#8-determinismo).

---

## 4. A árvore de objetos do cenário

`configs/scenario.epp.in` monta isto (o `@NUM_TC_THREADS@` é substituído por
`app/ScenarioTemplate.cpp` antes do parse, porque o teto depende da máquina):

```
( Station
   components:      { agent1..agent4 : ( SimAgent actorPlayerName: falconN ... ) }
   dataRecorder:    ( DataRecorder enabledList: [ 43 42 ]
                        outputHandler: ( RecorderOutputHandler
                           components: { ( TacviewOutput modelMap/typeMap/colorMap ) } ) )
   simulation: ( WorldModel
        numTcThreads: N
        players: {
           falcon1..falcon4 : ( Aircraft
              signature: ( SigSphere radius: 3.0 )        ← RCS: como os OUTROS radares o veem
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

Três coisas que valem entender nessa árvore:

- **`Player` não tem slot para subsistema nenhum.** Tudo entra por `components:` e é localizado
  **por tipo** (`Player::updateSystemPointers()`). Por isso a ordem no `.epp` é irrelevante, os
  nomes (`dynamicsModel:`, `pilot:`…) são livres, e um player sem um dado subsistema não é erro —
  é só um player que não tem aquilo (o caso do `bandit1`).
- **O agente mora na `Station`, não no player.** É assim que o `SimAgent` nativo funciona: ele
  amarra o ator pelo nome (`actorPlayerName`).
- **`signature:` é o que torna o avião detectável.** O radar de um player lê a `RfSignature` do
  outro para resolver a equação do radar. Sem ela, o avião é invisível — não importa quantos
  radares existam.

---

## 5. Cada peça, uma a uma

### 5.1 `( Aircraft )` — o player

Classe nativa (`models::Aircraft : AirVehicle : Player`). Fornece de graça: posição/atitude/
velocidade nos três referenciais (NED, geodésico, ECEF), o ciclo de 4 fases, a emissão de
`REID_PLAYER_DATA` para o gravador, a descoberta de subsistemas por tipo e os acessores de
telemetria que delegam ao dynamics model (`getMach()`, `getGload()`, `getFuelWt()`,
`getAngleOfAttack()`…).

**O que o player nativo não oferece:** um lugar para guardar o que é da aplicação. O rótulo do
comportamento vencedor (`PATROL`/`EVADE`/`SUPPORT`) não tem campo onde morar e ficou num quadro
de status por id de player (`include/xnative/BehaviorBoard.hpp`).

### 5.2 `( JSBSimModel )` — a dinâmica 6-DOF

Adaptador nativo do JSBSim. Roda na fase 0, chamado por `Player::dynamics()`.

Implementa a interface de comando do `DynamicsModel` (`setCommandedHeadingD/Altitude/
VelocityKts`) escrevendo nas propriedades JSBSim **`ap/heading_setpoint`**,
**`ap/altitude_setpoint`** e **`ap/airspeed_setpoint`** (mais os respectivos `*_hold`) —
confirmado com `strings libmixr_models.so`.

> **Consequência que define a poc:** esses comandos só têm efeito se o **modelo JSBSim** da
> aeronave tiver um autopilot próprio implementando essas propriedades. A maioria das aeronaves
> distribuídas com o JSBSim **não tem** (aí a aplicação precisa escrever as próprias leis de
> controle); o **c310** tem (`<autopilot file="c310ap">`). Foi essa a razão técnica da escolha da
> aeronave — não estética.

### 5.3 `( Autopilot )` — o controle

Classe nativa (`models::Autopilot : Pilot`), no slot `pilot:`. Roda na fase 3 e converte os alvos
(rumo, altitude, velocidade) em chamadas ao dynamics model, respeitando os limites declarados no
EDL:

```
maxRateOfTurnDps / maxBankAngle / maxPitchAngle / maxClimbRateMps / maxAcceleration
```

É o que dispensa escrever à mão a malha de rumo→banco→aileron, a malha de altitude→arfagem→
profundor com termo integral e os sinais de convenção do JSBSim.

**Unidades:** `setCommandedAltitudeFt()` é em **pés**; o resto da poc trabalha em metros. A
conversão fica em `FlightAction::execute()`, na fronteira.

### 5.4 O radar nativo — `Gimbal` + `Antenna` + `Tws` + `AirTrkMgr`

```
antennas: ( Gimbal { radar: ( Antenna ... ) } )   varredura mecânica + padrão de ganho
sensors:  ( SensorMgr { ( Tws antennaName: radar trackManagerName: twsTrkMgr ) } )
obc:      ( OnboardComputer { twsTrkMgr: ( AirTrkMgr ) } )
```

Zero linhas de detecção, RCS ou correlação de pista da nossa parte: fases 1 e 2 fazem
emissão/recepção, e o `AirTrkMgr` (fase 3) mantém a lista de pistas.

O padrão de ganho está **embutido** no `.epp`; trazê-lo de um `#include` exigiria um passo de
preprocessador C antes do `edl_parser`.

> **O radar nativo não separa amigo de inimigo.** `playerOfInterestTypes: { air }` filtra por
> **tipo** de player, não por lado: a esquadrilha inteira aparece na lista de pistas. O filtro
> amigo/inimigo é decisão tática e ficou onde deve estar — em `xnative::TrackQuery`, uma consulta
> só, usada tanto pelo `FlightState` quanto pelo status/dump da aplicação.

### 5.5 `xnative::AlertDatalink : models::Datalink` — a interação

A única classe MIXR que ainda escrevemos, e mesmo assim herdando quase tudo. Da classe base vêm:
`sendMessage()` (varre os players e entrega por evento), a tabela de eventos que aceita
`DATALINK_MESSAGE`, a fila de saída para a rede e a integração com `Player::getDatalink()`. Nossa
parte é **o que fazer com a mensagem** — ver a [seção 7](#7-interação-entre-players).

> **O que a classe base *não* faz:** sem `radioName:`, `sendMessage()` **não filtra alcance nem
> lado** — é broadcast para todo player local ativo. O slot `maxRange` existe, mas
> `noRadioMaxRange` nunca é lido em `Datalink.cpp`. Detalhe em [7.4](#74-canal-2--datalink).

### 5.6 `( SimAgent )` + `( UbfArbiter )` — o agente e o árbitro

`SimAgent` (nativo) resolve o ator por nome e executa o ciclo percepção→decisão→atuação.
`UbfArbiter` (nativo) é ele próprio um `AbstractBehavior` que consulta os comportamentos filhos e
devolve a ação de **maior voto**.

### 5.7 As peças que continuam nossas

| classe | papel no UBF | onde |
|---|---|---|
| `FlightState : ubf::AbstractState` | percepção → um `Snapshot` de números crus | `src/ubf/FlightState.cpp` |
| `BtBehavior : ubf::AbstractBehavior` | decisão, com a árvore BT.CPP como política interna | `src/ubf/BtBehavior.cpp` |
| `AltitudeSafetyBehavior : ubf::AbstractBehavior` | regra dura (piso de altitude), voto maior | `src/ubf/AltitudeSafetyBehavior.cpp` |
| `FlightAction : ubf::AbstractAction` | atuação: comanda `Autopilot` e `AlertDatalink` | `src/ubf/FlightAction.cpp` |
| `TacticalAlert : base::Object` | carga útil da mensagem de datalink | `src/xnative/TacticalAlert.cpp` |
| `domain/` | regras puras: patrulha, RTB, evasão, geometria | `src/domain/` |
| `bt/` | os 7 nós da árvore + factory | `src/bt/` |

O `domain/` **não conhece MIXR nem BehaviorTree.CPP** — é testável sozinho.

---

## 6. A cadeia de decisão: UBF + BehaviorTree

O UBF define três papéis desacoplados, mas não diz *como* decidir. Aqui a política de um dos
comportamentos é uma árvore do BehaviorTree.CPP:

```
SimAgent::controller(dt)
  ├─ FlightState::updateState(ator)     lê Aircraft, TrackManager e Datalink
  ├─ UbfArbiter::genAction(state, dt)
  │    ├─ AltitudeSafetyBehavior  vote 90  → ação só se altitude < piso
  │    └─ BtBehavior              vote 50  → tickRoot() da árvore
  │         └─ preenche um bt_nodes::FlightDecision
  │              (comando de voo + rótulo + pedido de transmissão)
  └─ FlightAction::execute(ator)        → Autopilot + AlertDatalink
```

A árvore (`configs/flight_tree.xml`):

```xml
<Fallback>
  <Sequence> <FuelLow margin="0.05"/>  <ReturnToBase/>   </Sequence>
  <Sequence> <ContactDetected/>        <ReportAndEvade/> </Sequence>
  <Sequence> <AlertReceived/>          <SupportAlert/>   </Sequence>
  <Patrol/>
</Fallback>
```

Pontos de projeto:

- **Nenhum nó toca em objeto MIXR.** Os nós leem o `Snapshot` e preenchem o `FlightDecision`;
  quem atua é a `FlightAction`. Os mesmos nós serviriam a outro atuador — ou a um teste unitário
  sem simulação.
- **Injeção de dependência pelo construtor**, via `factory.registerBuilder<T>(ID, builder)` — a
  forma documentada pelo autor da BT.CPP v3 para argumentos extras (a sobrecarga variádica de
  `registerNodeType` só existe em versões posteriores; aqui a lib é a 3.5.6). O blackboard **não**
  é usado como saco de ponteiros.
- **Port de verdade onde faz sentido:** `FuelLow` declara `InputPort<double>("margin", 0.0, ...)`
  e o XML passa `margin="0.05"`. A reserva do tanque é propriedade do **veículo** (slot EDL); a
  margem de decisão é propriedade da **árvore** (atributo XML).
- **Uma regra dura não precisa virar ramo da árvore.** O `AltitudeSafetyBehavior` é irmão do
  `BtBehavior` no árbitro nativo, com voto maior: quando o avião fura o piso, a ação dele vence
  sem que a árvore saiba que ele existe.

---

## 7. Interação entre players

Esta é a seção que responde "como dois players conversam" — não só nesta poc, mas no MIXR em
geral. A resposta cabe em uma frase:

> **Um player nunca chama método de outro player.** Ele entrega um `base::Object*` ao outro
> chamando `event()` nele — síncrono, dentro da própria pilha de chamada, **na thread de quem
> emitiu**.

Radar, datalink, colisão, IR, kill: **tudo** termina na mesma linha. O que muda entre os canais é
só (a) quem varreu a lista de players para achar o destinatário e (b) o que vai no payload.

### 7.1 A primitiva: `Component::event(token, Object*)`

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

### 7.2 Por que não é ponteiro direto

Você *pode* pegar o ponteiro do outro player — `getPlayers()` é público, e o `Autopilot` nativo
guarda um (`Autopilot.hpp:299`, `const Player* lead`, **sem `ref()`**). O que você não pode é
**agir** através dele, e o motivo é a [anatomia do frame](#3-anatomia-de-um-frame).

A lista de players é varrida **4 vezes por frame**, uma por fase (`Simulation.cpp:544-568`), e os
players podem estar em **threads diferentes** do pool T/C. Então, do seu componente:

- **ler** o outro player é corrida — ele pode estar no meio de `dynamics()` em outra thread, com
  a posição meio escrita;
- **escrever** nele é pior — você escreve fora da ordem determinística do frame, e o resultado
  passa a depender do escalonador.

`event()` sozinho **não** resolve a corrida: ele também roda na thread do emissor. O que ele dá é
um **ponto único de entrada**, que o receptor pode disciplinar. É exatamente o que o
`AlertDatalink` faz, e é o que sustenta o `make check-native-stack`:

| passo | onde roda | por quê |
|---|---|---|
| `onDatalinkMessageEvent()` só **encena** (`staged`), sob mutex curto | thread do **emissor** | seção crítica mínima; nenhuma decisão aqui |
| a fusão é **comutativa** (vence o mais próximo; empate → menor `senderId`) | idem | o resultado independe da **ordem de chegada**, que é do escalonador |
| `receive()` (fase 2) promove `staged → current` | thread do **receptor**, na fronteira de fase | latência **fixa** de 1 frame para todos |

Com ponteiro cru e escrita imediata, nenhuma das três propriedades existiria. E há o problema
banal por cima: ponteiro cru para um player que sai da lista fica pendurado — só `isActive()` e
`getDamage()` protegem o `lead` do `Autopilot`.

### 7.3 Canal 1 — RF: emissão, eco, pista

O canal mais elaborado, e o único em que o framework **modela física**. São **dois** percursos de
lista, em threads diferentes — essa é a parte não óbvia.

**Filtro (thread de fundo, fora do frame).** `RfSystem::updateData` (`:165`) pega
`sim->getPlayers()` (`:195`) e desce até `Tdb::processPlayers` — **o laço**, em `Tdb.cpp:280`.
Ali são aplicados os filtros que você declarou no `.epp`: `maxPlayersOfInterest`,
`playerOfInterestTypes`, `maxRange2PlayersOfInterest`, `maxAngle2PlayersOfInterest`, horizonte e
oclusão de terreno. Quem sobrevive vira `targets[]`.

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
  limiar do passo 7 e o avião simplesmente não existe para o radar. É por isso que **todo** player
  do nosso cenário declara uma: `SigSphere radius: 3.0` nos quatro caças, `4.0` no intruso.
- **A ordem dos componentes é ordem de execução.** Dentro da fase 3, `Component::updateTC()`
  percorre os subcomponentes **na ordem declarada**. Como `sensors:` vem antes de `obc:`, o
  `Radar::process()` (que faz `newReport`) roda antes do `TrackManager::process()` (que drena) —
  e a pista aparece no **mesmo** frame. Inverta os dois no EDL e ela atrasa um frame — sem que
  nenhuma linha de C++ tenha mudado.

### 7.4 Canal 2 — datalink

`Datalink` estende `System` (não `Radio`). `sendMessage()` tem dois modos, mutuamente exclusivos:

- **com `radioName:`** → a mensagem vira payload de uma `Emission` e desce a cadeia RF inteira,
  com potência, alcance e tudo o mais;
- **sem rádio** → o `for` da lista mostrado em [7.1](#71-a-primitiva-componenteventtoken-object).

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
> na migração para cá — é aqui que eles voltariam a ser necessários.

### 7.5 Canal 3 — evento próprio

Para um canal que não é radar nem datalink, você usa um token `>= USER_EVENTS (2000)` e chama
`event()`/`send()` direto — é o que a poc/08 faz. Duas armadilhas, ambas já pagas neste repo:

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
primeira vez). Foi o bug nº 1 da poc/08, e a correção está viva lá: repare que o `send()` é
chamado **no ownship**, não no `this`:

```cpp
// 08-event-relay/src/RadarContactRelay.cpp:114-122 -- hop LOCAL (para um irmão)
models::Player* const myPlayer{getOwnship()};
myPlayer->send(localComponentName->getString(), CONTACT_EVENT, msg, localSendData);

// :127-145 -- hop REMOTO: mesma primitiva, chamada no player achado por nome
base::Pair* p{players->findByName(relayToPlayerName->getString())};
const auto remote = dynamic_cast<models::Player*>(p->object());
remote->send(relayToComponentName->getString(), CONTACT_EVENT, msg, remoteSendData);
```

Bônus de `send()` que quase ninguém nota: as sobrecargas escalares (`int`, `double`, `bool`…) só
disparam se o valor **mudou** — é um filtro de mudança embutido. A sobrecarga com `Object*` não
filtra.

### 7.6 Player que nasce em runtime

Interação também acontece com quem ainda não existia. Só **três** lugares no framework inteiro
chamam `addNewPlayer()`, e dois são `AbstractWeapon` — é assim que chaff, flare e mísseis viram
players (a poc/09 usa esse caminho):

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
é essa ordenação que autoriza o `break` antecipado do `Datalink` em 7.1.

Consequência: **um player criado só passa a existir para os outros no próximo `updateData()`**.

### 7.7 O caminho desta poc, fim a fim

É o mecanismo pedido pelo enunciado: um evento de um player que muda o comportamento dos outros.

```
falcon3 detecta bandit1 (radar nativo -- 7.3)
   └─ árvore: ContactDetected → ReportAndEvade
        └─ FlightDecision.broadcastAlert = true (+ posição absoluta do contato)
             └─ FlightAction::execute()
                  └─ AlertDatalink::broadcastAlert()
                       └─ Datalink::sendMessage(TacticalAlert*)     [NATIVO -- 7.4]
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
   do frame seguinte, dando **latência fixa de um frame** para todos, em vez de "às vezes no
   mesmo frame, às vezes no próximo".

É a mesma disciplina que o framework usa entre as suas 4 fases: escreve numa fase, publica na
fronteira, lê na fase seguinte.

### 7.8 Receita: como escrever a sua

Escolha primeiro **de qual canal** você precisa:

| você quer… | use | custo em C++ |
|---|---|---|
| detecção física realista (RCS, potência, ruído, pistas filtradas) | RF nativo: `Antenna` + `Radar`/`Tws` + `AirTrkMgr` no `.epp` | **zero** |
| trocar uma **carga própria** entre players, sem modelar propagação | subclasse de `models::Datalink` | ~100 linhas — molde: [`AlertDatalink`](src/xnative/AlertDatalink.cpp) |
| um canal com regra própria (alcance, lado, quem ouve quem) | componente próprio + evento `>= 2000` | tudo seu — moldes: `12/AlertRadio`, `08/RadarContactRelay` |

O caminho do meio, que é o mais barato, em cinco passos:

1. **A carga** — um `base::Object` com `DECLARE_SUBCLASS`/`IMPLEMENT_SUBCLASS`
   ([`TacticalAlert`](src/xnative/TacticalAlert.cpp)). Não use `models::Message`: ela existe no
   framework e **ninguém a usa**; `sendMessage()` recebe `base::Object*` opaco.
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
- Nome errado em qualquer slot `*Name:` = componente inerte, **sem diagnóstico**. Vale para
  `leadPlayerName`, `antennaName`, `trackManagerName`, `actorPlayerName`.

---

## 8. Determinismo

**Determinístico** aqui quer dizer: rodar o mesmo cenário duas vezes e obter o mesmo estado, no
mesmo frame, até o último decimal. É exatamente o que `make check-native-stack` compara — e não é
de graça, porque a poc roda **em paralelo** em dois sentidos diferentes.

### 8.1 Dois paralelismos, um problema

| paralelismo | o que é | quebra o determinismo? |
|---|---|---|
| **pool T/C** (`numTcThreads: N`) | a lista de players é fatiada entre N threads dentro de `Simulation::updateTC()` | **não** — ver 8.2 |
| **decisão × física** | `SimAgent` decide em `updateData()` (laço do `main`, 10 Hz) enquanto `tcFrame()` roda na thread periódica (50 Hz) | **sim** — ver 8.3 |

O primeiro é o paralelismo que a poc *quer* (é para isso que existe o `-threads N`). O segundo é
um efeito colateral de herdar o `SimAgent` nativo — e é o que o artifício da seção 8.4 remove.

### 8.2 Por que o pool T/C **não** quebra nada

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
   que garante isso (escreve numa fase, publica na fronteira, lê na fase seguinte).
2. **A barreira fecha a fase para todos ao mesmo tempo.** Nenhum player pode "adiantar" a fase 2
   enquanto outro ainda está na fase 1. O estado no início de cada fase é sempre o mesmo,
   independentemente de quantas threads existam e de quem terminou primeiro.

Do nosso lado, três decisões conscientes fecham as brechas que sobrariam:

- **Passo fixo.** No Linux, `PeriodicThread::mainThreadFunc()` calcula `dt = 1/rate` uma vez e
  passa esse mesmo valor todo frame (o flag de delta variável é só do Windows e nasce desligado).
  O relógio de parede decide apenas *quando* o frame roda, nunca *quanto* ele avança.
- **Fusão comutativa dos alertas** (seção 7). O handler de datalink roda na thread do **emissor**:
  se dois aviões avisam no mesmo frame, a ordem de chegada depende do escalonador. Vence o contato
  de menor distância e, em empate exato, o emissor de menor id — o resultado não depende da ordem.
- **Escolha da pista sem depender da lista.** Menor distância e, em empate exato, menor id de
  pista — para não herdar a ordem interna do `TrackManager`.

Nada na poc usa relógio, sorteio ou identidade de thread para decidir.

### 8.3 O que **de fato** quebraria: a decisão fora do frame

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
   (dinâmica) e a fase 3 (controle), com o estado meio atualizado.
2. **Quantas decisões acontecem por segundo simulado varia** com jitter e drift do `msleep`.
3. **Leitura sem sincronização**: os acessores do `Player` não têm lock, então o `FlightState` lê
   posição/atitude enquanto a thread T/C as escreve.

Nenhum desses três se repete igual na execução seguinte.

### 8.4 O artifício: colapsar as duas threads numa só

O modo `-deterministic N` (`src/app/DeterministicRun.cpp`) **não sobe a thread periódica**. O mesmo thread
chama as duas coisas, em ordem fixa, com passo fixo:

```cpp
const double dt{1.0 / station->getTimeCriticalRate()};   // 0.02 s — o MESMO passo dos dois lados
for (long frame = 1; frame <= frames; ++frame) {
   station->tcFrame(dt);      // 1) frame inteiro: 4 fases, pool T/C com suas barreiras
   station->updateData(dt);   // 2) SÓ ENTÃO os SimAgents decidem, sobre estado assentado
}
```

O que essa troca de três linhas compra, ponto a ponto contra a seção 8.3:

| defeito | como o artifício o remove |
|---|---|
| ponto de amostragem arbitrário | a decisão só roda **depois** do frame inteiro fechado — sempre no mesmo ponto |
| número variável de decisões | uma decisão por frame, sempre; `dt` de `updateData` = `dt` de `tcFrame` |
| leitura concorrente do player | não há concorrência: a física já terminou quando o agente lê |

Repare que o pool T/C **continua ligado** — `-threads N` funciona igual nos dois modos. O
artifício não serializa a simulação; ele serializa apenas a fronteira **física → decisão**, que é
a única que o framework não sincroniza sozinho. O paralelismo que interessa medir permanece.

Ordem entre os quatro agentes também é fixa: eles são componentes da `Station` e o
`BaseClass::updateData()` percorre o `PairStream` na ordem de declaração do `.epp`.

### 8.5 O que o `check` prova (e o que não prova)

```bash
./build/src/13-native-stack/src/native-stack -deterministic 2000   # passo fixo, sem thread periódica
./build/src/13-native-stack/src/native-stack -threads 2            # força numTcThreads
```

`make check-native-stack` roda 2000 frames com **1, 2 e 4 threads** (mais uma repetição de 4) e
compara: os quatro dumps são **byte a byte idênticos**.

- **Prova**: que o paralelismo do pool T/C e as regras de fusão/desempate da poc não introduzem
  dependência de ordem. Trocar 1 por 4 threads não muda um decimal.
- **Não prova**: que o modo de tempo real é reprodutível. Ele não é, pelos motivos de 8.3.

> **Onde isso deixa a poc:** o determinismo aqui é propriedade do **harness**, não do modelo —
> vale enquanto o laço mantiver `tcFrame()` e `updateData()` em lockstep. Na poc/12, que usa um
> `AgentTC` próprio, a decisão roda **dentro** da fase 3 do frame e o determinismo é propriedade
> do modelo: vale também em tempo real, sem artifício nenhum. Foi esse o preço de herdar o
> `SimAgent` — o framework não registra `UbfAgentTC` em nenhuma factory (seção 10.6), então um
> agente de tempo crítico continua sendo código da aplicação.
>
> A **[poc/14](../14-tc-agent/)** é exatamente este subprojeto com essa única troca feita, para
> medir a diferença em vez de argumentar sobre ela. Resultado medido: em passo fixo as duas
> produzem estado **idêntico** (o lockstep já colocava a decisão no mesmo instante); em tempo
> real, a poc/14 decide a 50 Hz em vez de 10 Hz e dispensa o artifício.

---

## 9. Tacview

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
> com autopilot `ap/*` entre as distribuídas com o JSBSim. O `aircraft/f16` existe no JSBSim mas
> **não** tem autopilot, então trocar a dinâmica quebraria a cadeia nativa
> `Autopilot → JSBSimModel`. Trocar o ícone é uma linha no `.epp`; trocar a aeronave de verdade
> exigiria escrever as leis de controle à mão.

---

## 10. Armadilhas encontradas rodando

Tudo abaixo foi medido com o binário, não deduzido.

### 10.1 `models::JSBSimModel` é `final`

Não dá para estender para corrigir nada. Ou se usa como está, ou se escreve outro adaptador
falando direto com a `JSBSim::FGFDMExec`.

### 10.2 O `JSBSimModel` nativo nunca liga os motores

Ele não escreve `propulsion/set-running` (nem magneto/mistura/partida). Numa **turbina** isso
passa despercebido — o `FGTurbine` parte com o vento relativo em voo (medido: 14 230 lb de
empuxo sem nenhum comando de partida). Num motor a **pistão** a aeronave nasce planando:

```
thrust = 0 lb, velocidade 160 → 80 kts em 40 s,
altitude hold empinando o nariz até a perda de sustentação
```

Como a classe é `final`, a correção teve que ir para a **data da aeronave**:
`data/jsbsim/systems/engine-autostart.xml` (magnetos + partida +
`propulsion/engine[i]/set-running`).

> Cuidado documentado no próprio arquivo: escrever `propulsion/set-running` (o global) por um
> canal do FCS **reinicializa o motor a cada frame** — empuxo oscilando entre valores negativos e
> 900 lb.

### 10.3 O autopilot do c310 não fecha malha de velocidade

O `c310ap.xml` original apenas **declara** `ap/airspeed_hold` e `ap/throttle-cmd-norm`, sem canal
que os implemente. Como o MIXR já escreve `ap/airspeed_setpoint`, bastou acrescentar o canal
`AP Autothrottle` (PID → manete) à cópia vendorizada — de novo em **dados**, não em C++.

`app/Fleet.cpp` complementa fixando a potência de cruzeiro por `AirVehicle::setThrottles()` (método
do próprio framework): a velocidade passa a ser **resultado** (potência fixa + arrasto), não
comando.

### 10.4 O gravador nativo **segfalta** ao gravar pista nova

Assim que o radar cria a primeira pista:

```
AirTrkMgr::processTrackList()            (AirTrkMgr.cpp:351)
  → AbstractDataRecorder::recordData(REID_NEW_TRACK)
    → DataRecorder::recordNewTrack()     (DataRecorder.cpp:654)
      → __dynamic_cast                   → SEGV
```

Stack obtido com AddressSanitizer (`meson setup -Dasan=true build .`, que liga o sanitizer **só**
neste alvo). É a mesma família de defeito do `REID_WEAPON_RELEASED`, que aborta o processo por
outro caminho, e o contorno é o mesmo: habilitar apenas os tokens que o `TacviewOutput` usa, já
que `isDataEnabled()` é testado **antes** de chamar o handler quebrado:

```
enabledList: [ 43 42 ]      // 43 = REID_PLAYER_DATA, 42 = REID_PLAYER_REMOVED
```

### 10.5 O handler default de `DATALINK_MESSAGE` não enfileira nada

Duas tentativas erradas antes de achar o gancho certo:

| tentativa | resultado medido |
|---|---|
| drenar `receiveMessage()` na fase 2 | 0 alertas em 90 s, com 1113 transmissões |
| sobrescrever `queueIncomingMessage()` | 2016 chamadas de `onDatalinkMessageEvent` contra **0** de `queueIncomingMessage` |

O cabeçalho do `Datalink` já avisava: o handler *"passa as mensagens aos subcomponentes"*. A
`inQueue` é do caminho de **rádio/rede**, não da entrega local. O gancho correto é
**`onDatalinkMessageEvent()`**.

### 10.6 `UbfAgentTC` não é registrado por nenhuma factory do MIXR

`base/factory.cpp` registra apenas `UbfAgent` e `UbfArbiter`. Um agente de tempo crítico só
existe em EDL se a aplicação registrar o seu. Esta poc usa o `SimAgent` — e é por isso que a
decisão roda no laço de background.

### 10.7 O `Agent` não propaga o ciclo de componentes

Nem `updateData()` nem `reset()` chegam ao `state`/`behavior` (e um comportamento dentro do
`UbfArbiter` está dois níveis abaixo). Sintoma medido: os planos de voo ficavam com os
**defaults** do `domain::PatrolPlan` em vez dos valores dos slots, sem erro nenhum. Por isso o
`BtBehavior` configura os planos **preguiçosamente**, no primeiro `genAction()`.

---

## 11. Estrutura de arquivos

```
src/13-native-stack/
├── README.md                      este arquivo
├── meson.build                    só faz subdir('./src')
├── configs/
│   ├── scenario.epp.in            cenário (EDL) com @NUM_TC_THREADS@
│   └── flight_tree.xml            árvore de comportamento (BT.CPP v3)
├── data/
│   ├── jsbsim/                    dados da aeronave (vendorizados do pacote JSBSim)
│   │   ├── aircraft/c310/{c310.xml, c310ap.xml}   ← c310ap com o autothrottle acrescentado
│   │   ├── engine/{engIO470D.xml, propC10v.xml}
│   │   └── systems/{GNCUtilities.xml, engine-autostart.xml}   ← partida acrescentada
│   └── recordings/                mission.acmi
├── include/ e src/
│   ├── app/                       a aplicação, uma questão por arquivo
│   │   ├── Options.*              argv → struct (-f / -threads / -deterministic)
│   │   ├── ScenarioTemplate.*     .epp.in → .epp (resolve @NUM_TC_THREADS@)
│   │   ├── StationBuilder.*       .epp → Station de pé (edl_parser, RESET, WorldModel)
│   │   ├── Fleet.*                acha os players e fixa a potência de cruzeiro
│   │   ├── StatusReport.*         a linha de status humana (formato)
│   │   ├── DeterministicDump.*    o dump `frame=` que os `make check-*` comparam
│   │   ├── DeterministicRun.*     laço de passo fixo (-deterministic N)
│   │   └── RealTimeRun.*          laço de tempo real + teclado + Ctrl+C
│   ├── domain/                    regras puras (sem MIXR, sem BT, sem JSBSim)
│   │   ├── FlightCommand.hpp      DTO de comando (rumo/altitude/velocidade)
│   │   ├── geometry.*             wrap180/360, rumo e distância em NED
│   │   ├── PatrolPlan.*           circuito de patrulha cíclico
│   │   ├── RtbPlan.*              plano de retorno à base
│   │   └── ThreatPolicy.*         manobra de evasão
│   ├── xnative/                   o que sobrou de próprio no lado MIXR
│   │   ├── AlertDatalink.*        herda models::Datalink
│   │   ├── TacticalAlert.*        base::Object (carga do datalink)
│   │   ├── TrackQuery.*           contato hostil mais próximo (radar nativo)
│   │   ├── ThreadTag.*            índice estável da thread T/C chamadora
│   │   ├── Log.*                  console com mutex (várias threads escrevem)
│   │   ├── BehaviorBoard.*        quadro de rótulos por id de player
│   │   └── factory.*              registra as 6 classes próprias
│   ├── ubf/                       percepção / decisão / atuação
│   │   ├── FlightState.*          AbstractState  → Snapshot
│   │   ├── BtTuning.hpp           os números que o EDL ajusta no BtBehavior
│   │   ├── BtBehavior.*           AbstractBehavior (hospeda a árvore)
│   │   ├── BtBehaviorSlots.cpp    a fronteira com o EDL do BtBehavior
│   │   ├── AltitudeSafetyBehavior.*  AbstractBehavior (regra dura)
│   │   └── FlightAction.*         AbstractAction (Autopilot + Datalink)
│   ├── bt/                        adaptadores da árvore
│   │   ├── NodeContext.hpp        FlightDecision + dependência dos nós
│   │   ├── bt_factory.*           registerBuilder<T> de cada nó
│   │   └── nodes/                 FuelLow, ReturnToBase, ContactDetected,
│   │                              ReportAndEvade, AlertReceived, SupportAlert, Patrol
│   ├── mixr_factory.*             encadeia xnative → xtacview → framework
│   └── main.cpp                   só orquestra: chama os módulos de app/ na ordem
```

O `main.cpp` é fino de propósito: ele **não** pilota, **não** tica árvore e **não** monta ACMI.
Ele gera o `.epp` (só o número de threads), constrói a `Station`, chama `updateData()` no laço —
que drena o gravador e roda os `SimAgent`s — e imprime o status.

---

## 12. Como verificar tudo

```bash
# build + execução (Ctrl+C encerra)
make build && make run-native-stack

# determinismo: 1, 2 e 4 threads produzem o mesmo estado
make check-native-stack

# o que o replay recebeu
grep -o "Name=[^,]*,Type=[^,]*,Color=[^,]*,CallSign=[^,]*" \
     src/13-native-stack/data/recordings/mission.acmi | sort -u

# depuração com AddressSanitizer só neste alvo
meson configure build -Dasan=true && meson compile -C build native-stack
```

O status impresso a cada 2 s traz, por aeronave: altitude, rumo, banco, velocidade, **empuxo**,
mach, G, combustível, o rótulo do comportamento vencedor, o que o `Autopilot` está comandando, a
pista mais próxima (já filtrada por lado) e o alerta recebido. Numa sessão típica:

```
falcon3  ... bt=EVADE    ap(hdg=244,alt=13281ft,spd=190) pista=bandit1@6.2NM
falcon1  ... bt=SUPPORT  ap(hdg=49,alt=10537ft,spd=180)  alerta<-falcon3(bandit1)
falcon2  ... bt=SUPPORT  ap(hdg=3,alt=10537ft,spd=180)   alerta<-falcon3(bandit1)
falcon4  ... bt=SUPPORT  ap(hdg=68,alt=10537ft,spd=180)  alerta<-falcon3(bandit1)
bandit1  ... bt=(sem agente)
```

---

## 13. O que a poc responde

- **O framework dirige pela herança e encontra pela árvore de componentes**: o `.epp` diz *onde*
  o objeto está, a classe-base diz *quando* ele é chamado, e o `virtual` diz *o que* roda.
- **Herdar tudo o que dá deixa a aplicação com 6 classes próprias** — nenhuma delas player,
  dinâmica, controle ou sensor — e traz junto coisas que ninguém escreve por gosto: equação do
  radar, correlação de pistas, transporte de datalink, limites de autopilot.
- **O preço aparece nas bordas**: a aeronave tem que ser uma que o autopilot nativo consiga
  comandar (nem todo modelo JSBSim tem as propriedades `ap/*`), a classe da dinâmica é `final`,
  os motores não ligam sozinhos, o radar não filtra lado, a decisão sai do tempo crítico e não
  sobra lugar natural para guardar o estado que é da aplicação.
- **O que é decisão continua sendo seu**: percepção, política, atuação e as regras de
  fusão/determinismo. O UBF define os papéis; ele não os preenche.

---

## 14. Controle de tempo — acelerar, frear, pausar

Os dois executáveis leem o teclado enquanto rodam:

| tecla | efeito |
|---|---|
| `+` `=` | acelera (próximo degrau) |
| `-` `_` | freia (degrau anterior; abaixo de `1x` é câmara lenta) |
| `espaço` `p` | pausa / retoma |
| `1` | volta a tempo real e retoma |
| `h` `?` | reimprime a ajuda |

Escala em degraus: `0.10x 0.25x 0.50x 1x 2x 4x 8x 16x 32x 64x`.

A linha de status passou a mostrar os **dois relógios** lado a lado — é a diferença entre eles
que prova o efeito:

```
[t=24s sim=8.2s PAUSADO (1x)]      <- 24 s de parede, 8,2 s simulados
[t=16s sim=32.0s 4x]               <- o simulado corre quatro vezes mais rápido
```

O código vive em `shared/xclock/` — biblioteca compartilhada, mesmo padrão de `shared/xtacview`,
uma cópia só para as duas pocs. O cenário declara `( ClockStation )` no lugar de `( Station )`;
trocar de volta para `( Station )` continua rodando, apenas sem as teclas (`app/StationBuilder.cpp`
avisa e segue).

### 14.1 Acelerar já era do framework

`Station::processTimeCriticalTasks()` (`Station.cpp:506-511`) faz exatamente isto:

```cpp
for (unsigned int jj = 0; jj < getFastForwardRate(); jj++) {
   tcFrame( dt );
}
```

A cada período real da thread T/C, o tempo simulado avança N frames. `setFastForwardRate()` é
público e virtual, então muda em runtime. **Nada foi escrito para acelerar** — `ClockStation`
apenas chama esse setter e deixa a classe base rodar o laço.

### 14.2 Frear não existe — é a única coisa acrescentada

`fastForwardRate` é `unsigned int`: multiplica, nunca divide. E não há como baixar a taxa da
thread T/C em runtime — `Station` só expõe `getTimeCriticalRate()` (o setter é slot privado), e
o rate da `base::PeriodicThread` é fixado na construção.

Daí o único override da classe: abaixo de `1x`, roda **um** frame com o `dt` encurtado.

```cpp
void ClockStation::processTimeCriticalTasks(const double dt)
{
   if (isPaused()) return;

   if (slowFactor >= 1.0) {
      BaseClass::processTimeCriticalTasks(dt);   // caminho nativo, intocado
      return;
   }
   tcFrame(dt * slowFactor);                      // câmara lenta
}
```

Encurtar o `dt` é seguro: o passo de integração fica **menor**, nunca maior — a dinâmica do
JSBSim não degrada, fica mais fina.

### 14.3 Pausar é nativo, mas o freeze sozinho não basta

Não existe `Simulation::pause()`. O que existe é o flag de freeze do `base::Component`, honrado
por `Simulation::updateTC()`/`updateData()` com `if (isFrozen()) dt0 = 0.0`
(`Simulation.cpp:498` e `625`).

O detalhe que não é óbvio: **o freeze não se propaga para os filhos**. A cascata acontece por
*consulta*, no sentido inverso —

- `Player::isFrozen()` testa o próprio flag **ou** o da simulação (`Player.cpp:445-448`);
- `System::isFrozen()` testa o próprio **ou** o do ownship (`System.cpp:52-56`);
- `Player::dynamics()` repassa `isFrozen()` ao `DynamicsModel` (`Player.cpp:2773`), e o
  `JSBSimModel` põe a JSBSim em hold (`JSBSimModel.cpp:657`).

Por isso `setPaused()` age em `getSimulation()`, e não na `Station`: congelar a Station não
pararia nada disso.

**Armadilha encontrada rodando** (ver seção 10 para as outras): marcar o freeze *não* para o
relógio de execução. `Simulation::updateTC()` faz `execTime += dt` na **linha 462**, antes do
teste de freeze da linha 498 — e com o `dt` cru, não com o `dt0`. Medido: com a simulação
congelada, o mundo parava mas `sim=` continuava subindo. Isso vazaria direto para o Tacview,
que data cada linha ACMI justamente com `exec_time` (`TacviewOutput.cpp:373`) — o replay
avançaria com as aeronaves paradas.

A correção é não chamar `tcFrame()` quando pausado: o relógio de execução para junto com o
mundo, e ainda deixa de queimar CPU integrando um estado que não muda. O flag de freeze
continua marcado, porque é ele que congela o **outro** caminho, o de background
(`Simulation::updateData()`, linha 625), que não passa por `processTimeCriticalTasks()`.

### 14.4 O que foi medido

Teclas injetadas por pty (não há TTY no ambiente de desenvolvimento), amostrando a razão entre
tempo simulado e tempo de parede:

| escala pedida | razão medida |
|---|---|
| `1x` | 1.00x |
| `2x` | 2.00x |
| `4x` | 4.00x |
| `0.50x` | 0.50x |
| `0.25x` | 0.25x |
| `PAUSADO` | 0.00x |

Com a simulação pausada, oito amostras consecutivas de `falcon1` ao longo de 16 s de parede
saíram **byte a byte idênticas** — altitude, rumo, rolamento, velocidade, empuxo, combustível e
os comandos do autopilot, todos congelados.

O modo `-deterministic` não é afetado: ele chama `station->tcFrame(dt)` direto, sem passar por
`processTimeCriticalTasks()`. `make check-native-stack` e `make check-tc-agent` continuam
valendo.

### 14.5 Limite conhecido

Os agentes UBF são componentes da **Station**, e `ubf::Agent::updateData()` chama `controller(dt)`
sem consultar `isFrozen()` (`Agent.cpp:59-62`). Com a simulação pausada eles continuam
avaliando — só que sobre um mundo estático, reemitindo o mesmo comando para um autopilot
congelado. Nada se move; a decisão apenas não para. Vale para o `SimAgent` da poc/13 e para o
`FlightAgentTC` da poc/14 apenas no caminho de background.

### 14.6 Sem terminal

`ConsoleKeyboard` usa termios em modo raw (`~ICANON`/`~ECHO`, `VMIN=VTIME=0`) com `stdin` em
`O_NONBLOCK`, e restaura o terminal no destrutor. Sem TTY — pipe, redirecionamento, CI — o
`tcgetattr()` falha, `isActive()` fica `false` e a simulação roda normalmente, só sem teclado.
`app/RealTimeRun.cpp` diz isso na partida em vez de fingir que as teclas funcionam.
