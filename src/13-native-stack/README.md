# poc/13 — native-stack

Mesmo cenário da [poc/12](../12-jsbsim-ubf/) com a regra invertida: **herdar do
MIXR tudo o que o framework já tem pronto**. A poc/12 escreveu player,
dinâmica, controle, sensor, rádio e agente do zero; esta troca cada peça pela
equivalente nativa e mede o que se ganha e o que se paga.

O cenário é idêntico nas duas: 4 aviões patrulhando em quadrantes distintos,
1 intruso cruzando a área, quem detecta **avisa os outros**, e quem recebe o
aviso muda de comportamento e vai apoiar.

```bash
make build
make run-native-stack        # Tacview Real-Time Telemetry na porta 1234
make check-native-stack      # verifica o determinismo (1, 2 e 4 threads T/C)
```

---

## 1. Índice

1. [O que foi trocado](#2-o-que-foi-trocado)
2. [Como o framework chama o nosso código](#3-como-o-framework-chama-o-nosso-código)
3. [Anatomia de um frame](#4-anatomia-de-um-frame)
4. [A árvore de objetos do cenário](#5-a-árvore-de-objetos-do-cenário)
5. [Cada peça, uma a uma](#6-cada-peça-uma-a-uma)
6. [A cadeia de decisão: UBF + BehaviorTree](#7-a-cadeia-de-decisão-ubf--behaviortree)
7. [Interação entre aviões](#8-interação-entre-aviões)
8. [Determinismo](#9-determinismo)
9. [Tacview](#10-tacview)
10. [Armadilhas encontradas rodando](#11-armadilhas-encontradas-rodando)
11. [Estrutura de arquivos](#12-estrutura-de-arquivos)
12. [Como verificar tudo](#13-como-verificar-tudo)

---

## 2. O que foi trocado

| peça | poc/12 (do zero) | poc/13 (nativo) |
|---|---|---|
| player | `xair::Airplane : models::Player` | **`( Aircraft )`** |
| dinâmica 6-DOF | `xair::JsbsimFlightModel : models::System` (fala com a `FGFDMExec`) | **`( JSBSimModel )`** |
| controle de voo | `xair::FlightDirector : models::System` (leis próprias) | **`( Autopilot )`** |
| sensor | `xair::ProximitySensor : models::System` (geométrico) | **`( Gimbal/Antenna + Tws + AirTrkMgr )`** |
| interação entre players | `xair::AlertRadio : models::System` (evento próprio) | `xnative::AlertDatalink : **models::Datalink**` |
| agente do UBF | `xair::FlightAgent : ubf::AgentTC` | **`( SimAgent )`** |
| árbitro | `( UbfArbiter )` | `( UbfArbiter )` — já era nativo |
| percepção / decisão / atuação | `FlightState` / `BtBehavior` / `FlightAction` | **iguais** (o UBF não traz prontas) |

A factory própria caiu de **11 classes para 6** — e nenhuma das 6 é player,
dinâmica, controle ou sensor:

```cpp
// src/xnative/factory.cpp
AlertDatalink   TacticalAlert                    // datalink derivado + carga útil
FlightState     BtBehavior                       // percepção + decisão
AltitudeSafetyBehavior  FlightAction             // regra dura + atuação
```

O que **não** dá para herdar, e por quê: `models/` estende o UBF apenas com
`SimAgent` e `MultiActorAgent`. Não existem `AbstractState`, `AbstractBehavior`
ou `AbstractAction` concretos no framework — a política é, por definição, da
aplicação.

---

## 3. Como o framework chama o nosso código

Não existe registro nem callback. O MIXR dirige o nosso código porque **duas**
coisas são verdade ao mesmo tempo:

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

Por isso o `AlertDatalink` desta poc só precisa herdar `models::Datalink` (que
é um `System`) e sobrescrever `receive()` — quem o chama na fase certa é o
`System::updateTC()`, que já existe.

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

> **Detalhe do `dt`:** a lista de players é percorrida 4× por frame, cada vez com
> `dt/4`. Como cada método de fase roda em **uma** dessas passagens,
> `Player::updateTC()` e `System::updateTC()` recompõem `dt*4` no ponto do
> despacho. Um `dynamics()` recebe o dt do frame inteiro.

---

## 4. Anatomia de um frame

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

thread de background (laço do main.cpp, bgRate = 10 Hz)
└─ Station::updateData(dt)
   ├─ SimAgent::updateData() → Agent::controller()
   │     ├─ FlightState::updateState(ator)      PERCEPÇÃO
   │     ├─ UbfArbiter::genAction()             DECISÃO (por voto)
   │     │     ├─ AltitudeSafetyBehavior  (voto 90)
   │     │     └─ BtBehavior              (voto 50) → tick da árvore
   │     └─ FlightAction::execute(ator)         ATUAÇÃO → Autopilot + Datalink
   └─ DataRecorder::processRecords() → TacviewOutput → stream/arquivo ACMI
```

Repare onde a decisão foi parar: **na thread de background**. `SimAgent` deriva
de `ubf::Agent`, cujo ciclo roda em `updateData()`. Na poc/12 o agente era um
`AgentTC` nosso, filtrado para a fase 3 — precisamente porque o framework não
oferece um agente de tempo crítico pronto (`UbfAgentTC` existe como classe, mas
**nenhuma factory do MIXR o constrói**).

---

## 5. A árvore de objetos do cenário

`configs/scenario.epp.in` monta isto (o `@NUM_TC_THREADS@` é substituído pelo
`main.cpp` antes do parse, porque o teto depende da máquina):

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

- **`Player` não tem slot para subsistema nenhum.** Tudo entra por `components:`
  e é localizado **por tipo** (`Player::updateSystemPointers()`). Por isso a
  ordem no `.epp` é irrelevante, os nomes (`dynamicsModel:`, `pilot:`…) são
  livres, e um player sem um dado subsistema não é erro — é só um player que
  não tem aquilo (o caso do `bandit1`).
- **O agente mora na `Station`, não no player.** É assim que o `SimAgent`
  nativo funciona: ele amarra o ator pelo nome (`actorPlayerName`). Na poc/12 o
  agente era componente do próprio player.
- **`signature:` é o que torna o avião detectável.** O radar de um player lê a
  `RfSignature` do outro para resolver a equação do radar. Sem ela, o avião é
  invisível — não importa quantos radares existam.

---

## 6. Cada peça, uma a uma

### 6.1 `( Aircraft )` — o player

Classe nativa (`models::Aircraft : AirVehicle : Player`). Fornece de graça:
posição/atitude/velocidade nos três referenciais (NED, geodésico, ECEF), o ciclo
de 4 fases, a emissão de `REID_PLAYER_DATA` para o gravador, a descoberta de
subsistemas por tipo e os acessores de telemetria que delegam ao dynamics model
(`getMach()`, `getGload()`, `getFuelWt()`, `getAngleOfAttack()`…).

**O que se perde em relação ao player próprio da poc/12:** não há onde guardar
o que é nosso. O rótulo do comportamento (`PATROL`/`EVADE`/`SUPPORT`) morava num
campo do `xair::Airplane`; aqui foi para um quadro de status por id de player
(`include/xnative/runtime_utils.hpp`).

### 6.2 `( JSBSimModel )` — a dinâmica 6-DOF

Adaptador nativo do JSBSim. Roda na fase 0, chamado por `Player::dynamics()`.

Implementa a interface de comando do `DynamicsModel`
(`setCommandedHeadingD/Altitude/VelocityKts`) escrevendo nas propriedades
JSBSim **`ap/heading_setpoint`**, **`ap/altitude_setpoint`** e
**`ap/airspeed_setpoint`** (mais os respectivos `*_hold`) — confirmado com
`strings libmixr_models.so`.

> **Consequência que define a poc:** esses comandos só têm efeito se o **modelo
> JSBSim** da aeronave tiver um autopilot próprio implementando essas
> propriedades. O F4N das pocs 04/05/12 **não tem** (por isso lá foi preciso
> escrever leis de controle à mão); o **c310** tem
> (`<autopilot file="c310ap">`). Foi essa a razão técnica de trocar de
> aeronave — não estética.

### 6.3 `( Autopilot )` — o controle

Classe nativa (`models::Autopilot : Pilot`), no slot `pilot:`. Roda na fase 3 e
converte os alvos (rumo, altitude, velocidade) em chamadas ao dynamics model,
respeitando os limites declarados no EDL:

```
maxRateOfTurnDps / maxBankAngle / maxPitchAngle / maxClimbRateMps / maxAcceleration
```

Substitui inteiramente o `xair::FlightDirector` da poc/12 (que tinha malha de
rumo→banco→aileron, malha de altitude→arfagem→profundor com termo integral, e
os sinais de convenção do JSBSim como slots).

**Unidades:** `setCommandedAltitudeFt()` é em **pés**; o resto desta poc
trabalha em metros. A conversão fica em `FlightAction::execute()`, na fronteira.

### 6.4 O radar nativo — `Gimbal` + `Antenna` + `Tws` + `AirTrkMgr`

A mesma cadeia das pocs 06/07:

```
antennas: ( Gimbal { radar: ( Antenna ... ) } )   varredura mecânica + padrão de ganho
sensors:  ( SensorMgr { ( Tws antennaName: radar trackManagerName: twsTrkMgr ) } )
obc:      ( OnboardComputer { twsTrkMgr: ( AirTrkMgr ) } )
```

Zero linhas de detecção, RCS ou correlação de pista da nossa parte: fases 1 e 2
fazem emissão/recepção, e o `AirTrkMgr` (fase 3) mantém a lista de pistas.

O padrão de ganho está **embutido** no `.epp` (nas pocs 06/07 vinha de um
`#include`, que exige um passo de preprocessador C antes do `edl_parser`).

> **O radar nativo não separa amigo de inimigo.** `playerOfInterestTypes: { air }`
> filtra por **tipo** de player, não por lado: a esquadrilha inteira aparece na
> lista de pistas. O filtro amigo/inimigo é decisão tática e ficou onde deve
> estar — no `FlightState` (e no display do `main.cpp`, para os dois baterem).

### 6.5 `xnative::AlertDatalink : models::Datalink` — a interação

A única classe MIXR que ainda escrevemos, e mesmo assim herdando quase tudo. Da
classe base vêm: `sendMessage()` (varre os players, respeita alcance/rádio e
entrega por evento), a tabela de eventos que aceita `DATALINK_MESSAGE`, as filas
e a integração com `Player::getDatalink()`. Nossa parte é **o que fazer com a
mensagem** — ver a [seção 8](#8-interação-entre-aviões).

### 6.6 `( SimAgent )` + `( UbfArbiter )` — o agente e o árbitro

`SimAgent` (nativo) resolve o ator por nome e executa o ciclo
percepção→decisão→atuação. `UbfArbiter` (nativo) é ele próprio um
`AbstractBehavior` que consulta os comportamentos filhos e devolve a ação de
**maior voto**.

### 6.7 As peças que continuam nossas

| classe | papel no UBF | onde |
|---|---|---|
| `FlightState : ubf::AbstractState` | percepção → um `Snapshot` de números crus | `src/ubf/FlightState.cpp` |
| `BtBehavior : ubf::AbstractBehavior` | decisão, com a árvore BT.CPP como política interna | `src/ubf/BtBehavior.cpp` |
| `AltitudeSafetyBehavior : ubf::AbstractBehavior` | regra dura (piso de altitude), voto maior | `src/ubf/AltitudeSafetyBehavior.cpp` |
| `FlightAction : ubf::AbstractAction` | atuação: comanda `Autopilot` e `AlertDatalink` | `src/ubf/FlightAction.cpp` |
| `TacticalAlert : base::Object` | carga útil da mensagem de datalink | `src/xnative/TacticalAlert.cpp` |
| `domain/` | regras puras: patrulha, RTB, evasão, geometria | `src/domain/` |
| `bt/` | os 7 nós da árvore + factory | `src/bt/` |

O `domain/` é idêntico ao da poc/12 e **não conhece MIXR nem BehaviorTree.CPP**
— é testável sozinho.

---

## 7. A cadeia de decisão: UBF + BehaviorTree

O UBF define três papéis desacoplados, mas não diz *como* decidir. Aqui a
política de um dos comportamentos é uma árvore do BehaviorTree.CPP:

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

- **Nenhum nó toca em objeto MIXR.** Os nós leem o `Snapshot` e preenchem o
  `FlightDecision`; quem atua é a `FlightAction`. Os mesmos nós serviriam a
  outro atuador — ou a um teste unitário sem simulação.
- **Injeção de dependência pelo construtor**, via
  `factory.registerBuilder<T>(ID, builder)` — a forma documentada pelo autor da
  BT.CPP v3 para argumentos extras (a sobrecarga variádica de
  `registerNodeType` só existe em versões posteriores; aqui a lib é a 3.5.6). O
  blackboard **não** é usado como saco de ponteiros.
- **Port de verdade onde faz sentido:** `FuelLow` declara
  `InputPort<double>("margin", 0.0, ...)` e o XML passa `margin="0.05"`. A
  reserva do tanque é propriedade do **veículo** (slot EDL); a margem de decisão
  é propriedade da **árvore** (atributo XML).
- **Uma regra dura não precisa virar ramo da árvore.** O
  `AltitudeSafetyBehavior` é irmão do `BtBehavior` no árbitro nativo, com voto
  maior: quando o avião fura o piso, a ação dele vence sem que a árvore saiba
  que ele existe.

---

## 8. Interação entre aviões

É o mecanismo pedido: um evento de um player que muda o comportamento dos
outros. O caminho completo:

```
falcon3 detecta bandit1 (radar nativo)
   └─ árvore: ContactDetected → ReportAndEvade
        └─ FlightDecision.broadcastAlert = true (+ posição absoluta do contato)
             └─ FlightAction::execute()
                  └─ AlertDatalink::broadcastAlert()
                       └─ Datalink::sendMessage(TacticalAlert*)     [NATIVO]
                            └─ ... → player->event(DATALINK_MESSAGE, msg)
                                 └─ AlertDatalink::onDatalinkMessageEvent()   ← nosso gancho
                                      └─ ENCENA o alerta (fusão comutativa)
   ... fronteira de fase ...
   fase 2 do frame seguinte: AlertDatalink::receive() promove encenado → corrente
   fase 3/background:        FlightState vê hasAlert → árvore: AlertReceived → SupportAlert
```

Três decisões de modelagem que valem mais que o transporte:

1. **O handler roda na thread do EMISSOR.** O `sendMessage()` nativo chama
   `event()` direto no destino. Por isso o handler só *encena* o alerta, sob um
   mutex curto, e nunca mexe no estado corrente.
2. **A entrada não é fila FIFO.** Se dois aviões avisam no mesmo frame, a ordem
   de chegada depende do escalonador. A fusão é **comutativa** — vence o contato
   de menor distância; em empate exato, o emissor de menor id — então o
   resultado independe da ordem.
3. **A promoção acontece numa fronteira de fase.** O alerta encenado só passa a
   valer na fase 2 do frame seguinte, dando **latência fixa de um frame** para
   todos, em vez de "às vezes no mesmo frame, às vezes no próximo".

É a mesma disciplina que o framework usa entre as suas 4 fases: escreve numa
fase, publica na fronteira, lê na fase seguinte.

---

## 9. Determinismo

A simulação é de **passo fixo**: no Linux, `PeriodicThread::mainThreadFunc()`
calcula `dt = 1/rate` uma vez e passa esse mesmo valor todo frame (o flag de
delta variável é só do Windows e nasce desligado). O relógio de parede decide
apenas *quando* o frame roda.

Nada nesta poc usa relógio, sorteio ou identidade de thread para decidir. Onde
havia risco de dependência de ordem, ela foi eliminada explicitamente:

- fusão comutativa dos alertas (seção 8);
- escolha da pista: menor distância e, em empate exato, menor id de pista —
  para não depender da ordem da lista do `TrackManager`.

Para provar, o binário tem dois modos:

```bash
./build/poc/13-native-stack/src/native-stack -deterministic 2000   # passo fixo, sem thread periódica
./build/poc/13-native-stack/src/native-stack -threads 2            # força numTcThreads
```

`make check-native-stack` roda 2000 frames com **1, 2 e 4 threads** (mais uma
repetição de 4) e compara: os quatro dumps são **byte a byte idênticos**.

> **Ressalva honesta:** no modo de tempo real, a decisão roda na thread de
> background a 10 Hz enquanto a física roda a 50 Hz — os instantes de decisão
> não são fixos em relação aos frames. O determinismo verificado é o do modo de
> passo fixo, que é onde `tcFrame()` e `updateData()` andam em lockstep. Essa é
> uma consequência direta do `SimAgent` nativo; a poc/12, com `AgentTC`, decide
> dentro da fase 3 e não tem essa ressalva.

---

## 10. Tacview

A exportação é a de `shared/xtacview`, ligada na cadeia nativa do `dataRecorder`
(nenhum código de stream no `main.cpp`). Cada player precisa de
`dataLogTime: ( Seconds 0.1 )` — o slot nasce zero e, sem ele, o player nunca
aparece.

Semântica ACMI (é onde quase todo mundo erra):

| campo | conteúdo | slot |
|---|---|---|
| `Name` | **modelo** em notação ICAO/OTAN — é por ele que o Tacview acha a aeronave na base e escolhe o ícone/modelo 3D | `modelMap` |
| `Type` | taxonomia (`Air+FixedWing`) | `typeMap` |
| `CallSign` / `Pilot` | **nome do player** — é o que aparece no rótulo | automático |
| `Color` | lado | `colorMap` |

A poc declara `modelMap: { falcon1: "F-16C" ... }`, então o replay desenha
caças F-16:

```
65,T=...,Name=F-16C,Type=Air+FixedWing,Color=Blue,CallSign=falcon1,Pilot=falcon1
```

> **O ícone é apresentação, não física.** A dinâmica continua sendo a do c310 —
> a única aeronave com autopilot `ap/*` entre as distribuídas com o JSBSim. O
> `aircraft/f16` existe no JSBSim mas **não** tem autopilot, então trocar a
> dinâmica quebraria a cadeia nativa `Autopilot → JSBSimModel`. Trocar o ícone é
> uma linha no `.epp`; trocar a aeronave de verdade exigiria voltar ao controle
> próprio da poc/12.

---

## 11. Armadilhas encontradas rodando

Tudo abaixo foi medido com o binário, não deduzido.

### 11.1 `models::JSBSimModel` é `final`

Não dá para estender para corrigir nada. Ou se usa como está, ou se escreve
outro adaptador — que foi o caminho da poc/12.

### 11.2 O `JSBSimModel` nativo nunca liga os motores

Ele não escreve `propulsion/set-running` (nem magneto/mistura/partida). Numa
**turbina** isso passa despercebido — o `FGTurbine` parte com o vento relativo
em voo (medido com o F4N: 14 230 lb de empuxo). Num motor a **pistão** a
aeronave nasce planando:

```
thrust = 0 lb, velocidade 160 → 80 kts em 40 s,
altitude hold empinando o nariz até a perda de sustentação
```

Como a classe é `final`, a correção teve que ir para a **data da aeronave**:
`data/jsbsim/systems/engine-autostart.xml` (magnetos + partida +
`propulsion/engine[i]/set-running`).

> Cuidado documentado no próprio arquivo: escrever `propulsion/set-running`
> (o global) por um canal do FCS **reinicializa o motor a cada frame** — empuxo
> oscilando entre valores negativos e 900 lb.

### 11.3 O autopilot do c310 não fecha malha de velocidade

O `c310ap.xml` original apenas **declara** `ap/airspeed_hold` e
`ap/throttle-cmd-norm`, sem canal que os implemente. Como o MIXR já escreve
`ap/airspeed_setpoint`, bastou acrescentar o canal `AP Autothrottle` (PID →
manete) à cópia vendorizada — de novo em **dados**, não em C++.

### 11.4 O gravador nativo **segfalta** ao gravar pista nova

Assim que o radar cria a primeira pista:

```
AirTrkMgr::processTrackList()            (AirTrkMgr.cpp:351)
  → AbstractDataRecorder::recordData(REID_NEW_TRACK)
    → DataRecorder::recordNewTrack()     (DataRecorder.cpp:654)
      → __dynamic_cast                   → SEGV
```

Stack obtido com AddressSanitizer (`meson setup -Dasan=true build .`, que liga
o sanitizer **só** neste alvo). É a mesma família do `REID_WEAPON_RELEASED`
(poc/09) e o contorno é o mesmo: habilitar apenas os tokens que o
`TacviewOutput` usa, já que `isDataEnabled()` é testado **antes** de chamar o
handler quebrado:

```
enabledList: [ 43 42 ]      // 43 = REID_PLAYER_DATA, 42 = REID_PLAYER_REMOVED
```

### 11.5 O handler default de `DATALINK_MESSAGE` não enfileira nada

Duas tentativas erradas antes de achar o gancho certo:

| tentativa | resultado medido |
|---|---|
| drenar `receiveMessage()` na fase 2 | 0 alertas em 90 s, com 1113 transmissões |
| sobrescrever `queueIncomingMessage()` | 2016 chamadas de `onDatalinkMessageEvent` contra **0** de `queueIncomingMessage` |

O cabeçalho do `Datalink` já avisava: o handler *"passa as mensagens aos
subcomponentes"*. A `inQueue` é do caminho de **rádio/rede**, não da entrega
local. O gancho correto é **`onDatalinkMessageEvent()`**.

### 11.6 `UbfAgentTC` não é registrado por nenhuma factory do MIXR

`base/factory.cpp` registra apenas `UbfAgent` e `UbfArbiter`. Um agente de
tempo crítico só existe em EDL se a aplicação registrar o seu — foi o que a
poc/12 fez. Esta poc usa o `SimAgent` (background) exatamente para mostrar a
diferença.

### 11.7 O `Agent` não propaga o ciclo de componentes

Nem `updateData()` nem `reset()` chegam ao `state`/`behavior` (e um
comportamento dentro do `UbfArbiter` está dois níveis abaixo). Sintoma real na
poc/12: os planos de voo ficavam com os **defaults** do `domain::PatrolPlan` em
vez dos valores dos slots, sem erro nenhum. Por isso o `BtBehavior` configura
os planos **preguiçosamente**, no primeiro `genAction()`.

---

## 12. Estrutura de arquivos

```
poc/13-native-stack/
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
│   └── recordings/                mission.acmi (gitignored)
├── include/ e src/
│   ├── domain/                    regras puras (sem MIXR, sem BT, sem JSBSim)
│   │   ├── FlightCommand.hpp      DTO de comando (rumo/altitude/velocidade)
│   │   ├── geometry.*             wrap180/360, rumo e distância em NED
│   │   ├── PatrolPlan.*           circuito de patrulha + plano de retorno
│   │   └── ThreatPolicy.*         manobra de evasão
│   ├── xnative/                   o que sobrou de próprio no lado MIXR
│   │   ├── AlertDatalink.*        herda models::Datalink
│   │   ├── TacticalAlert.*        base::Object (carga do datalink)
│   │   ├── factory.*              registra as 6 classes próprias
│   │   └── runtime_utils.*        tag de thread, log com mutex, quadro de status
│   ├── ubf/                       percepção / decisão / atuação
│   │   ├── FlightState.*          AbstractState  → Snapshot
│   │   ├── BtBehavior.*           AbstractBehavior (hospeda a árvore)
│   │   ├── AltitudeSafetyBehavior.*  AbstractBehavior (regra dura)
│   │   └── FlightAction.*         AbstractAction (Autopilot + Datalink)
│   ├── bt/                        adaptadores da árvore
│   │   ├── NodeContext.hpp        FlightDecision + dependência dos nós
│   │   ├── bt_factory.*           registerBuilder<T> de cada nó
│   │   └── nodes/                 FuelLow, ReturnToBase, ContactDetected,
│   │                              ReportAndEvade, AlertReceived, SupportAlert, Patrol
│   ├── mixr_factory.*             encadeia xnative → xtacview → framework
│   └── main.cpp                   gera o .epp, constrói a Station, roda o laço/status
```

O `main.cpp` é fino de propósito: ele **não** pilota, **não** tica árvore e
**não** monta ACMI. Ele gera o `.epp` (só o número de threads), constrói a
`Station`, chama `updateData()` no laço — que drena o gravador e roda os
`SimAgent`s — e imprime o status.

---

## 13. Como verificar tudo

```bash
# build + execução (Ctrl+C encerra)
make build && make run-native-stack

# determinismo: 1, 2 e 4 threads produzem o mesmo estado
make check-native-stack

# o que o replay recebeu
grep -o "Name=[^,]*,Type=[^,]*,Color=[^,]*,CallSign=[^,]*" \
     poc/13-native-stack/data/recordings/mission.acmi | sort -u

# depuração com AddressSanitizer só neste alvo
meson setup --reconfigure -Dasan=true build . && meson compile -C build native-stack
```

O status impresso a cada 2 s traz, por aeronave: altitude, rumo, banco,
velocidade, **empuxo**, mach, G, combustível, o rótulo do comportamento
vencedor, o que o `Autopilot` está comandando, a pista mais próxima (já
filtrada por lado) e o alerta recebido. Numa sessão típica:

```
falcon3  ... bt=EVADE    ap(hdg=244,alt=13281ft,spd=190) pista=bandit1@6.2NM
falcon1  ... bt=SUPPORT  ap(hdg=49,alt=10537ft,spd=180)  alerta<-falcon3(bandit1)
falcon2  ... bt=SUPPORT  ap(hdg=3,alt=10537ft,spd=180)   alerta<-falcon3(bandit1)
falcon4  ... bt=SUPPORT  ap(hdg=68,alt=10537ft,spd=180)  alerta<-falcon3(bandit1)
bandit1  ... bt=(sem agente)
```

---

## 14. Resumindo o que a poc responde

- **O framework dirige pela herança e encontra pela árvore de componentes**: o
  `.epp` diz *onde* o objeto está, a classe-base diz *quando* ele é chamado, e o
  `virtual` diz *o que* roda.
- **Herdar tudo o que dá reduz muito o código próprio** (11 → 6 classes, sem
  player/dinâmica/controle/sensor) e traz junto coisas que ninguém escreve por
  gosto: equação do radar, correlação de pistas, transporte de datalink, limites
  de autopilot.
- **O preço aparece nas bordas**: a aeronave teve que mudar (o autopilot nativo
  depende de propriedades que só alguns modelos JSBSim têm), a classe da
  dinâmica é `final`, os motores não ligam sozinhos, o radar não filtra lado, a
  decisão saiu do tempo crítico e sumiu o lugar natural para guardar o que é
  nosso.
- **O que é decisão continua sendo seu**: percepção, política, atuação e as
  regras de fusão/determinismo. O UBF define os papéis; ele não os preenche.
