# flight

Quatro caças A-4 (Douglas A-4 Skyhawk) patrulham quadrantes distintos sobre a Serra do Mar; um
intruso (`bandit1`) cruza a área — chegando **só pela rede**, ver [`../bandit/`](../bandit/) e
[`../README.md`](../README.md) para o grupo. Quem detecta avisa os outros pelo datalink, e quem
recebe o aviso muda de comportamento e vai apoiar. Sobre terreno real, com dinâmica 6-DOF real,
radar real, e a decisão saindo **direto** de uma árvore de comportamento — sem árbitro por voto no
meio (`behavior:` do agente aponta direto para `( BtBehavior )`; ver a nota "SEM ARBITRO" no topo
de [`configs/scenario.edl.in`](configs/scenario.edl.in) e a [§6.6](#66--flightagenttc---o-agente)
para onde o árbitro nativo continua em uso de verdade neste repositório).

A regra de projeto é uma só: **herdar do MIXR tudo o que o framework já tem pronto**. Player,
dinâmica 6-DOF, controle de voo, radar e banco de elevação são nativos. O que é nosso é o que o
framework, por definição, não fornece: a **política** — percepção, decisão, atuação —, o **agente**
que a roda dentro do frame de tempo crítico, e a carga útil da mensagem trocada entre os aviões.

> **O agente do UBF é um `( FlightAgentTC )` PRÓPRIO, componente do PLAYER, decidindo na FASE 3 do
> frame de tempo crítico** — nunca o `( SimAgent )` nativo, que rodaria em `updateData()` (thread de
> background). É o **único** agente que este modelo tem hoje: não existe mais um caminho alternativo
> por `( SimAgent )` numa `Station`. Até uma passada anterior deste repositório havia dois cenários
> gêmeos que trocavam só essa peça — um decidindo assim, dentro do frame, um caça por thread do
> pool; outro com um agente nativo decidindo os quatro em sequência, numa thread de background —,
> unificados neste, que preserva a árvore, a pilha e os números calibrados dos dois. Os quatro
> falcons decidem **em paralelo**, um por thread do pool de tempo crítico, a **50 Hz** (`tcRate`,
> default nativo do `simulation::Station` — não sobrescrito por este cenário).

> **ONDE MORA O QUÊ.** Esta pasta é só **dado**: [`configs/`](configs/) (o cenário) e `data/`
> (gravações/logs/mensagens de runtime, gitignored). Não há executável próprio — quem executa é o
> **`./app`**, o runner único de todas as pocs deste repositório. A **política** —
> `domain/`, `bt/`, `ubf/`, `xnative/` — não está aqui: mora em
> [`models/players/A-4/`](../../../../models/players/A-4/), um projeto Meson **independente**,
> construído numa etapa **anterior** (`make models`) e carregado por `dlopen` durante o parse do
> cenário (o bloco `( PluginLoader )`, primeira entrada de `components:` — ver [§4](#4-a-árvore-de-objetos-do-cenário)).
> Isso não é arrumação: é o que torna verificável o cenário de um terceiro entregar só o binário.
> Ver [CONTRIBUTING.md](../../../../CONTRIBUTING.md) para escrever um modelo novo, e
> [libs/xplugin/README.md](../../../../libs/xplugin/README.md) para o contrato.

```bash
make build
./build/app/src/app -f src/poc/dis/flight/configs/scenario.edl.in
# equivalente (configs/ tem um único .edl.in): ./build/app/src/app -folder src/poc/dis -scenario flight
# Tacview Real-Time Telemetry na porta 1234; Ctrl+C encerra

./tests/determinism/check_determinism.sh ./build/app/src/app flight 2000 flight
# verifica o determinismo (1, 2 e 4 threads T/C) sobre uma fixture hermética derivada deste cenário
```

> **Rode sempre a partir da raiz do repositório**: o cenário, os dados do JSBSim, o tile SRTM e a
> gravação `.acmi` são resolvidos por caminho relativo (`./src/poc/dis/flight/...`,
> `./shared/data/...`, `./dist/share/mixr-plugins/flight/...`).

---

## Índice

1. [O que vem do framework e o que é nosso](#1-o-que-vem-do-framework-e-o-que-é-nosso)
2. [Anatomia de um frame](#2-anatomia-de-um-frame)
3. [Como o framework chama o nosso código](#3-como-o-framework-chama-o-nosso-código)
4. [A árvore de objetos do cenário](#4-a-árvore-de-objetos-do-cenário)
5. [Padrões dos exemplos oficiais usados aqui](#5-padrões-dos-exemplos-oficiais-usados-aqui)
6. [Cada peça nativa, uma a uma](#6-cada-peça-nativa-uma-a-uma)
7. [Dissecação: o repositório em ordem de dependência](#7-dissecação-o-repositório-em-ordem-de-dependência)
8. [A cadeia de decisão: UBF + BehaviorTree](#8-a-cadeia-de-decisão-ubf--behaviortree)
9. [Interação entre players](#9-interação-entre-players)
10. [Elevação de terreno](#10-elevação-de-terreno)
11. [Tacview](#11-tacview)
12. [Determinismo](#12-determinismo)
13. [Armadilhas encontradas rodando](#13-armadilhas-encontradas-rodando)
14. [Controle de tempo — acelerar, frear, pausar](#14-controle-de-tempo--acelerar-frear-pausar)
15. [O que foi medido rodando](#15-o-que-foi-medido-rodando)
16. [Como verificar tudo](#16-como-verificar-tudo)
17. [O que a poc responde](#17-o-que-a-poc-responde)

> **Sugestão de leitura.** Se você quer *entender o MIXR*, leia 1 → 2 → 3 → 4. Se você quer
> *dissecar este repositório*, pule para a [seção 7](#7-dissecação-o-repositório-em-ordem-de-dependência),
> que percorre todos os arquivos do modelo na ordem em que as dependências aparecem. A [seção 6.6](#66--flightagenttc---o-agente)
> é o mergulho mais próprio desta poc — o agente inteiro, linha por linha. As seções 8 a 12 são
> mergulhos temáticos; a 13 é o catálogo de cicatrizes.

---

## 1. O que vem do framework e o que é nosso

| peça | classe nativa |
|---|---|
| player | `( Aircraft )` |
| dinâmica 6-DOF | `( JSBSimModel )` |
| controle de voo | `( Autopilot )` |
| sensor | `( Gimbal/Antenna )` + `( Tws )` + `( AirTrkMgr )` |
| gravação/exportação | `( ExposedDataRecorder )` (`libs/xtacview` — `DataRecorder` nativo com `getOutputHandler()` público) |
| terreno | `( SrtmHgtFile )` + `Player::updateElevation()` |

> **SEM ÁRBITRO.** `behavior:` do `( FlightAgentTC )` aponta **direto** para o `( BtBehavior )` — não
> há `( UbfArbiter )`/`( AltitudeSafetyBehavior )` no meio (comentário "SEM ARBITRO" no topo de
> [`configs/scenario.edl.in`](configs/scenario.edl.in)). `AltitudeSafetyBehavior` e `RLBridgeBehavior`
> continuam compiladas e exportadas pelo plugin — são duas das nove classes próprias, ver a tabela
> abaixo —, só não são instanciadas por este cenário. Sem o árbitro, o piso anti-CFIT que sobra é
> só o `terrainClearance:` do próprio `BtBehavior`, que age apenas **dentro do ramo de evasão** —
> não há um piso independente cobrindo `PATROL`/`RTB`/`SUPPORT`. O mecanismo em si —
> `base::ubf::UbfArbiter`, composto **nativo** do MIXR — segue em uso de verdade em
> [`src/rl`](../../../../src/rl/), onde protege uma política de RL ruim contra o chão. Detalhes em
> [§6.6](#66--flightagenttc---o-agente) e [§10.2](#102-o-que-a-elevação-faz-com-o-comportamento).

**Nove classes próprias**, registradas em
[`models/players/A-4/src/xnative/factory.cpp`](../../../../models/players/A-4/src/xnative/factory.cpp)
e publicadas por `libflight.so`. Nenhuma delas é player, dinâmica, controle ou sensor:

| classe nossa | herda de | por que não dá para herdar pronta |
|---|---|---|
| `xnative::FlightAgentTC` | `base::ubf::AgentTC` | é o **único** agente que este modelo usa — decide na fase 3 do frame de tempo crítico, componente do player. `base::factory.cpp` só registra `"UbfAgent"`/`"UbfArbiter"`; um agente de tempo crítico é, na prática, código da aplicação (ver [§6.6](#66--flightagenttc---o-agente)) |
| `xnative::FlightState` | `base::ubf::AbstractState` | o UBF define a **interface** de percepção; `models/` não traz implementação pronta |
| `xnative::BtBehavior` | `base::ubf::AbstractBehavior` | idem, para decisão |
| `xnative::AltitudeSafetyBehavior` | `base::ubf::AbstractBehavior` | idem — exportada, mas não instanciada por este cenário (ver a nota acima) |
| `ubf::RLBridgeBehavior` | `base::ubf::AbstractBehavior` | idem — a ponte com `src/rl`; também não instanciada por este cenário |
| `xnative::FlightAction` | `base::ubf::AbstractAction` | idem, para atuação |
| `xnative::AlertDatalink` | `models::Datalink` | o framework transporta; **o que fazer com a mensagem** é da aplicação |
| `events::TacticalAlert` | `base::Object` | a carga útil do alerta — mora em `models/events/`, não em `xnative/`, porque um payload cruzando `dlopen()` só é seguro numa `shared_library()` linkada por **todos** os lados envolvidos (ver [`models/events/README.md`](../../../../models/events/README.md)) |
| `xnative::ThreadTagProbe` | `base::Component` | publica em qual thread do pool T/C um player **sem** agente próprio (o `bandit1` local de outros cenários, um míssil de outro modelo) está sendo processado |

O que se ganha ao herdar não é só linha de código economizada — é modelo que ninguém escreve por
gosto: equação do radar com RCS e perdas, correlação de pistas com filtro alfa-beta, transporte de
datalink com fila de rede, limites de autopilot, integração 6-DOF, consulta a banco de elevação. E o
que se **paga** aparece nas bordas, catalogado na [seção 13](#13-armadilhas-encontradas-rodando).

---

## 2. Anatomia de um frame

Este diagrama é a chave de leitura de tudo o que vem depois. **Onde uma coisa roda determina o que
ela pode ler e o que ela pode escrever.**

```
thread de tempo crítico (PeriodicThread, dt = 1/tcRate FIXO — 50 Hz)
└─ Station::tcFrame(dt) → Simulation::updateTC(dt)
   │      (a lista de players é fatiada entre numTcThreads, com BARREIRA por fase)
   │
   ├─ FASE 0  "dinâmica"
   │    Player::dynamics()  →  JSBSimModel::dynamics()   ← passo do 6-DOF
   │                            ├─ lê  ap/heading_setpoint, ap/altitude_setpoint,
   │                            │      ap/airspeed_setpoint  (escritos pelo Autopilot)
   │                            └─ FDM Run() → superfícies → forças → estado do Player
   │
   ├─ FASE 1  "sensores transmitem"
   │    Antenna/Tws  → emissões de RF para os players de interesse
   │                 → targets[i]->event(RF_EMISSION, em)   ← a interação entre players
   │
   ├─ FASE 2  "sensores recebem"
   │    Radar::receive()          → detecções (limiar S/I)
   │    AlertDatalink::receive()  → PROMOVE o alerta encenado (nosso, ver §9)
   │
   └─ FASE 3  "lógica e controle"
        AirTrkMgr::process()          → cria/atualiza as pistas (alfa-beta)
        FlightAgentTC::controller()   ←←← A DECISÃO ESTÁ AQUI
           ├─ FlightState::updateState(ator)      PERCEPÇÃO
           ├─ BtBehavior::genAction()              DECISÃO — direto, sem árbitro no meio
           │     └─ tick da árvore
           └─ FlightAction::execute(ator)          ATUAÇÃO → Autopilot + AlertDatalink
        Autopilot::process()          → erro de rumo/altitude/velocidade → setCommanded*()
                                          consome o comando que acabou de ser escrito

thread de background (laço de tempo real do ./app, ~10 Hz — ver §14)
└─ Station::updateData(dt)
   ├─ Simulation::updateData() → updateBgPlayerList()
   │     └─ Player::updateData() → updateElevation()   ← consulta ao banco de terreno
   ├─ MsgFeed::updateData()                            ← libs/xmsg, amostra e emite mensagens
   └─ ExposedDataRecorder::processRecords() → TacviewOutput → stream/arquivo ACMI
```

Três leituras que valem guardar:

1. **A decisão mora DENTRO da fase 3, não no laço de background.** `FlightAgentTC` deriva de
   `base::ubf::AgentTC`, cujo `updateTC()` é chamado pelo ciclo de componentes do próprio player,
   dentro do `tcFrame()`. Os quatro agentes rodam em paralelo, um por thread do pool T/C — não em
   sequência, numa thread só. É essa a diferença estrutural que faz o determinismo em tempo real
   ser propriedade do **modelo**, e não do laço que o chama (ver [§12](#12-determinismo)).
2. **A elevação do terreno continua sendo de background.** `Player::updateElevation()` é chamado de
   `Player::updateData()`, não de nenhuma das quatro fases — mesmo com a decisão dentro do frame,
   a leitura do banco de elevação não migrou. Consequência medida na [seção 10](#10-elevação-de-terreno).
3. **A ordem DENTRO da fase 3 é ordem de declaração no `.edl`.** `agent:` é o **último** componente
   de cada `falconN`, depois de `obc:` — assim `AirTrkMgr::process()` (que cria as pistas) roda
   **antes** de `FlightAgentTC::controller()` (que as lê), no **mesmo** frame; e o `Autopilot`
   consome o comando que o agente acabou de escrever, também no mesmo frame. Inverter a ordem no
   EDL custaria um frame de latência, sem uma linha de C++ mudar.

> **Detalhe do `dt`:** a lista de players é percorrida 4× por frame, cada vez com `dt/4`. Como cada
> método de fase roda em **uma** dessas passagens, `Player::updateTC()`/`System::updateTC()`/
> `AgentTC::updateTC()` recompõem `dt*4` no ponto do despacho — um `dynamics()` ou um
> `controller()` recebem o `dt` do frame inteiro, não um quarto dele.

---

## 3. Como o framework chama o nosso código

Não existe registro nem callback. O MIXR dirige o nosso código porque **duas** coisas são
verdade ao mesmo tempo:

1. **o objeto está na árvore de componentes** (foi o `.edl` que o colocou lá), e
2. **a classe herda de algo que o framework já sabe dirigir**.

O despacho é virtual, puro e simples:

```
Component::updateTC(dt)          percorre a lista de componentes
   └─ System::updateTC(dt)       consulta sim->phase() e despacha:
        fase 0 → dynamics(dt*4)
        fase 1 → transmit(dt*4)
        fase 2 → receive(dt*4)
        fase 3 → process(dt*4)
   └─ AgentTC::updateTC(dt)      chama controller(dt) em TODA fase -- FlightAgentTC
                                 filtra por conta própria (ver §6.6, armadilha 2)
```

Por isso o `AlertDatalink` só precisa herdar `models::Datalink` (que é um `System`) e sobrescrever
`receive()` — quem o chama na fase certa é o `System::updateTC()`, que já existe.

Cada peça é "pega" por um mecanismo diferente:

| classe | como o framework chega nela |
|---|---|
| `Aircraft` (`falcon1`…`falcon4`) | estão na lista `players:` do `WorldModel`, percorrida pela `Simulation` |
| `JSBSimModel` | achado **por tipo** em `Player::updateSystemPointers()` (`findByType(typeid(DynamicsModel))`) e chamado por `Player::dynamics()` na fase 0 |
| `Autopilot` | idem, como `Pilot` (`getPilotByType`), rodando na fase 3 |
| `Antenna`/`Tws`/`AirTrkMgr` | são `System`s dentro do player: fases 1 (transmite), 2 (recebe) e 3 (processa pistas) |
| `AlertDatalink` | `System` na lista de componentes → fase 2; e `event(DATALINK_MESSAGE)`/`event(EID_ALERT)` pela tabela `BEGIN_EVENT_HANDLER` da classe base |
| `SrtmHgtFile` | objeto do slot `terrain:` do `WorldModel`; carregado por `Terrain::reset()` no `RESET_EVENT` e consultado por `Player::updateElevation()` |
| `FlightAgentTC` | componente do **player** (não da `Station`); `initActor()` sobe a cadeia de containers para achar o próprio player — sem nome nenhum a amarrar (ver [§6.6](#66--flightagenttc---o-agente)) |
| `FlightState`/`BtBehavior`/`FlightAction` | **não** são chamados pelo ciclo de componentes: quem os chama é `Agent::controller()`, disparado pelo `FlightAgentTC` |
| `ExposedDataRecorder`/`TacviewOutput` | elo da cadeia de `OutputHandler`s do gravador, drenada por `Station::updateData()` |

**A consequência que quase todo mundo descobre tarde:** um nome errado em qualquer slot `*Name:`
produz um componente **inerte, sem diagnóstico**. Vale para `antennaName`, `trackManagerName`. O
objeto é construído, entra na árvore, é chamado em todas as fases — e não faz nada, porque o
ponteiro que ele procurava é nulo. `FlightAgentTC` não sofre dessa classe de erro: não amarra o
ator por nome, resolve pelo próprio container.

---

## 4. A árvore de objetos do cenário

[`configs/scenario.edl.in`](configs/scenario.edl.in) monta isto (o `@NUM_TC_THREADS@` é substituído
antes do parse, porque o teto depende da máquina — ver [§14](#14-controle-de-tempo--acelerar-frear-pausar)):

```
( ClockStation                                          ← libs/xclock: Station + controle de tempo
   tcPriority: 0.5
   ownship: falcon1

   components: {
      plugins: ( PluginLoader                           ← CARREGA O MODELO, dlopen, ANTES de tudo
         modules: { ( PluginModule file: "libflight.so"
                       provides: { AlertDatalink TacticalAlert ThreadTagProbe FlightAgentTC
                                   FlightState BtBehavior AltitudeSafetyBehavior
                                   RLBridgeBehavior FlightAction } ) } )
      msgFeed: ( MsgFeed ... )                           ← libs/xmsg, roda em updateData(), não decide
   }
                                                          ← NENHUM AGENTE aqui: cada agente mora
                                                            DENTRO do player que ele pilota

   networks: { ( DisNetIO                                ← recebe bandit1 e EMITE falcon1..4
      netInput:  ( UdpBroadcastHandler port: 3000 ignoreSourcePort: 3002 ... )
      netOutput: ( UdpBroadcastHandler port: 3000 localPort: 3002 ... )
      inputEntityTypes:  { ( DisNtm template: ( Aircraft type: "A4" ... ) disEntityType: [...] ) }
      outputEntityTypes: { ( DisNtm template: ( Aircraft type: "A4" )     disEntityType: [...] ) }
   ) }

   dataRecorder: ( ExposedDataRecorder eventName: "flight" enabledList: [ 43 42 ]
      outputHandler: ( RecorderOutputHandler
         components: { ( TacviewOutput port: 1234 callsign: "poc-mixr/flight"
                          fileName: "./src/poc/dis/flight/data/recordings/mission.acmi"
                          modelMap/typeMap/colorMap: { ... falcon1..4 blue, bandit1 red ... } ) } ) )

   simulation: ( WorldModel
        numTcThreads: @NUM_TC_THREADS@
        latitude/longitude: -22.25 / -42.48              ← Serra do Mar (RJ), miolo do tile
        terrain: ( SrtmHgtFile path/file: S23W043.hgt )
        players: {
           falcon1..falcon4 : ( Aircraft  id: 10N  side: blue  type: "A4"
              signature: ( SigSphere radius: 3.0 )        ← RCS: como os OUTROS radares o veem
              dataLogTime: ( Seconds 0.1 )                ← sem isto, não aparece no Tacview
              interpolateTerrain: true                    ← bilinear entre os posts de 30 m
              components: {
                 dynamicsModel: ( JSBSimModel  rootDir/model: A4 )
                 pilot:         ( Autopilot    hold modes + limites )
                 datalink:      ( AlertDatalink holdTime )
                 antennas:      ( Gimbal components: { radar: ( Antenna gainPattern ... ) } )
                 sensors:       ( SensorMgr components: { ( Tws antennaName: radar
                                                              trackManagerName: twsTrkMgr ) } )
                 obc:           ( OnboardComputer components: { twsTrkMgr: ( AirTrkMgr ) } )
                 agent:         ( FlightAgentTC state: ( FlightState )
                                    behavior: ( BtBehavior treeFile: ... 18 parâmetros ... ) )
              } )
           // bandit1 NÃO mora aqui -- processo próprio src/poc/dis/bandit, recebido só
           // pela rede via 'networks:' acima. Ver ../README.md.
        } ) )
```

Seis coisas que valem entender nessa árvore:

- **`components:` da `Station` não tem mais agente nenhum.** Antes de a decisão migrar para dentro
  do player, era ali que os quatro `( SimAgent )` moravam. Hoje `components:` existe só para o
  `( PluginLoader )` (que precisa vir **primeiro** — a `arglist` do `edl_parser` é recursiva à
  esquerda, então as formas são construídas na ordem do texto, e o `( PluginLoader )` tem que
  carregar antes do primeiro `( FlightState )`/`( BtBehavior )` usado mais abaixo) e o
  `( MsgFeed )` (mensageria, fora do frame de tempo crítico, não é decisão).
- **`Player` não tem slot para subsistema nenhum.** Tudo entra por `components:` e é localizado
  **por tipo** (`Player::updateSystemPointers()`). Por isso a ordem no `.edl` é irrelevante *para
  encontrar*, os rótulos (`dynamicsModel:`, `pilot:`…) são livres, e um player sem um dado
  subsistema não é erro — é só um player que não tem aquilo.
- **A ordem É ordem de EXECUÇÃO dentro da fase.** `Component::updateTC()` percorre os
  subcomponentes na ordem declarada. `sensors:` vem antes de `obc:`, então `Radar::process()`
  (que faz `newReport`) roda antes de `AirTrkMgr::process()` (que drena) — a pista aparece no
  **mesmo** frame. E `agent:` é o **último**, depois de `obc:` — ver [§2](#2-anatomia-de-um-frame).
- **O agente mora DENTRO do player, não na `Station`.** É o oposto do `( SimAgent )` nativo, que
  amarraria o ator por `actorPlayerName`. Aqui o bloco `agent: ( FlightAgentTC ... )` é **idêntico**
  nas quatro aeronaves — nenhum nome a amarrar, porque `initActor()` sobe a cadeia de containers.
- **`signature:` é o que torna o avião detectável.** O radar de um player lê a `RfSignature` do
  outro para resolver a equação do radar. Sem ela, o avião é invisível — não importa quantos
  radares existam.
- **`terrain:` é slot do `WorldModel`, e a factory dele não vem de graça.** `mixr::models::factory`
  **não** encadeia a de terreno; sem `mixr::terrain::factory` na cadeia do host (ver
  [§5.2](#52-factory-encadeada-por-nome--hoje-duas-cadeias-separadas)), o `( SrtmHgtFile )` não constrói nada e o mundo fica sem
  chão, em silêncio.

**As altitudes não são arbitrárias.** Cada falcon voa no **pico do próprio circuito de patrulha
+ 300 m**, arredondado para cima:

| player | pico do circuito (comentado no `.edl`) | `initAlt` | `rtbAltitude` |
|---|---|---|---|
| falcon1 | 1435 m | 1750 m | 2050 m |
| falcon2 | 1529 m | 1850 m | 2100 m |
| falcon3 | 1738 m | 2050 m | 2150 m |
| falcon4 | 1709 m | 2100 m | 2200 m |

A folga existe porque, com terreno carregado, o `CRASH_EVENT` nativo passa a valer: abaixo do solo
o player vai para `CRASHED` e **congela** (ver [§10.4](#104-armadilhas-confirmadas-no-fonte)). O
intruso (`bandit1`, em `../bandit/`) voa a 2400 m — acima de todos —, e é isso que faz o
desconflito vertical da evasão ser para **baixo**, o único caso em que o terreno tem algo a dizer.
Ver a [seção 10](#10-elevação-de-terreno).

---

## 5. Padrões dos exemplos oficiais usados aqui

Os exemplos que acompanham o MIXR são a única documentação de *como se escreve uma aplicação* com
ele. A destilação está em `contexts/MIXR-PATTERN-CONTEXT.md`; abaixo, os padrões que este cenário
usa e onde eles aparecem — hoje repartidos entre **duas** factories, porque o modelo virou plugin.

### 5.1 O *builder* canônico

Trinta e oito dos `main.cpp` oficiais fazem a mesma coisa: chamam `base::edl_parser(arquivo,
minhaFactory, &erros)`, desembrulham o `base::Pair` de topo, fazem `dynamic_cast` para o tipo
raiz esperado, e abortam se qualquer passo falhar. No `./app` (o runner único de todas as pocs)
isso vive em `app/StationBuilder.cpp`, isolado do `main.cpp`:

```cpp
obj = pair->object();  obj->ref();  pair->unref();   // o desembrulho canônico
```

O `ref()` antes do `unref()` não é paranoia: o `Pair` é dono do objeto, e destruí-lo sem
incrementar a contagem levaria a `Station` junto.

### 5.2 Factory encadeada por nome — hoje, duas cadeias separadas

Todo exemplo tem uma função `factory(const std::string&)` que tenta as suas classes e cai para as
do framework. **A primeira que retorna não-nulo vence.** Antes de o modelo virar plugin, as classes
próprias entravam nessa mesma lista; hoje há duas cadeias, resolvidas em momentos diferentes:

- **A do HOST** (`app/src/mixr_factory.cpp`, compartilhada por **todas** as pocs deste
  repositório):
  ```
  xplugin → xtacview → xclock → xjoystick → xmsg → simulation → models → terrain → dis → linkage → recorder → base
  ```
  `terrain`/`dis`/`linkage` **têm** de estar presentes — `models::factory` não os encadeia; sem a
  linha certa, `( SrtmHgtFile )`/`( DisNetIO )` não constroem nada, em silêncio. `xclock` vem antes
  de `simulation` porque `ClockStation` tem nome de fábrica próprio (`"ClockStation"`), não
  `"Station"`.
- **A do MODELO** (`models/players/A-4/src/xnative/factory.cpp`), invocada não pela cadeia acima,
  mas pelo mecanismo de `libs/xplugin` no `isValid()` do `( PluginLoader )` — **antes** de o resto
  do `.edl` ser interpretado (ver [§4](#4-a-árvore-de-objetos-do-cenário)). Depois desse ponto, os
  nove nomes de `provides:` valem no resto do arquivo como se fossem do framework.

### 5.3 Estrutura em EDL, comportamento em C++

Nenhum exemplo oficial constrói a hierarquia de simulação em código. Quais players existem, com
quais subsistemas, em que taxa, com quantas threads — é tudo `.edl`. Aqui isso vai ao extremo: o
número de threads T/C é um marcador `@NUM_TC_THREADS@` resolvido **antes do parse**, porque
`setSlotNumTcThreads()` é privado e não há setter público — a única forma honesta de manter tudo
instanciado via EDL.

### 5.4 O gancho de sensor é `transmit()`, e a biblioteca `libs/x<nome>`

O único exemplo de sensor novo em toda a árvore oficial (`mainGndMapRdr/RealBeamRadar`) engata em
`transmit(dt)`, chamando `BaseClass::transmit(dt)` primeiro, e busca recursos do mundo
preguiçosamente. Este cenário não escreve sensor próprio, mas usa o **outro** padrão do mesmo
exemplo: o de empacotar o que é transversal como `libs/x<nome>` com factory própria —
[`libs/xtacview`](../../../../libs/xtacview/) (a exportação Tacview), `libs/xclock` (`ClockStation`)
e `libs/xmsg` (`MsgFeed`) são as três que este cenário de fato usa.

### 5.5 `dataRecorder` com cadeia de `OutputHandler` — e uma variante própria do gravador

O padrão de gravação oficial é `DataRecorder` → `RecorderOutputHandler` → `components: { ... }`.
Aqui a raiz é `( ExposedDataRecorder )`, não `( DataRecorder )` — a mesma classe nativa, só com
`getOutputHandler()` tornado público, o que deixa `app/StationBuilder.cpp` achar o
`TacviewOutput` **de fora**, para alimentar a varredura de radar exibida no Tacview. Ninguém abre
socket no `main.cpp`. É exatamente onde o `TacviewOutput` se pendura — ver a
[seção 11](#11-tacview).

---

## 6. Cada peça nativa, uma a uma

### 6.1 `( Aircraft )` — o player

`models::Aircraft : AirVehicle : Player`. Traz posição/atitude/velocidade em três sistemas de
coordenadas, integração de estado, *ground clamping*, detecção de colisão com o solo, ciclo de
4 fases, tabela de eventos e a resolução por tipo dos subsistemas.

Não há slot para subsistema: tudo entra por `components:`. `AirVehicle` acrescenta os acessores de
aeronave — `getMach()`, `getGload()`, `getAngleOfAttack()`, `getFuelWt()`, `getEngThrust()`,
`setThrottles()` — que delegam ao `DynamicsModel`. É por eles que a percepção lê o 6-DOF sem
conhecer o JSBSim.

### 6.2 `( JSBSimModel )` — a dinâmica 6-DOF, hoje pilotando um A-4

`models::JSBSimModel : DynamicsModel : System`. É o adaptador nativo para a
[JSBSim](https://jsbsim.sourceforge.net/): monta uma `FGFDMExec`, carrega a aeronave de
`rootDir`/`model` (aqui, `"A4"`), e a cada fase 0 roda um passo do FDM completo — aerodinâmica,
propulsão, massa, atmosfera, trem de pouso.

O comando não é por chamada de método: o `Autopilot` escreve nas **propriedades JSBSim**
`ap/heading_setpoint`, `ap/altitude_setpoint`, `ap/airspeed_setpoint` (mais os respectivos
`*_hold`), e é o autopilot **do modelo da aeronave** que fecha a malha até as superfícies.

> **A aeronave é dado do MODELO, não do cenário — e por que é um A-4.** A cadeia
> `Autopilot → JSBSimModel → superfícies` só funciona se o modelo JSBSim tiver autopilot próprio.
> O A-4 de fábrica (gerado por Aeromatic) não vem com nenhum, então
> `models/players/A-4/data/jsbsim/aircraft/A4/a4ap.xml` foi escrito **para esta poc**, no mesmo
> papel que um `c310ap.xml` cumpriria — com ele, a cadeia funciona sem uma linha de lei de controle
> própria. O motor é um **J52** (turbina única, não par de motores a pistão): `getEngRPM()` devolve
> `GetN2()` em **percentual** (~60–100), nunca RPM absoluto — polimorfismo de unidade documentado em
> [`models/players/A-4/docs/ARCHITECTURE.md`](../../../../models/players/A-4/docs/ARCHITECTURE.md).
> Ao contrário de um motor a pistão, uma turbina **liga sozinha**: `FGTurbine::InitRunning()` já
> força `Running=true` com N2 em idle, e o giro relativo ao vento em voo faz o resto — este modelo
> **não** precisa de um `systems/engine-autostart.xml` como um motor a pistão precisaria.

> **Limite conhecido, documentado em `docs/ARCHITECTURE.md`, não escondido.** Os dados
> aerodinâmicos do A-4 (Aeromatic) têm um modo látero-direcional levemente instável em espiral, e
> `JSBSimModel::reset()` nativo **não** roda `FGTrim` (só `RunIC()`) — a aeronave nasce destrimada.
> `a4ap.xml` ganhou um SAS sempre-ativo (nivelador de asas + amortecedores de taxa em
> rolagem/arfagem, independente dos *hold modes* do `Autopilot`) e um trim estático de profundor;
> sem eles a aeronave diverge em poucos segundos, mesmo com o piloto automático desligado. Com o
> SAS, o determinismo (1/2/4 threads, ver [§12](#12-determinismo)) continua byte a byte, mas o
> voo ainda deriva lentamente (dezenas de segundos) para fora do nível antes de o nivelador
> reafirmar o controle — característica aerodinâmica real do dado, não uma regressão.

Três cicatrizes desta peça estão em [§13.1](#131-modelsjsbsimmodel-é-final),
[§13.2](#132-setcommandedaltitude-e-companhia-ignoram-os-limites-de-taxa-do-autopilot-com-jsbsimmodel)
e [§13.3](#133-o-autopilot-do-a-4-não-fecha-malha-de-velocidade-sozinho).

### 6.3 `( Autopilot )` — o controle

`models::Autopilot : Pilot : System`. Roda na fase 3 e converte comandos de alto nível (rumo,
altitude, velocidade) em chamadas ao *dynamics model*. Traz de graça `headingHoldMode`,
`altitudeHoldMode`, `velocityHoldMode`, `navMode` (seguir uma `Route`) e `followTheLeadMode` (voo em
formação) — é o que dispensa escrever à mão a malha rumo→banco→aileron e a malha
altitude→arfagem→profundor.

> **Unidade:** `setCommandedAltitudeFt()` é em **pés**; o resto deste modelo trabalha em metros. A
> conversão acontece na fronteira, em
> [`ubf/FlightAction.cpp`](../../../../models/players/A-4/src/ubf/FlightAction.cpp).

> **Os limites de taxa do `.edl` (`maxRateOfTurnDps`, `maxBankAngle`, `maxPitchAngle`,
> `maxClimbRateMps`, `maxAcceleration`) são DECORATIVOS para `JSBSimModel`.** Confirmado lendo
> `JSBSimModel.cpp` (documentado em `docs/ARCHITECTURE.md`): `setCommandedHeadingD()`,
> `setCommandedAltitude()` e `setCommandedVelocityKts()` ignoram o 2º/3º parâmetro — os que
> carregariam esses limites. Este cenário continua declarando os cinco por documentação/coerência
> com a aeronave (`falcon1..4` usam `maxRateOfTurnDps: 6.0`/`7.0`, `maxBankAngle: 45.0`/`48.0`,
> `maxClimbRateMps: 40.0`), mas quem de fato limita a taxa de subida/descida e de guinada é o
> **próprio `a4ap.xml`** — o SAS e os canais de controle da aeronave, não este slot. Isso não é um
> defeito local desta poc: é como `JSBSimModel` sempre se comportou, para qualquer aeronave.

### 6.4 O radar nativo — `Gimbal` + `Antenna` + `Tws` + `AirTrkMgr`

Quatro classes, quatro papéis, três fases:

| classe | papel | fase |
|---|---|---|
| `Gimbal` | aponta e **filtra** os *players of interest* (tipo, alcance, ângulo, horizonte, oclusão de terreno) | fundo |
| `Antenna` | padrão de ganho (`Func1`/`Table1`), volume de busca, ERP, entrega a emissão | 1 |
| `Tws` (*Track While Scan*) | modelo de radar: potência, frequência, PRF, largura de pulso, limiar S/I | 1 e 2 |
| `AirTrkMgr` | correlação e filtro alfa-beta; produz os `RfTrack` | 3 |

O que se herda aqui é a equação do radar inteira, com RCS do alvo, perdas de propagação de ida e
volta e limiar de detecção — e a correlação de pistas com *gates* de posição, alcance e velocidade.
Ver o percurso completo em [§9.3](#93-canal-1--rf-emissão-eco-pista).

**Duas regras que o sensor nativo não implementa** e por isso moram em
[`libs/xtrack/TrackQuery`](../../../../libs/xtrack/TrackQuery.hpp) (`shared_library()`, parte do
SDK): o radar **não filtra por lado** (`playerOfInterestTypes` filtra por *tipo* de player, não por
*side* — a esquadrilha inteira aparece como pista), e a escolha do "contato mais próximo" precisa de
desempate determinístico.

### 6.5 `xnative::AlertDatalink : models::Datalink` — a interação

Herda o transporte inteiro e acrescenta **só** o que o framework não tem como saber: o que fazer
com a mensagem recebida — e uma **segunda** via de entrega própria, além do `sendMessage()` nativo
(ver [§9.4](#94-canal-2--datalink)). Detalhes no
[header da classe](../../../../models/players/A-4/include/xnative/AlertDatalink.hpp), que documenta
o que vem de graça e os dois enganos fáceis.

### 6.6 `( FlightAgentTC )` — o agente

[`include/xnative/FlightAgentTC.hpp`](../../../../models/players/A-4/include/xnative/FlightAgentTC.hpp) ·
[`src/xnative/FlightAgentTC.cpp`](../../../../models/players/A-4/src/xnative/FlightAgentTC.cpp) —
menos de 50 linhas de implementação, e **cada bloco delas existe por causa de uma armadilha do
framework**. É o resumo mais honesto do que custa mover uma decisão para dentro do frame de tempo
crítico.

```cpp
class FlightAgentTC : public base::ubf::AgentTC
{
   DECLARE_SUBCLASS(FlightAgentTC, base::ubf::AgentTC)
public:
   FlightAgentTC();
   long getDecisionCount() const;
   int  getLastThreadTag() const;
   void updateData(const double dt = 0.0) override;   // armadilha 3
protected:
   void controller(const double dt = 0.0) override;   // armadilha 2
   void initActor() override;
private:
   std::atomic<long> decisions{};
   std::atomic<int>  lastThreadTag{-1};
};
```

#### 6.6.1 Armadilha 1 — `UbfAgentTC` existe, mas nenhuma factory do MIXR o constrói

`base::ubf::AgentTC` está lá, pronto, no mesmo header do `Agent`. Mas `base/factory.cpp` registra
apenas `"UbfAgent"` e `"UbfArbiter"` — escrever `( UbfAgentTC ... )` no EDL **não constrói nada**.

**Um agente de tempo crítico é, na prática, código da aplicação**: a classe é do framework, o
registro é nosso. Uma linha em
[`src/xnative/factory.cpp`](../../../../models/players/A-4/src/xnative/factory.cpp):

```cpp
else if ( name == FlightAgentTC::getFactoryName() )  obj = new FlightAgentTC();
```

> **Detalhe do slottable.** A classe usa `EMPTY_SLOTTABLE`, e ainda assim `state:` e `behavior:`
> funcionam no `.edl`: a busca de slot **sobe a hierarquia**, e esses dois slots são de
> `ubf::Agent`. `EMPTY_SLOTTABLE` aqui significa "não acrescento slot nenhum", não "não tenho
> slots".

#### 6.6.2 Armadilha 2 — `AgentTC::updateTC()` chama `controller()` em **toda** fase

O agente é um `base::Component` dentro do player, e a lista de players é percorrida **4× por
frame** — uma vez por fase, com `dt/4`. `AgentTC::updateTC()` não filtra nada:

```cpp
void AgentTC::updateTC(const double dt) { controller(dt); }   // framework
```

Sem filtro, a decisão rodaria **4 vezes por frame**, três delas nas fases erradas (dinâmica,
transmissão, recepção) — lendo um estado meio atualizado e comandando o autopilot antes de o
`AirTrkMgr` ter processado as pistas. O filtro é o mesmo que `models::System` aplica às suas quatro
fases:

```cpp
const models::WorldModel* const world{player->getWorldModel()};
if (world == nullptr) return;

if (world->phase() != 3) return;      // a decisão pertence à fase "lógica e controle"

const int tag{xboard::threadTag()};
lastThreadTag.store(tag, std::memory_order_relaxed);
xboard::setThreadTag(player->getID(), tag);

BaseClass::controller(dt * 4.0);      // e com o dt do frame INTEIRO, não dt/4
decisions.fetch_add(1, std::memory_order_relaxed);
```

**O `dt * 4.0` não é detalhe.** O `domain::PatrolPlan` integra o relógio da perna com esse `dt`, e a
`domain::ThreatPolicy` envelhece a histerese com ele. Passar `dt/4` faria a patrulha e a histerese
andarem 4× mais devagar — e nada acusaria o erro.

#### 6.6.3 Armadilha 3 — `Agent::updateData()` **também** chama `controller()`

Esta é a que morde de verdade. `AgentTC` **acrescenta** `updateTC()` mas **não desliga** o caminho
de background herdado de `Agent`:

```cpp
void Agent::updateData(const double dt) { controller(dt); }   // framework — continua valendo
```

E `Player::updateData()` propaga para a lista de componentes. Ou seja: um agente dentro do player
decidiria **duas vezes por frame** — uma no tempo crítico e outra no background.

**E o filtro de fase não salva:** ao fim do `tcFrame()` a fase corrente **fica em 3**, então a
chamada de background passaria direto pelo `if`. Por isso:

```cpp
void FlightAgentTC::updateData(const double) { }   // no-op deliberado
```

Não se está suprimindo trabalho: o `Agent` nativo **não** repassa `updateData()` ao
`state`/`behavior` — o próprio `BtBehavior` já configura seus planos preguiçosamente por causa
dessa mesma limitação (ver [§13.4](#134-o-agent-não-propaga-o-ciclo-de-componentes)) —, então não
há nada a propagar aqui.

#### 6.6.4 O ator vem do container

```cpp
void FlightAgentTC::initActor()
{
   if (getActor() != nullptr) return;
   const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
   if (player != nullptr) setActor(player);
}
```

O `SimAgent` nativo faria o contrário: moraria na `Station` e resolveria o ator **por nome**,
procurando na lista de players do `WorldModel` (slot `actorPlayerName`). Aqui basta **subir a
cadeia de containers** — e a consequência prática é que **o bloco EDL fica idêntico para as quatro
aeronaves**, sem nenhum nome a amarrar. Um slot `*Name:` a menos é uma classe inteira de erro
silencioso a menos (ver [§3](#3-como-o-framework-chama-o-nosso-código)).

#### 6.6.5 Os dois contadores, e por que um deles não entra no dump

```cpp
std::atomic<long> decisions{};      // observável E determinístico: 1 por frame
std::atomic<int>  lastThreadTag{-1}; // depende do escalonador
```

`decisions` é o que **prova** que a decisão está amarrada ao frame — tem de bater com o número de
frames em 1, 2 ou 4 threads. `lastThreadTag` **não** entra no dump determinístico: depende do
escalonador. Aparece só no status humano, para mostrar que os quatro agentes rodam em threads
diferentes.

Os dois são `std::atomic` porque são escritos na thread T/C e lidos no laço de background.
`memory_order_relaxed` basta: são contadores de diagnóstico, não sincronizam nada.

> **Nota de arquitetura, mais recente que os dois parágrafos acima.** `getDecisionCount()`/
> `getLastThreadTag()` continuam existindo, mas hoje **nada os chama** de fora — o dump e a linha
> de status leem `dec=`/`thr=` de `mixr::xboard::Readout`, publicado por
> `ubf::FlightAction::execute()` (`xboard::bumpDecisionCount()`/`setThreadTag()`), o mesmo ponto de
> atuação que qualquer agente usaria. `FlightAgentTC::controller()` também escreve
> `xboard::setThreadTag()`, redundante mas inofensivo — os dois atômicos desta classe ficaram como
> contadores internos, verificáveis só por quem tem acesso direto ao objeto.

#### 6.6.6 `findFlightAgent()` — a busca por tipo

```cpp
const FlightAgentTC* findFlightAgent(const models::AirVehicle* air)
{
   const base::Pair* const pair{air->findByType(typeid(FlightAgentTC))};
   ...
}
```

Como o agente é componente do **player**, a busca é a mesma que o framework usa para os
subsistemas: **por tipo**, na lista de componentes. Devolve `nullptr` se a aeronave não declarar
um `( FlightAgentTC )` no `.edl` — o caso do próprio `bandit1`, que nunca teve agente algum (é um
intruso scriptado, só `Autopilot`; ver [`../bandit/`](../bandit/)).

### 6.7 As peças que continuam nossas

As nove da [seção 1](#1-o-que-vem-do-framework-e-o-que-é-nosso), mais dois utilitários de runtime
compartilhados (`libs/xboard`, com `threadTag()`/`currentCpu()`/`Readout`) e a camada `domain/`
inteira — que não é MIXR nem BT, e é justamente por isso que ela é a parte testável sem simulação.

---

## 7. Dissecação: o repositório em ordem de dependência

Esta seção percorre os arquivos do **modelo** — `models/players/A-4/` — na ordem em que se pode
lê-los sem precisar saltar para frente: cada camada só depende das anteriores. A aplicação host
(`./app`, compartilhada por todas as pocs) não entra aqui — tem seção própria em
[`app/README.md`](../../../../app/README.md).

```
CAMADA 1  domain/          C++ puro. Zero includes de MIXR, BT ou JSBSim.
   ↓
CAMADA 2  xnative/ (util)  threadTag()/currentCpu() -- promovidos para libs/xboard/Board.hpp.
   ↓
CAMADA 3  xnative/ (MIXR)  AlertDatalink, events::TacticalAlert, ThreadTagProbe. Herdam do framework.
   ↓
CAMADA 4  ubf/             FlightState, BtTuning, BtBehavior, FlightAction, AltitudeSafetyBehavior,
   ↕                       RLBridgeBehavior.  (ciclo controlado com bt/ -- explicado em 7.4)
CAMADA 5  bt/              NodeContext, DecisionContext, os nós, bt_factory[.+_sdk].
   ↓
CAMADA 6  xnative/factory.cpp  registra as nove classes -- consumido por libs/xplugin (dlopen)
   ↓
CAMADA 7  configs/ e data/     o que não é código
```

### 7.1 Camada 1 — `domain/`: as regras puras

Sete arquivos, **nenhum** deles inclui um header do MIXR, do BehaviorTree.CPP ou do JSBSim. É a
camada que se pode compilar e testar sem levantar simulação nenhuma — e é o que `tests/domain/` do
próprio modelo faz (42 testes, ~10 ms), cobrindo a histerese da evasão, o alvo fixado na entrada, o
lado da quebra e o piso anti-CFIT. Todas as unidades trazem a unidade no **nome do campo**
(`altitudeM`, `speedKts`, `headingDeg`) — a armadilha clássica deste repositório é misturar pés,
metros e nós.

**1. [`domain/FlightCommand.hpp`](../../../../models/players/A-4/include/domain/FlightCommand.hpp)**
— a base de tudo. Um DTO de três campos:

```cpp
struct FlightCommand {
   double headingDeg{};    // rumo verdadeiro comandado (graus)
   double altitudeM{};     // altitude comandada (metros)
   double speedKts{};      // velocidade comandada (nós)
};
```

Todo plano de voo e toda política produzem **isto**. É o contrato entre a decisão e a atuação, e é
o motivo de a árvore de comportamento nunca tocar num objeto MIXR.

**2. [`domain/geometry.hpp`](../../../../models/players/A-4/include/domain/geometry.hpp)** —
matemática de plano tangente. Opera no NED da *gaming area* do `WorldModel` (x = Norte, y = Leste,
metros a partir do ponto de referência do cenário):

| função | o que faz |
|---|---|
| `wrap180(deg)` | normaliza para `(-180, 180]` |
| `wrap360(deg)` | normaliza para `[0, 360)` |
| `headingToDeg(fromN, fromE, toN, toE)` | `atan2(ΔLeste, ΔNorte)` — zero aponta ao norte e cresce para leste |
| `distanceM(...)` | distância horizontal euclidiana |
| `relativeTo(...)` | devolve `RelativeGeometry{rangeM, bearingDeg, relBearingDeg, deltaAltM}` |

A ordem dos argumentos do `atan2` é a pegadinha: em navegação o ângulo é medido do Norte, então é
`atan2(E, N)`, e não o `atan2(y, x)` matemático.

**3. [`domain/PatrolPlan.hpp`](../../../../models/players/A-4/include/domain/PatrolPlan.hpp)** —
circuito cíclico. `configure()` recebe rumo inicial, duração da perna, curva por perna, altitude e
velocidade; `advance(dt)` integra o relógio da perna e devolve `true` quando trocou; `command()`
devolve `startHeading + turnPerLeg × leg`. Com `turnPerLegDeg=90` o circuito é um quadrado; com 120,
um triângulo; com 60, um hexágono.

> **Detalhe de projeto que vale copiar:** `advance(dt)` só é chamado **pelo nó `Patrol`**. Quando o
> avião está em RTB ou evadindo, o relógio da perna **não corre** — a patrulha é retomada
> exatamente de onde parou, em vez de "pular" o tempo em que esteve ocupado.

> **Jitter de rumo opcional.** `setHeadingJitter(amplitudeDeg, seed)` liga um pequeno offset
> aleatório, resorteado a cada troca de perna — nunca por `dt`, que é o que mantém o determinismo
> entre 1/2/4 threads (o número de trocas de perna independe de quantas threads existem; a cadência
> de `dt` por frame, não). A classe só semeia e sorteia — a semente que chega já é o resultado final
> de uma hierarquia de derivação (semente mestra do cenário → hash do nome do player → salt de
> propósito), calculada em `BtBehavior::configurePlans()` com `libs/xrandom` (header-only, ver
> `CLAUDE.md`, seção `libs/xrandom`). `patrolJitterHeading: ( Degrees 6 )` e
> `patrolMasterSeed: 20260903` são o mesmo par de valores nos quatro falcons deste cenário.

**4. [`domain/RtbPlan.hpp`](../../../../models/players/A-4/include/domain/RtbPlan.hpp)** — retorno à
base (a origem da área de jogo). `command(ownN, ownE, ownHeading)` aponta para a base; ao chegar
dentro de `arrivalRadiusM`, mantém o rumo e reduz a velocidade a 60 %. Não modela reabastecimento —
isso é estado do sistema de combustível, não do plano.

**5. [`domain/TerrainFloor.hpp`](../../../../models/players/A-4/include/domain/TerrainFloor.hpp)** —
a regra de terreno, em duas funções e um struct:

```cpp
struct GroundReference { bool valid{}; double elevationM{}; };

double terrainFloorM(const GroundReference&, double clearanceM, double absoluteFloorM);
double clampToTerrain(double commandedAltM, const GroundReference&,
                      double clearanceM, double absoluteFloorM);
```

Duas camadas, e a de baixo é a que importa: **terreno + folga** quando há dado, e **piso absoluto**
sempre, como mínimo. O piso absoluto não é redundância — o `Player::updateElevation()` nativo
ignora o retorno de `getElevation()`, então não existe forma honesta de perguntar "estou coberto?"
(ver [§10](#10-elevação-de-terreno)). `clearanceM <= 0` desliga a camada de terreno sem desligar a
rede, que é o que torna possível o controle negativo do cenário sem recompilar.

**6. [`domain/ThreatPolicy.hpp`](../../../../models/players/A-4/include/domain/ThreatPolicy.hpp)** —
a manobra de evasão, e o único arquivo de `domain/` com estado interno de máquina:

```
livre        sem contato e sem histerese   → engaged() == false
em manobra   com contato                   → alvo FIXADO, timer cheio
em arrasto   sem contato, timer > 0        → mesmo alvo, timer caindo
```

`update()` calcula o comando **uma única vez, na entrada da manobra**:

- rumo: marcação **absoluta** do contato (`ownHeading + relBearing`) deslocada por `breakTurnDeg`
  para o lado oposto ao que ele ocupa;
- altitude: `ownAlt ∓ climbM`, no sentido contrário ao do contato, e então **passada pelo piso de
  terreno** (`clampToTerrain`);
- velocidade: `dashSpeedKts`.

As três correções que estão neste arquivo — alvo fixado na entrada, rumo relativo ao contato, e
histerese — estão explicadas na [seção 8](#8-a-cadeia-de-decisão-ubf--behaviortree) e na
[seção 10](#10-elevação-de-terreno).

### 7.2 Camada 2 — `xnative/`, o utilitário de runtime

**`threadTag()`/`currentCpu()`** — mora hoje em
[`libs/xboard/Board.hpp`](../../../../libs/xboard/Board.hpp), não mais em um `xnative/ThreadTag.*`
próprio deste modelo. Promovido para lá porque **mais de um plugin no mesmo processo** (`flight` e,
em outro cenário, o modelo de míssil) precisa da MESMA numeração para a mesma thread física do
pool, não uma por `.so`. `threadTag()` devolve um índice pequeno e estável (0, 1, 2…) para a thread
chamadora, e `currentCpu()` chama `sched_getcpu()`. Existe porque o MIXR **não expõe os handles do
seu pool** (`tcThreads` é privado em `simulation::Simulation`), mas o nosso código *roda* nessas
threads — então dá para registrar, de dentro, quem está processando cada player. É assim que o
*round-robin* do pool fica observável (ver [§15](#15-o-que-foi-medido-rodando)).

A implementação tem um detalhe que vale copiar:

```cpp
static thread_local int cachedTag{-1};
if (cachedTag >= 0) return cachedTag;    // o mutex global é tocado UMA vez por thread
```

Sem o cache haveria um lock global no caminho quente (todo player, todo frame) — exatamente o tipo
de serialização que anularia o pool de threads do framework.

**`xboard::Readout`/`setBehaviorLabel()`/`bumpDecisionCount()`/`setThreadTag()`/`setAlert()`/
`setDatalinkCounters()`** — o quadro de leitura sob mutex que substitui o antigo
`xnative::BehaviorBoard`. O player é o `models::Aircraft` nativo, que não tem campo próprio para
guardar rótulo de comportamento nenhum; um quadro global por id resolve sem subclassear o `Player`
só por causa de uma string. É escrito pela atuação (thread de tempo crítico, um write por decisão) e
lido pelo dump/status (laço de background).

### 7.3 Camada 3 — `xnative/`, as classes derivadas do MIXR

**[`events::TacticalAlert`](../../../../models/events/payloads/EID_ALERT/TacticalAlert.hpp)** — a
carga útil do alerta. Mora em `models/events/`, não em `xnative/`, porque um `dynamic_cast` de um
`base::Object*` recebido de **outro** `.so` só é seguro se a classe do payload vier de uma
`shared_library()` linkada por AMBOS os lados (ver `models/events/README.md`). É um `base::Object`
de verdade (ref-contado, com a RTTI do framework), como a `Emission` do radar: é assim que o MIXR
transporta dados em eventos, e não com um struct solto. Campos deliberadamente **crus** — posição
NED em metros, alcance, nome do contato, id e nome do emissor. **O alerta não carrega nenhuma
ordem**: quem recebe decide o que fazer, o que mantém o emissor ignorante sobre o comportamento do
receptor.

> Não use `models::Message` para isso: ela existe no framework e **ninguém a usa**; `sendMessage()`
> recebe um `base::Object*` opaco.

**[`xnative/AlertDatalink.hpp`](../../../../models/players/A-4/include/xnative/AlertDatalink.hpp)**
— depende de `events::TacticalAlert`. Herda `models::Datalink` e acrescenta um único slot
(`holdTime`) e o resto do comportamento, hoje entregue por **DOIS** caminhos independentes:

| caminho | método | thread | o que faz |
|---|---|---|---|
| (a) nativo | `broadcastAlert()` chama `sendMessage()` (framework) | do **emissor** | varre `getWorldModel()->getPlayers()`, entrega `event(DATALINK_MESSAGE, msg)` local, alimenta a fila de rede — mas **só alcança player com `Datalink`** |
| (b) direto | `broadcastAlert()` também chama `player->event(events::EID_ALERT, msg)` | do **emissor** | mesmo laço, sem passar pelo `Datalink` — alcança **qualquer** player local ativo, com ou sem `Datalink` |

O segundo caminho existe porque `sendMessage()` não alcança um player sem subsistema de rádio (um
flyout de arma, por exemplo) — ver `models/events/README.md`. Nos dois casos:

```cpp
bool AlertDatalink::onDatalinkMessageEvent(base::Object* const msg)   // roda na thread do EMISSOR
```

só *encena* o alerta em `staged`, sob mutex curto, com fusão **comutativa** (vence o contato mais
próximo; empate exato → menor id de emissor). `receive(dt)` (fase 2, do **receptor**) promove
`staged → current`, publica no `xboard` **fora** do próprio mutex (segurar os dois ao mesmo tempo
criaria uma ordem de travamento sem necessidade) e envelhece com `holdTime`.

O `reset()` faz duas chamadas que não são óbvias:

```cpp
setNetworkQueueEnabled(false);   // sem NetIO no cenário, ninguém drenaria a fila de rede
setLocalSendEnabled(true);
```

Por que a disciplina encena/promove existe, e por que a fusão é comutativa, está em
[§9.7](#97-o-caminho-desta-poc-fim-a-fim). É ela que sustenta o determinismo
([§12](#12-determinismo)).

**[`xnative::ThreadTagProbe`](../../../../models/players/A-4/include/xnative/ThreadTagProbe.hpp)**
— um `base::Component` genérico, sem slot nenhum, que só publica `xboard::setThreadTag()` no
`updateTC()`, filtrado à fase 3 (mesmo filtro do `FlightAgentTC`). Não deriva de `ubf::Agent`, não
decide nada — só existe para tornar observável a thread de um player **sem** agente próprio (o
`bandit1` de outros cenários deste repositório, um míssil de outro modelo). Nenhum player deste
cenário o declara (os quatro falcons já têm `FlightAgentTC`); existe porque é uma das nove classes
que `libflight.so` publica.

**[`libs/xtrack/TrackQuery`](../../../../libs/xtrack/TrackQuery.hpp)** — uma função livre,
`nearestHostileTrack(air)`, que percorre `AirVehicle → OnboardComputer → TrackManager("twsTrkMgr")
→ Track` e devolve um `TrackInfo`. Está numa `shared_library()` própria porque é consultada em
**dois lugares muito diferentes** — a percepção do UBF e o dump/status da aplicação — e os dois
precisam dizer a mesma coisa. As duas regras que ela acrescenta ao sensor nativo (filtro por lado e
desempate determinístico) estão em [§6.4](#64-o-radar-nativo--gimbal--antenna--tws--airtrkmgr).

### 7.4 Camada 4 — `ubf/`: percepção, decisão, atuação

Aqui aparece o único ciclo de dependência do modelo, e ele é **deliberado e controlado**:

```
ubf/BtBehavior.hpp  ──inclui──→  bt/NodeContext.hpp   (precisa do struct FlightDecision)
bt/nodes/*.cpp      ──inclui──→  ubf/BtBehavior.hpp   (os nós leem o snapshot pelo behavior)
```

O ciclo não fecha porque **`bt/NodeContext.hpp` só faz *forward declaration*** de
`bt_nodes::DecisionContext`. Ou seja: `bt/` conhece o *nome* da interface, mas não a definição —
quem inclui a definição são os `.cpp` dos nós. É o padrão clássico de quebra de ciclo, e é a razão
de `NodeContext` ser um struct de **um ponteiro só**.

**1. [`ubf/FlightState.hpp`](../../../../models/players/A-4/include/ubf/FlightState.hpp)** — a
**percepção**. Herda `base::ubf::AbstractState`. Um único método útil,
`updateState(const base::Component* actor)`, que lê o ator e monta um `domain::WorldView` de
números crus (`using Snapshot = domain::WorldView`, mantido como alias — a estrutura em si mora em
`domain/`, para quem *consome* a percepção compilar sem o MIXR).

Repare na assinatura: **o ator chega como `const Component*`** — percepção lê, não atua. Os campos:

| grupo | campos | de onde vem |
|---|---|---|
| identidade | `ownerName` | nome EDL do ator — a forma confiável de saber "quem decide" (ver a nota sobre o monitor do Groot, adiante) |
| próprio | `northM`, `eastM`, `altitudeM`, `headingDeg`, `speedKts`, `rollDeg`, `pitchDeg` | `AirVehicle` |
| telemetria 6-DOF | `fuelFraction`, `mach`, `gLoad`, `alphaDeg` | `AirVehicle` → `DynamicsModel` |
| **solo** | `terrainValid`, `terrainElevM`, `altitudeAglM` | `Player::updateElevation()` (background) |
| contato | `hasContact`, `contactName`, `contactRangeM`, `contactRelBearingDeg`, `contactDeltaAltM`, `contactNorth/East/AltitudeM` | `libs/xtrack::nearestHostileTrack()` |
| alerta | `hasAlert`, `alertSender`, `alertContactName`, `alertNorth/East/AltitudeM`, `alertRangeM` | `AlertDatalink::getAlert()` |
| armamento | `weaponReady` | `StoresMgr` — inerte em produção: nenhum falcon declara `stores:` |
| navegação | `hasNavSteering`, `navTrueBrgDeg`, `hasNavCmdAlt`, `navCmdAltM`, `hasNavCmdSpeed`, `navCmdSpeedKts` | `Navigation::updateNavSteering()` nativo — não usados por `flight_tree.xml` (usados por outra árvore, `flight_tree_nav.xml`, de outra poc) |

> **Armadilha do framework documentada no próprio header:** um `Agent` **não propaga**
> `updateTC()`/`updateData()` aos filhos, e o `state` é filho do agente. Este objeto **nunca**
> recebe o ciclo normal de componentes — tudo o que ele precisa fazer tem de estar dentro de
> `updateState()`.

**2. [`ubf/BtTuning.hpp`](../../../../models/players/A-4/include/ubf/BtTuning.hpp)** — um struct sem
lógica e sem tipo MIXR, com os números que o EDL ajusta e os *defaults* visíveis lado a lado. Existe
por dois motivos práticos: `BtBehavior::copyData()` copia **um** membro em vez de dezenove, e
acrescentar um parâmetro passa a ser uma linha aqui e uma no slot — sem risco de esquecer a cópia.

**3. [`ubf/BtBehavior.hpp`](../../../../models/players/A-4/include/ubf/BtBehavior.hpp) +
[`ubf/BtBehaviorSlots.cpp`](../../../../models/players/A-4/src/ubf/BtBehaviorSlots.cpp)** — a
**decisão**. Herda `base::ubf::AbstractBehavior`. São dois arquivos `.cpp` para a **mesma classe**,
separados por questão:

- `BtBehavior.cpp` — ciclo de vida da árvore e o tick que produz a ação;
- `BtBehaviorSlots.cpp` — a **fronteira com o EDL**: `BEGIN_SLOTTABLE`, `BEGIN_SLOT_MAP` e **19**
  setters (`treeFile` … `patrolSeedOverride`), cada um fazendo as mesmas três coisas (recusa nulo,
  converte para a unidade interna, valida a faixa) e escrevendo no `BtTuning`. Este cenário declara
  **18** deles em cada falcon — o 19º, `patrolSeedOverride`, é o *escape hatch* que pula a derivação
  de semente por nome, não usado em produção (ver `CLAUDE.md`, seção `libs/xrandom`).

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

- **`configurePlans()` é preguiçoso.** `reset()` nunca chega ao `behavior` de um `Agent`
  (ver [§13.4](#134-o-agent-não-propaga-o-ciclo-de-componentes)), então a configuração vinda dos
  slots é aplicada na primeira decisão. Como este cenário não usa `( UbfArbiter )`, `BtBehavior`
  está **direto** sob `agent:` — mas a mesma preguiça vale para um comportamento aninhado mais
  fundo dentro de um árbitro, como `src/rl` ainda usa.
- **`buildTree()` é protegido por mutex global.** `BT::BehaviorTreeFactory::createTreeFromFile()`
  **não é reentrante**, e os quatro aviões chegam ao primeiro `genAction()` ao mesmo tempo, em
  threads T/C diferentes.

Cada `BtBehavior` — isto é, **cada aeronave** — tem a sua própria `BT::BehaviorTreeFactory` e a sua
própria `BT::Tree`. Não há árvore compartilhada.

**4. [`ubf/FlightAction.hpp`](../../../../models/players/A-4/include/ubf/FlightAction.hpp)** — a
**atuação**. Herda `base::ubf::AbstractAction`. É o **único** ponto do modelo que escreve nos
subsistemas a partir da decisão, e também o único ponto que loga (via `libs/xlog`).

```cpp
bool execute(base::Component* actor) override;   // o ator chega como PARÂMETRO
```

Essa assinatura é o que desacopla decisão de atuação no UBF: a ação **não guarda ponteiro para o
ator**, não o conhece na construção, e pode ser gerada por um comportamento que nem sabe de quem é
a aeronave. O corpo acha o autopilot por tipo (`getPilotByType(typeid(models::Autopilot))`), liga
os três *hold modes*, comanda rumo/altitude/velocidade — **convertendo metros para pés aqui, na
fronteira** — grava o rótulo no `xboard` e, se pedido, chama `AlertDatalink::broadcastAlert()`.

Registra em `LOG(...)` (`libs/xlog`, uma única cópia no processo — o que o modelo escreve aparece
sozinho na aba "Log" do `./app`, quando ela existe): `INFO` na transição de comportamento
(`falcon1: PATROL -> EVADE`), `WARNING` no alerta tático transmitido, `ERROR` quando o ator não tem
`Autopilot` (a decisão não pode ser atuada — antes, essa falha era muda), e `DEBUG` de batimento a
cada 500 decisões atuadas, com a thread que decidiu. O pedido de alerta fica **ligado** enquanto a
aeronave evade — logar por decisão daria dezenas de linhas por segundo, então há uma regra de
**borda** (`changedFor()`, mapa estático com mutex) que emite só na transição.

**5. [`ubf/AltitudeSafetyBehavior.hpp`](../../../../models/players/A-4/include/ubf/AltitudeSafetyBehavior.hpp)**
— o segundo comportamento que o modelo exporta, pensado para votar **90** (maior que o 50 da árvore)
dentro de um `( UbfArbiter )`. Depende de `FlightState`, `FlightAction` e `domain/TerrainFloor`.

**Este cenário não a instancia** — ver a nota "SEM ARBITRO" e a [§1](#1-o-que-vem-do-framework-e-o-que-é-nosso).
A classe continua compilada e exportada, só não faz parte do `behavior:` de nenhum dos quatro
agentes. Existe para mostrar **composição do UBF que não passa pela árvore**: uma regra dura ficaria
fora da política tática, e quando a aeronave furasse o piso a ação dela venceria por voto, sem que a
árvore precisasse saber que ela existe. Devolver `nullptr` — "não recomendo nada" — é legítimo e é o
caso normal quando ela roda. É exatamente esse desenho que [`src/rl`](../../../../src/rl/) usa hoje,
de verdade, para blindar uma política de RL ruim.

**6. [`ubf/RLBridgeBehavior.hpp`](../../../../models/players/A-4/include/ubf/RLBridgeBehavior.hpp)**
— a ponte com `src/rl`: publica a percepção como `Observation` via `libs/xrlbridge` e consome um
`Command` pendente. Também não instanciada por este cenário; mora aqui (dentro de `libflight.so`,
não num plugin próprio) porque precisa de `dynamic_cast` para tipos concretos do modelo
(`xnative::FlightState`), frágil de atravessar a fronteira de `dlopen` entre dois `.so` distintos.

### 7.5 Camada 5 — `bt/`: a árvore de comportamento

**1. [`bt/NodeContext.hpp`](../../../../models/players/A-4/include/bt/NodeContext.hpp)** — dois
tipos, e nenhum inclui MIXR. `FlightDecision` é o que a árvore **produz** num tick: um
`FlightCommand`, um rótulo, e o pedido de transmissão do alerta. Os nós não tocam em objeto MIXR
nenhum — eles só preenchem esta estrutura. Quem a transforma em atuação é o `FlightAction`.
`NodeContext` é a dependência fixa dos nós: **um ponteiro** para o comportamento que os hospeda —
mas para a **interface**, não para a classe concreta.

> **[`bt/DecisionContext.hpp`](../../../../models/players/A-4/include/bt/DecisionContext.hpp)** é
> **oito** getters (`snapshot()`, `decision()`, `patrolPlan()`, `rtbPlan()`, `threatPolicy()`,
> `getFrameDt()`, `getFuelReserve()`, `getSupportSpeedKts()`). `BtBehavior` a implementa sem um
> método novo — as assinaturas já eram estas. Resultado: `bt/nodes/*.cpp` compilam com BT.CPP +
> `domain/` apenas. `tests/tree/` do modelo carrega o `flight_tree.xml` **de produção** contra um
> `FakeDecisionContext` e verifica qual ramo venceu — `ldd` no binário de teste mostra **zero**
> bibliotecas do MIXR.

> **Por que injeção por construtor e não pelo blackboard.** O blackboard do BehaviorTree.CPP é para
> dados que fluem **entre nós**, não para injeção de dependência. E há uma armadilha registrada:
> `Blackboard::create(parent)` **não** compartilha entradas automaticamente via `get`/`set`. Aqui a
> árvore é criada com um blackboard vazio, que ninguém usa para estado.

**2. Os sete nós que `flight_tree.xml` usa** (de um total de onze registrados no modelo — os outros
quatro servem outras árvores: `Navigate`, `OnnxPolicy`, `OnnxScore`, `PyDecide`), em
[`src/bt/nodes/`](../../../../models/players/A-4/src/bt/nodes/) — cada um guarda o `NodeContext`
**por valor** e não faz nada além de ler o comportamento e preencher a decisão:

| nó | tipo BT | lê | escreve |
|---|---|---|---|
| `FuelLowCondition` (`FuelLow`) | `ConditionNode` | `snapshot().fuelFraction`, `getFuelReserve()`, porta XML `margin` | — |
| `ReturnToBaseAction` (`ReturnToBase`) | `SyncActionNode` | `snap.northM/eastM/headingDeg` → `rtbPlan()` | decisão `RTB` ou `HOME` |
| `ContactDetectedCondition` (`ContactDetected`) | `ConditionNode` | **só** `threatPolicy().engaged()` | — |
| `ReportAndEvadeAction` (`ReportAndEvade`) | `SyncActionNode` | `threatPolicy()`, `snap.contact*` | decisão `EVADE`/`BREAK` + pedido de alerta |
| `AlertReceivedCondition` (`AlertReceived`) | `ConditionNode` | **só** `snapshot().hasAlert` | — |
| `SupportAlertAction` (`SupportAlert`) | `SyncActionNode` | `snap.alert*`, `getSupportSpeedKts()` | decisão `SUPPORT` |
| `PatrolAction` (`Patrol`) | `SyncActionNode` | `patrolPlan()`, `getFrameDt()` | decisão `PATROL` |

Repare no `ContactDetected`: ele **não** pergunta "estou vendo o intruso agora?". Ele pergunta se a
manobra de evasão está valendo — que continua verdadeiro por `evadeHold` segundos depois de a pista
sumir. **A histerese é do modelo, não da árvore**: trocar o XML não a desliga.

**3. [`bt/bt_factory.hpp`](../../../../models/players/A-4/include/bt/bt_factory.hpp) +
`bt/bt_factory_sdk.hpp`** — registram os nós. O primeiro (`registerNodes()`) registra os sete que
`flight_tree.xml` usa, sem depender do SDK (`sdk_dep`) — compartilhado com `test-tree`, que não
linka MIXR. O segundo (`registerSdkNodes()`) registra os quatro que dependem do SDK
(`OnnxPolicy`/`OnnxScore`, via `libs/xinfer`; `PyDecide`, via `libs/xpyembed`; `Navigate`) —
separado por essa mesma razão de pureza. O ponto de extensão do BehaviorTree.CPP **v3** para
construtores com argumentos extras é `registerBuilder<T>(ID, builder)`, com um lambda capturando o
contexto:

```cpp
BT::NodeBuilder builder{
   [context](const std::string& name, const BT::NodeConfiguration& config) {
      return std::make_unique<NodeType>(name, config, context);
   }};
factory.registerBuilder<NodeType>(id, builder);
```

> A sobrecarga variádica de `registerNodeType` **só existe em versões posteriores**. Aqui é a v3.5.6.

### 7.6 Camada 6 — `xnative/factory.cpp`

Uma cadeia de `else if` sobre `getFactoryName()`, registrando as **nove** classes próprias listadas
na [seção 1](#1-o-que-vem-do-framework-e-o-que-é-nosso). Consumida não pela cadeia do host (ver
[§5.2](#52-factory-encadeada-por-nome--hoje-duas-cadeias-separadas)), mas por `libs/xplugin`, dentro
do `isValid()` do `( PluginLoader )` — antes de qualquer coisa escrita depois dele no `.edl`.

### 7.7 Camada 7 — `configs/` e `data/`

**[`configs/scenario.edl.in`](configs/scenario.edl.in)** (este cenário) — dissecado na
[seção 4](#4-a-árvore-de-objetos-do-cenário). É `.in` e não `.edl` por causa do `@NUM_TC_THREADS@`.
O `.edl` gerado é *gitignored*, escrito hoje em `build/generated-scenarios/`.

**[`models/players/A-4/configs/flight_tree.xml`](../../../../models/players/A-4/configs/flight_tree.xml)**
— a árvore, um `Fallback` de quatro ramos em ordem de prioridade:

```xml
<Fallback name="root">
  <Sequence name="rtb_sequence">     <FuelLow margin="0.05"/>  <ReturnToBase/>  </Sequence>
  <Sequence name="engage_sequence">  <ContactDetected/>        <ReportAndEvade/></Sequence>
  <Sequence name="support_sequence"> <AlertReceived/>          <SupportAlert/>  </Sequence>
  <Patrol/>
</Fallback>
```

Os itens 2 e 3 são os dois lados da interação entre players: quem viu transmite, quem recebeu reage.
**Nenhum nó fala diretamente com outro avião.** O arquivo também carrega um bloco
`<TreeNodesModel>` — necessário só para o Groot reconhecer os nós customizados; gerado por
`models/players/A-4/tools/dump_tree_model.cpp` (`make update-bt`), não mantido à mão.

**[`models/players/A-4/data/jsbsim/`](../../../../models/players/A-4/data/jsbsim/)** — a aeronave, o
A-4. É dado do **MODELO**, não do cenário: `domain::TerrainFloor`/`domain::ThreatPolicy` e as
velocidades comandadas (`patrolSpeed`/`rtbSpeed`/`evadeSpeed`/`supportSpeed`, ajustadas por
cenário, não por aeronave) são calibrados para esta aeronave especificamente — trocar de aeronave
sem recalibrar não faria sentido. `install_subdir()` publica em
`dist/share/mixr-plugins/flight/jsbsim/`, e é lá que o `rootDir:` do `( JSBSimModel )` deste cenário
(e do de [`../bandit/`](../bandit/), que pilota a mesma aeronave sem carregar o plugin nenhum)
aponta.

**`data/recordings/`, `data/logs/`, `data/messages/`** — saída de runtime: `mission.acmi`
(Tacview), `*.log` (`libs/xlog`), `mission.jsonl` (`libs/xmsg`), todos *gitignored*.

**`../../../shared/data/terrain/srtm/S23W043.hgt.gz`** — o tile, fora do subprojeto porque é
compartilhado com `../bandit/`.

---

## 8. A cadeia de decisão: UBF + BehaviorTree

O **UBF** (*Unified Behavior Framework*) define **três papéis** e nada mais:

| papel | interface do MIXR | nossa implementação |
|---|---|---|
| percepção | `base::ubf::AbstractState` | `xnative::FlightState` |
| decisão | `base::ubf::AbstractBehavior` | `xnative::BtBehavior` (e `AltitudeSafetyBehavior`/`RLBridgeBehavior`, exportadas mas não instanciadas por este cenário — ver [§1](#1-o-que-vem-do-framework-e-o-que-é-nosso)) |
| atuação | `base::ubf::AbstractAction` | `xnative::FlightAction` |

O que o UBF **não** diz é *como* decidir. É aí que entra a árvore: `BtBehavior` é um comportamento
do UBF cuja política interna é uma `BT::Tree` do BehaviorTree.CPP v3.

```
FlightAgentTC (nosso, componente do player, fase 3)
 └─ Agent::controller(dt)                          o ciclo, do framework
      ├─ FlightState::updateState(ator)            PERCEPÇÃO → WorldView (números crus)
      ├─ BtBehavior::genAction(state, dt)           DECISÃO — direto, sem árbitro no meio
      │    └─ WorldView → ThreatPolicy → tick da árvore
      │                    └─ FlightDecision (comando + rótulo)
      └─ FlightAction::execute(ator)                ATUAÇÃO → Autopilot + AlertDatalink
```

**Os dois encaixes são independentes, e isso é o ponto.** O UBF não sabe que existe uma árvore; a
árvore não sabe que existe um UBF. `BtBehavior` é o adaptador entre os dois, e é por isso que ele
concentra as três responsabilidades feias: construir a árvore (uma vez, sob mutex), traduzir o
`WorldView` para `domain::ThreatPolicy`, e empacotar a `FlightDecision` num `FlightAction` com o
voto certo (o `setVote(getVote())` continua existindo mesmo sem árbitro — é assim que o UBF espera
receber a ação de qualquer `AbstractBehavior`, arbitrado ou não).

Quatro lições de projeto que este modelo paga para aprender:

- **Uma regra dura não precisa virar ramo da árvore.** `AltitudeSafetyBehavior` existe justamente
  para vencer por **voto** sem que a árvore soubesse que ele existe — a composição continua válida
  como padrão de projeto, é o que [`src/rl`](../../../../src/rl/) usa de verdade hoje, só não é o
  que este cenário faz (ver [§1](#1-o-que-vem-do-framework-e-o-que-é-nosso)).
- **A histerese é do modelo, não da árvore.** `ContactDetected` consulta
  `domain::ThreatPolicy::engaged()`, não `snapshot().hasContact`.
- **Os nós são burros de propósito.** Nenhum nó toca em objeto MIXR; todos leem o `BtBehavior` (via
  `DecisionContext`) e preenchem a `FlightDecision`. O conjunto inteiro de nós é reutilizável fora
  de simulação.
- **O `dt` da árvore vem do UBF.** `getFrameDt()` devolve o `dt` que o `genAction()` recebeu — é ele
  que o `Patrol` integra. A árvore não tem relógio próprio.

### O ciclo-limite que ensinou tudo isso

> **Observado no Tacview: as aeronaves voavam "batendo asa".** Os ramos 2 e 3 comandam sentidos
> opostos sobre o **mesmo** objeto (fugir do intruso / ir até o ponto avisado). A quebra de 110°
> tira o intruso do setor do radar (`searchVolume` = ±30° em azimute), a pista some no mesmo
> instante, o ramo de apoio assume e traz a aeronave de volta — que reaquisita e quebra de novo.
>
> Duas correções, ambas em `domain/ThreatPolicy`:
>
> 1. **histerese** — `engaged()` vale por `evadeHold` (30 s) após perder o contato, então o ramo de
>    apoio não assume no piscar da pista;
> 2. **alvo fixado na entrada** — o comando era recalculado a cada tick como
>    `meu_rumo + breakTurn` (e `minha_altitude − evadeClimb`), um setpoint que fugia na mesma
>    velocidade em que a aeronave girava: a curva nunca terminava. Agora o alvo sai da **marcação
>    absoluta do contato**, calculado uma vez, e o piloto automático tem para onde convergir.
>
> Um rótulo separa os dois estados no status: `EVADE` (quebrando com a pista na tela) e `BREAK`
> (terminando a quebra no arrasto da histerese).

---

## 9. Interação entre players

Esta é a seção que responde "como dois players conversam" — não só neste cenário, mas no MIXR em
geral. A resposta cabe em uma frase:

> **Um player nunca chama método de outro player.** Ele entrega um `base::Object*` ao outro chamando
> `event()` nele — síncrono, dentro da própria pilha de chamada, **na thread de quem emitiu**.

Radar, datalink, colisão, IR, kill: **tudo** termina na mesma linha. O que muda entre os canais é só
(a) quem varreu a lista de players para achar o destinatário e (b) o que vai no payload.

### 9.1 A primitiva: `Component::event(token, Object*)`

```cpp
// base/Component.hpp -- a primitiva única de interação no MIXR inteiro
virtual bool event(const int event, base::Object* const obj = nullptr);
```

Não existe broker, fila global, pub/sub, nem roteamento declarativo no `.edl`. A prova mais curta é
o próprio datalink nativo sem rádio — literalmente um `for` na lista:

```cpp
base::PairStream* players{sim->getPlayers()};
while (playerItem != nullptr) {
   Player* player{static_cast<Player*>(playerPair->object())};
   if (player->isLocalPlayer()) {
      if ((player->isActive() || player->isMode(Player::PRE_RELEASE)) && player != getOwnship())
         player->event(DATALINK_MESSAGE, msg);   // Datalink.cpp  <- a interação inteira
      playerItem = playerItem->getNext();
   } else playerItem = nullptr;   // networked ficam no fim da lista; para aqui
}
```

`xnative::AlertDatalink::broadcastAlert()` faz **exatamente o mesmo laço**, uma segunda vez, com
`events::EID_ALERT` (ver [§7.3](#73-camada-3--xnative-as-classes-derivadas-do-mixr)). O radar faz
o mesmo, com outro token: `targets[i]->event(RF_EMISSION, em)` (`Antenna.cpp`).

A lista é sempre a mesma: `Simulation::getPlayers()`, devolve **pré-`ref()`'d** — quem chama tem que
`unref()`. Há três formas de achar alguém nela: `findPlayer(id)`, `findPlayerByName(nome)` e
`PairStream::findByName(nome)` sobre o retorno de `getPlayers()`.

Do lado do receptor, o despacho é a macro `base/macros.hpp` (`BEGIN_EVENT_HANDLER`): uma cadeia de
`if`s sobre `_used`, **o primeiro que casa vence** — e por isso `ON_EVENT_OBJ` vem sempre antes de
`ON_EVENT` para o mesmo token.

### 9.2 Por que não é ponteiro direto

Você *pode* pegar o ponteiro do outro player — `getPlayers()` é público, e o `Autopilot` nativo
guarda um (`const Player* lead`, **sem `ref()`**, para `followTheLeadMode`). O que você não pode é
**agir** através dele, e o motivo é a [anatomia do frame](#2-anatomia-de-um-frame).

A lista de players é varrida **4 vezes por frame**, uma por fase, e os players podem estar em
**threads diferentes** do pool T/C. Então, do seu componente:

- **ler** o outro player é corrida — ele pode estar no meio de `dynamics()` em outra thread, com a
  posição meio escrita;
- **escrever** nele é pior — você escreve fora da ordem determinística do frame, e o resultado passa
  a depender do escalonador.

`event()` sozinho **não** resolve a corrida: ele também roda na thread do emissor. O que ele dá é um
**ponto único de entrada**, que o receptor pode disciplinar. É exatamente o que o `AlertDatalink`
faz, e é o que sustenta o determinismo ([§12](#12-determinismo)):

| passo | onde roda | por quê |
|---|---|---|
| `onDatalinkMessageEvent()` só **encena** (`staged`), sob mutex curto | thread do **emissor** | seção crítica mínima; nenhuma decisão aqui |
| a fusão é **comutativa** (vence o mais próximo; empate → menor `senderId`) | idem | o resultado independe da **ordem de chegada**, que é do escalonador |
| `receive()` (fase 2) promove `staged → current` | thread do **receptor**, na fronteira de fase | latência **fixa** de 1 frame para todos |

Com ponteiro cru e escrita imediata, nenhuma das três propriedades existiria.

### 9.3 Canal 1 — RF: emissão, eco, pista

O canal mais elaborado, e o único em que o framework **modela física**. São **dois** percursos de
lista, em threads diferentes.

**Filtro (thread de fundo, fora do frame).** `RfSystem::updateData` pega `sim->getPlayers()` e desce
até `Tdb::processPlayers` — **o laço**. Ali são aplicados os filtros declarados no `.edl`:
`maxPlayersOfInterest`, `playerOfInterestTypes`, `maxRange2PlayersOfInterest`,
`maxAngle2PlayersOfInterest`, horizonte e oclusão de terreno (se `Gimbal.terrainOcculting: true` —
não usado por este cenário). Quem sobrevive vira `targets[]`.

**Ida, volta e detecção (dentro do frame).** A equação do radar vem partida em duas metades — ida na
fase 1, volta na fase 2:

| # | fase | o que acontece |
|---|---|---|
| 1 | 1 | `Radar::transmit()` monta a `Emission`; marca `setReturnRequest(...)` e `setTransmitter(this)` |
| 2 | 1 | `Antenna::rfTransmit()` recalcula a geometria **agora**, aplica ganho/ERP e pré-calcula `lossRng = 1/(4πr²)` |
| 3 | 1 | **`targets[i]->event(RF_EMISSION, em)`** — chega no outro player |
| 4 | 1 | o alvo calcula o próprio RCS: `signature->getRCS(em)`. **Sem `signature:` no `.edl`, vira 0** |
| 5 | 1 | o eco volta **direto pelo ponteiro** que veio no pacote — não passa pela lista de players |
| 6 | 1 | antena do emissor → sensor; sinal de **ida** = `power × rangeLoss × gain / losses`, enfileirado |
| 7 | 2 | `Radar::receive()` filtra o próprio eco (`em->getTransmitter() == this`), aplica `rcs × rangeLoss` de **volta** e testa o limiar S/I |
| 8 | 3 | `Radar::process()` correlaciona e — **só no fim da varredura** (`endOfScanFlg`) — chama `tm->newReport(...)` |
| 9 | 3 | `AirTrkMgr::processTrackList()` drena, correlaciona, roda alfa-beta, cria o `RfTrack` |
| 10 | — | você lê: `trkMgr->getTrackList(...)` — `libs/xtrack::nearestHostileTrack()` |

Duas consequências práticas para quem escreve o `.edl`: **`signature:` não é enfeite** — sem ele o
alvo devolve RCS 0 e o avião simplesmente não existe para o radar; e **a ordem dos componentes é
ordem de execução** — ver [seção 4](#4-a-árvore-de-objetos-do-cenário).

### 9.4 Canal 2 — datalink

`Datalink` estende `System` (não `Radio`). `sendMessage()` tem dois modos, mutuamente exclusivos:
com `radioName:` a mensagem vira payload de uma `Emission` e desce a cadeia RF inteira; sem rádio, o
`for` da lista de [§9.1](#91-a-primitiva-componenteventtoken-object). Este cenário não declara
`radioName:` em nenhum `AlertDatalink` — é broadcast global.

Do lado de quem recebe, são **três saltos**, todos do framework:

```
player->event(DATALINK_MESSAGE, msg)                    Player.cpp  (tabela de eventos)
   └─ Player::onDatalinkMessageEventPlayer()
        └─ getDatalink()->event(DATALINK_MESSAGE, msg)
             └─ Datalink::onDatalinkMessageEvent()      <- o nosso override
                  └─ BaseClass:: ... repassa aos subcomponentes
```

O `Player` acha o próprio datalink **por tipo**, em `updateSystemPointers()` — é por isso que uma
subclasse nossa declarada no slot `datalink:` é encontrada sozinha, sem uma linha de fiação.

> **Armadilha confirmada no fonte — o que `sendMessage()` NÃO faz.** Sem `radioName:`, ele **não
> filtra alcance nem lado**. O slot `maxRange` existe e o setter grava em `noRadioMaxRange`, mas
> `sendMessage()` nunca lê essa variável: a entrega é **broadcast global** para todo player local
> ativo — **inclusive o `bandit1`**, que é `red`. Neste cenário a mensagem morre lá porque o intruso
> não declara nenhum `datalink:` no seu `.edl` próprio (`../bandit/`), e
> `Player::onDatalinkMessageEventPlayer()` só repassa se `getDatalink() != nullptr`. Basta dar um
> datalink ao inimigo para ele passar a escutar a esquadrilha inteira.

### 9.5 Canal 3 — evento próprio

Para um canal que não é radar nem datalink, usa-se um token `>= USER_EVENTS` e chama-se
`event()`/`send()` direto — é exatamente como `events::EID_ALERT` foi construído
(`models/events/EventTokens.hpp`). Duas armadilhas, ambas já pagas neste modelo:

**(a) Eventos de aplicação NÃO sobem para o container.** `Component::event()` é escrito à mão, e não
com as macros, por causa exatamente do final:

```cpp
// *** Special handling of the end of the EVENT table ***
// Pass only key events up to our container
if (_event <= MAX_KEY_EVENT && container() != nullptr)   // MAX_KEY_EVENT == 999
    _used = container()->event(_event,_obj);
```

Um token de usuário **morre em silêncio** se ninguém tratar naquele nível exato. Só teclas sobem. Não
adianta "jogar na árvore" e esperar que alguém pegue: você tem que endereçar o destinatário.

**(b) `send(nome, ...)` resolve o nome entre os FILHOS de quem chama** — não em si mesmo, não na
árvore toda, com o ponteiro cacheado depois da primeira vez. A correção é chamar o `send()` **no
ownship**, ou no player achado por nome, e não no `this`.

### 9.6 Player que nasce em runtime

Interação também acontece com quem ainda não existia. No caso deste cenário isso acontece pelo
lado da rede: `interop::NetIO::createIPlayer()` clona por inteiro o `template:` do `DisNtm` de
entrada (`signature`, `dataLogTime`) a cada primeiro PDU do `bandit1`, e insere o clone na **mesma**
lista que `Simulation::getPlayers()` devolve — a mesma que o radar já varria. `addNewPlayer()` só
**enfileira**; quem materializa é `updatePlayerList()`, chamado de `Simulation::updateData()` —
**thread de fundo, não o frame**. Consequência: **um player criado só passa a existir para os
outros no próximo `updateData()`.**

### 9.7 O caminho desta poc, fim a fim

```
falcon3 detecta bandit1 (radar nativo -- 9.3)
   └─ árvore: ContactDetected → ReportAndEvade
        └─ FlightDecision.broadcastAlert = true (+ posição absoluta do contato)
             └─ FlightAction::execute()
                  └─ AlertDatalink::broadcastAlert()
                       ├─ Datalink::sendMessage(TacticalAlert*)     [caminho a -- NATIVO, §9.4]
                       └─ event(events::EID_ALERT, msg) direto      [caminho b -- ver §7.3]
                            └─ ... → player->event(DATALINK_MESSAGE, msg)
                                 └─ AlertDatalink::onDatalinkMessageEvent()   ← nosso gancho
                                      └─ ENCENA o alerta (fusão comutativa)
   ... fronteira de fase ...
   fase 2 do frame seguinte: AlertDatalink::receive() promove encenado → corrente
   fase 3 do mesmo frame:   FlightState vê hasAlert → árvore: AlertReceived → SupportAlert
```

Três decisões de modelagem que valem mais que o transporte:

1. **O handler roda na thread do EMISSOR.** Por isso o handler só *encena* o alerta, sob um mutex
   curto, e nunca mexe no estado corrente.
2. **A entrada não é fila FIFO.** Se dois aviões avisam no mesmo frame, a ordem de chegada depende
   do escalonador. A fusão é **comutativa** — vence o contato de menor distância; em empate exato, o
   emissor de menor id — então o resultado independe da ordem.
3. **A promoção acontece numa fronteira de fase.** O alerta encenado só passa a valer na fase 2 do
   frame seguinte, dando **latência fixa de um frame** para todos.

É a mesma disciplina que o framework usa entre as suas 4 fases: escreve numa fase, publica na
fronteira, lê na fase seguinte.

### 9.8 Receita: como escrever a sua

Escolha primeiro **de qual canal** você precisa:

| você quer… | use | custo em C++ |
|---|---|---|
| detecção física realista (RCS, potência, ruído, pistas filtradas) | RF nativo: `Antenna` + `Radar`/`Tws` + `AirTrkMgr` no `.edl` | **zero** |
| trocar uma **carga própria** entre players, sem modelar propagação | subclasse de `models::Datalink` | ~100 linhas — molde: [`AlertDatalink`](../../../../models/players/A-4/src/xnative/AlertDatalink.cpp) |
| um canal com regra própria, alcançando qualquer player | payload em `models/events/` + broadcast direto | molde: [`events::TacticalAlert`](../../../../models/events/payloads/EID_ALERT/TacticalAlert.hpp) |

O caminho do meio, em cinco passos:

1. **A carga** — um `base::Object` com `DECLARE_SUBCLASS`/`IMPLEMENT_SUBCLASS`.
2. **Emitir** — montar, `sendMessage(msg)`, `msg->unref()`. E, sem `NetIO` no cenário, forçar o
   caminho local no `reset()`:
   ```cpp
   setNetworkQueueEnabled(false);   // sem NetIO, ninguém drena a fila de rede
   setLocalSendEnabled(true);
   ```
3. **Receber** — sobrescrever `onDatalinkMessageEvent()` tratando como código que roda **na thread
   do outro**: `dynamic_cast` para a sua carga (pode chegar outro tipo), mutex **curto**, escrever
   num `staged` — nunca no estado que a decisão lê —, fusão **comutativa** se puderem chegar várias
   no mesmo frame, e terminar com `return BaseClass::onDatalinkMessageEvent(msg)` para não quebrar
   o repasse aos subcomponentes.
4. **Publicar numa fronteira de fase** — `receive(dt)` (fase 2) promove `staged → current` e
   envelhece com `holdTime`.
5. **Declarar** — `datalink: ( SuaClasse ... )` na lista de componentes do player (achado por tipo,
   automaticamente) e registrar a classe na factory do modelo.

E as regras que valem para os **três** canais: `event()` roda na **thread do emissor** (se o handler
escreve, ou é comutativo, ou é determinístico só por sorte); escreva numa fase, **publique na
fronteira**, leia na fase seguinte; ordem dos componentes no `.edl` é ordem de execução dentro da
fase, não decoração; nome errado em qualquer slot `*Name:` = componente inerte, **sem diagnóstico**.

---

## 10. Elevação de terreno

O banco de elevação é **100 % nativo**. `libmixr_terrain.so` já vinha linkado, o `WorldModel` já
tinha o slot `terrain`, e o `Player` já tinha `getTerrainElevationM()`/`getAltitudeAglM()`/
`updateElevation()`. **Nada foi escrito do lado do framework.**

### 10.1 As três coisas que faltavam

| # | o quê | onde |
|---|---|---|
| 1 | **a factory** — `models::factory` não encadeia a de terreno | `app/src/mixr_factory.cpp` (host, compartilhado por todas as pocs) |
| 2 | **o dado** — tile SRTM1 `S23W043` da Serra do Mar, descomprimido e conferido | `app/TerrainData` + `shared/data/terrain/srtm/` |
| 3 | **a ponte até a decisão** — campos no `WorldView`, uma regra pura, um consumidor | `FlightState`, `domain/TerrainFloor` |

O caminho completo do dado:

```
configs/scenario.edl.in     terrain: ( SrtmHgtFile path/file )
   └─ mixr::terrain::factory  constrói o SrtmHgtFile
        └─ WorldModel::setSlotTerrain()
             └─ RESET_EVENT → WorldModel::reset() → Terrain::reset() → loadData()
                                                     (25 MB, 3601×3601 posts de 2 bytes)
   ... a cada frame de BACKGROUND ...
   Player::updateData() → updateElevation() → terrain->getElevation(lat, lon, interp)
                                            → setTerrainElevation(el)  [tElev, tElevValid]
   ... a cada ciclo de decisão (fase 3) ...
   FlightState::updateState()  → snap.terrainElevM / altitudeAglM / terrainValid
        └─ BtBehavior::feedThreatPolicy()  → domain::GroundReference
             └─ domain::ThreatPolicy::breakCommand() → clampToTerrain(...)
                  (o único piso anti-CFIT vivo neste cenário -- só dentro do ramo de evasão)
```

### 10.2 O que a elevação faz com o comportamento

**Piso anti-CFIT da evasão.** "Desconflitar para baixo" só faz sentido enquanto houver espaço
embaixo. O alvo passa por `clampToTerrain()`: nunca abaixo de `elevação + terrainClearance`
(800 m em todo falcon deste cenário), com o piso absoluto (200 m) sobrando como rede. Como o alvo é
fixado **uma vez, na entrada da manobra**, o piso também é avaliado uma vez.

**Sem árbitro, esse é o ÚNICO piso que existe.** Este cenário não monta `( UbfArbiter )` (ver
[§1](#1-o-que-vem-do-framework-e-o-que-é-nosso)), então uma aeronave patrulhando ou retornando à
base sobre um relevo que sobe rápido não tem uma segunda camada checando isso — só o ramo de
evasão respeita `terrainClearance`. Quem quiser um piso independente de novo precisa reintroduzir o
árbitro no `behavior:` do agente — é exatamente o que [`src/rl`](../../../../src/rl/) faz, com
`( UbfArbiter behaviors: { ( AltitudeSafetyBehavior vote: 90 ... ) ( RLBridgeBehavior vote: 50 ) } )`.

### 10.3 Quem de fato limita a manobra

Ao contrário do que um `.edl` com `maxClimbRateMps` sugeriria, **este slot é decorativo para
`JSBSimModel`** (ver [§6.3](#63--autopilot---o-controle)) — a taxa de subida/descida real da
manobra de evasão vem do próprio `a4ap.xml` (SAS + canais de controle da aeronave), não de um
parâmetro ajustável no cenário. `evadeClimb` (700 m em `falcon1`/`falcon2`/`falcon4`, 750 m em
`falcon3`) é o deslocamento de altitude que `domain::ThreatPolicy` fixa na entrada da manobra —
quanto dele a aeronave de fato alcança dentro de `evadeHold` (30 s) depende da física do A-4, não de
um número no `.edl`. É por isso que a folga de `terrainClearance` (800 m) é uma margem de segurança
sobre a arimética de `clampToTerrain()`, não uma calibração fina contra uma taxa de descida
conhecida com precisão.

### 10.4 Armadilhas confirmadas no fonte

1. **`Player::updateElevation()` ignora o retorno de `getElevation()`.** Fora da célula do tile,
   `el` fica `0.0` e `setTerrainElevation(0.0)` liga `tElevValid = true`.
   **`isTerrainElevationValid()` não é guarda de cobertura.** Daí o piso absoluto em
   `domain/TerrainFloor.hpp` e a referência do cenário no miolo da célula (ver a nota em
   [§4](#4-a-árvore-de-objetos-do-cenário)).
2. **`getAltitudeAgl()` não consulta `tElevValid`**: sem banco carregado, AGL == altitude HAE,
   silenciosamente.
3. **Os slots são `path` e `file`** — não `pathname`/`filename`, que é como se chamam os setters.
   Nome errado dá `slot not found` e o tile não carrega.
4. **`SrtmHgtFile` não lê `.gz`** e valida o **tamanho exato em bytes** (2 884 802 = SRTM3,
   25 934 402 = SRTM1). Qualquer outro tamanho falha com *"ERROR in determining SRTM type"*, sem
   dizer qual arquivo. O nome é lido por **posição fixa nos últimos 11 caracteres**.
5. **`CRASH_EVENT` deixa de ser letra morta.** `Player.cpp` dispara com `AGL < 0`, e
   `crashNotification()` faz `setMode(CRASHED)` e manda `KILL_EVENT` a todos os subcomponentes — o
   avião **congela e para de decidir**, porque `updateTC`/`updateData` só rodam com `mode ==
   ACTIVE`. É a razão de cada falcon voar no pico do próprio circuito + 300 m (ver
   [§4](#4-a-árvore-de-objetos-do-cenário)). Escape hatch, se precisar: `crashOverride: true`
   (slot nativo 26).
6. **`terrainElevReq` tem de continuar `false`** (default deste cenário). Com `true`,
   `updateElevation()` **pula** a consulta ao banco e fica esperando um gerador de imagem externo
   empurrar o valor.
7. `getMinElevation()`/`getMaxElevation()` do `SrtmHgtFile` estão **errados** — refletem só a última
   linha lida.
8. `WorldModel::reset()` imprime `"Loading Terrain Data..."` em `stdout`, incondicionalmente.
   Inofensivo: os scripts de verificação filtram por `grep '^frame='`.
9. **`enabledList: [ 43 42 ]`** do `dataRecorder` não pode ganhar mais tokens sem cuidado —
   `crashNotification()` grava `REID_PLAYER_CRASH`; mantê-lo fora da lista mantém o handler nativo
   (da mesma família que estoura, ver [§13](#13-armadilhas-encontradas-rodando)) fora do caminho.

---

## 11. Tacview

A exportação é a de [`libs/xtacview`](../../../../libs/xtacview/), ligada na cadeia nativa do
`dataRecorder` — neste cenário, `( ExposedDataRecorder )` (ver [§5.5](#55-datarecorder-com-cadeia-de-outputhandler--e-uma-variante-própria-do-gravador)).
Nenhum código de stream no `main.cpp`. Cada player precisa de `dataLogTime: ( Seconds 0.1 )` — o
slot nasce zero e, sem ele, o player nunca aparece.

Semântica ACMI (é onde quase todo mundo erra):

| campo | conteúdo | slot |
|---|---|---|
| `Name` | **modelo** em notação ICAO/OTAN — é por ele que o Tacview acha a aeronave na base e escolhe o ícone/modelo 3D | `modelMap` |
| `Type` | taxonomia (`Air+FixedWing`) | `typeMap` |
| `CallSign` / `Pilot` | **nome do player** — é o que aparece no rótulo | automático |
| `Color` | lado | `colorMap` |

Este cenário declara `modelMap: { falcon1: "A-4E" ... bandit1: "A-4E" }` — o mesmo modelo para os
cinco, porque hoje a dinâmica **é** a de um A-4 de verdade, não mais uma escolha de apresentação por
cima de outra aeronave (a dinâmica é o próprio JSBSim `A4`):

```
65,T=...,Name=A-4E,Type=Air+FixedWing,Color=Blue,CallSign=falcon1,Pilot=falcon1
```

**Posição:** o registro do gravador carrega ECEF, convertido com `base::nav::convertEcef2Geod()`
antes de virar linha ACMI. A altitude no `.acmi` é **MSL/HAE — não há referência de solo no
stream**, mesmo com terreno carregado.

**O intruso aparece no Tacview de dois lados.** `bandit1` chega por rede (`inputEntityTypes:`) e
entra em `modelMap`/`typeMap`/`colorMap` deste cenário como `red`; e as quatro falcons chegam no
Tacview de `../bandit/` (porta 1235) pelo caminho inverso, `outputEntityTypes:` — ver
[`../README.md`](../README.md).

---

## 12. Determinismo

**Determinístico** aqui quer dizer: rodar o mesmo cenário duas vezes e obter o mesmo estado, no
mesmo frame, até o último decimal. É exatamente o que
`./tests/determinism/check_determinism.sh` compara.

### 12.1 O paralelismo do pool T/C não quebra nada

O paralelismo do MIXR é de fork/join com **barreira em cada fase**, não uma corrida livre:

```
para cada uma das 4 fases do frame:
     fatia a lista de players entre as N threads      ← fork
     cada thread roda SÓ a fase corrente dos SEUS players
     SyncThread::waitForAllCompleted()                ← join, barreira
```

Duas consequências que juntas dão o determinismo: **dentro de uma fase, os players são
independentes** (cada thread mexe nos seus próprios players; nada que uma escreve é lido por outra
na mesma fase), e **a barreira fecha a fase para todos ao mesmo tempo** (nenhum player pode
"adiantar" a fase 2 enquanto outro ainda está na fase 1).

Do nosso lado, quatro decisões conscientes fecham as brechas que sobrariam:

- **Passo fixo** — `dt = 1/rate` calculado uma vez, o mesmo valor todo frame; o relógio de parede
  decide apenas *quando* o frame roda, nunca *quanto* ele avança.
- **Fusão comutativa dos alertas** (ver [§9.7](#97-o-caminho-desta-poc-fim-a-fim)).
- **Escolha da pista sem depender da lista** — menor distância e, em empate exato, menor id de pista
  (`libs/xtrack::TrackQuery`).
- **A consulta de elevação é uma leitura de um banco imutável** depois de carregado, feita sempre no
  mesmo ponto do passo — não introduz dependência de ordem.

Nada neste cenário usa relógio, sorteio ou identidade de thread para decidir — inclusive o jitter da
patrulha ([§7.1](#71-camada-1--domain-as-regras-puras)), que é resorteado por **troca de perna**
(evento determinístico), nunca por `dt`.

### 12.2 A decisão dentro do frame é a diferença estrutural

Com `FlightAgentTC` decidindo na fase 3, a decisão é **parte do frame**. A fase 3 já é um trecho
ordenado e com barreira: nenhum player entra nela antes que todos tenham terminado a fase 2, e
nenhum sai do frame antes que todos tenham terminado a fase 3. Isso remove, **por construção**, os
três defeitos que um agente decidindo fora do frame (num laço de background com relógio próprio)
teria: ponto de amostragem arbitrário, número variável de decisões por segundo simulado, e leitura
do `Player` concorrente com a escrita da física. Nenhum artifício de laço precisa colapsar duas
threads para obter isso — a propriedade é do **modelo**, não de quem o chama.

### 12.3 O modo `-deterministic`, e por que ele continua existindo

`-deterministic N` não sobe a thread periódica de tempo crítico nem o laço de tempo real: a mesma
thread chama `station->tcFrame(dt)` e depois `station->updateData(dt)`, em ordem fixa, `N` vezes.
Isso não é mais o que **produz** o determinismo (que já é propriedade da fase 3) — é o que faz o
resultado **reproduzível bit a bit entre execuções**, sem depender de pacing de relógio de parede,
e é o formato que os scripts de verificação comparam.

> **Armadilha, encontrada rodando: `-deterministic` não é hermético com o cenário de produção.** O
> bloco `networks:` abre a porta DIS 3000 e ingere PDUs de quem estiver na rede — com um `bandit`
> de outra sessão no ar, duas execuções idênticas divergem e o script de verificação acusaria falso
> não-determinismo. Por isso `check_determinism.sh` deriva uma **fixture hermética** (sem
> `networks:`) do próprio `configs/scenario.edl.in`, via `tests/scenario/make_fixture.py --poc
> flight`, em vez de rodar contra o `.edl.in` de produção direto.

### 12.4 O que o `check` prova (e o que não prova)

`./tests/determinism/check_determinism.sh ./build/app/src/app flight 2000 flight` roda 2000 frames
com **1, 2 e 4 threads T/C** (mais uma repetição de 4) e compara os dumps.

- **Prova**: que o paralelismo do pool T/C e as regras de fusão/desempate não introduzem
  dependência de ordem — trocar 1 por 4 threads não muda um decimal. E que a decisão está **amarrada
  ao frame**: a contagem de decisões (`dec=`, publicada por `xboard`) avança na mesma taxa que
  `frame` entre dumps consecutivos, em qualquer configuração de threads — nem duas decisões no
  mesmo frame, nem frame sem decisão. **Não** é `dec == frame`: há um *offset* de partida (o frame
  de aquecimento que a `Station` dispara logo após o `RESET_EVENT` já roda a fase 3 uma vez antes do
  laço de frames começar) — o que se compara é a **taxa**, não o valor absoluto.
- **Não prova**: que o modo de tempo real é reprodutível **bit a bit** contra outra execução — ele
  não precisa ser, porque a propriedade que importa (mesma sequência de decisões, mesmo resultado
  físico) já é garantida pela fase 3, não pelo pacing de relógio de parede.
- **Não prova**, e esta é a lacuna maior: que o modelo decide **certo**. Reprodutibilidade não é
  correção — um modelo que erra sempre igual passa aqui sem reclamar. É o que as camadas `domain`,
  `tree` (do modelo) e `scenario` (do host) fecham — ver [`tests/README.md`](../../../../tests/README.md).

### 12.5 Os contadores de instância não são atômicos

`mixr::base::MetaObject::count/mc/tc` são mantidos por `int` cru
(`STANDARD_CONSTRUCTOR`/`STANDARD_DESTRUCTOR`), sem lock. Com os quatro agentes decidindo em
paralelo no pool T/C, os incrementos de `TacticalAlert`/`FlightAction` (criados a cada decisão)
correm entre si — um teste de vazamento sobre esses contadores tem de rodar com `-threads 1`, e um
número negativo transitório numa contagem "ao vivo" (ex.: na aba Memória do `./app`) é a assinatura
dessa corrida, **não** de um vazamento (que seria determinístico e nunca daria um valor impossível).

---

## 13. Armadilhas encontradas rodando

Tudo abaixo foi medido com o binário ou confirmado no fonte, não deduzido. As específicas de
terreno estão na [seção 10.4](#104-armadilhas-confirmadas-no-fonte); as do próprio `FlightAgentTC`,
na [seção 6.6](#66--flightagenttc---o-agente).

### 13.1 `models::JSBSimModel` é `final`

Não dá para estender para corrigir nada. Ou se usa como está, ou se escreve outro adaptador falando
direto com a `JSBSim::FGFDMExec`.

### 13.2 `setCommandedAltitude()` e companhia ignoram os limites de taxa do Autopilot com `JSBSimModel`

Já discutido em [§6.3](#63--autopilot---o-controle)/[§10.3](#103-quem-de-fato-limita-a-manobra):
`maxRateOfTurnDps`/`maxBankAngle`/`maxPitchAngle`/`maxClimbRateMps`/`maxAcceleration` são
declarados no `.edl` por documentação/coerência, mas `JSBSimModel::setCommandedHeadingD()`/
`setCommandedAltitude()`/`setCommandedVelocityKts()` ignoram o parâmetro de taxa. Quem de fato
limita a manobra é o `<autopilot>` da própria aeronave (`a4ap.xml`).

### 13.3 O autopilot do A-4 não fecha malha de velocidade sozinho

O A-4 de fábrica (Aeromatic) **declara** `ap/airspeed_hold`/`ap/airspeed_setpoint`, mas não vem com
um canal que os implemente — o MIXR já escreve `ap/airspeed_setpoint`; faltava algo do lado JSBSim
para lê-lo e agir. `a4ap.xml` ganhou um canal `AP Autothrottle` (PID → manete) — motor **único**
(J52), diferente de um bimotor como o c310, que precisaria espelhar o comando para os dois motores.
Sem esse canal, a velocidade nunca convergiria para o valor comandado por conta própria.

### 13.4 O `Agent` não propaga o ciclo de componentes

Nem `updateData()` nem `reset()` chegam ao `state`/`behavior` de um `Agent` — vale tanto para o
`BtBehavior` **direto** (a configuração deste cenário) quanto para um comportamento aninhado mais
fundo dentro de um `UbfArbiter` (dois níveis abaixo do `Agent`; `src/rl` usa essa forma). Sintoma
medido: os planos de voo ficariam com os **defaults** de `domain::PatrolPlan`/`domain::RtbPlan` em
vez dos valores dos slots, sem erro nenhum. Por isso `BtBehavior` configura os planos
**preguiçosamente**, no primeiro `genAction()` (ver [§7.4](#74-camada-4--ubf-percepção-decisão-atuação)).

### 13.5 `BT::BehaviorTreeFactory::createTreeFromFile()` não é reentrante

Os quatro aviões chegam ao primeiro `genAction()` ao mesmo tempo, em threads T/C diferentes.
`BtBehavior.cpp` protege a construção com um mutex de arquivo — cada avião ainda tem sua própria
`BT::Tree`, só a construção é serializada.

### 13.6 O gravador nativo **segfalta** ao gravar pista nova ou arma liberada

Assim que o radar cria a primeira pista (`REID_NEW_TRACK`) ou uma arma é liberada
(`REID_WEAPON_RELEASED`), o `DataRecorder` nativo estoura dentro de `recordNewTrack()`/handler
equivalente, via `__dynamic_cast`. O contorno é o mesmo dos dois casos: habilitar apenas os tokens
que o `TacviewOutput` de fato usa, já que `isDataEnabled()` é testado **antes** de chamar o handler
quebrado:

```
enabledList: [ 43 42 ]      // 43 = REID_PLAYER_DATA, 42 = REID_PLAYER_REMOVED
```

### 13.7 O handler default de `DATALINK_MESSAGE` não enfileira nada

Duas tentativas erradas antes de achar o gancho certo: drenar `receiveMessage()` na fase 2 (zero
alertas, mesmo com centenas de transmissões) e sobrescrever `queueIncomingMessage()` (zero
chamadas). O cabeçalho do `Datalink` já avisava: o handler nativo *"passa as mensagens aos
subcomponentes"*. A `inQueue` é do caminho de **rádio/rede**, não da entrega local. O gancho correto
é **`onDatalinkMessageEvent()`**.

---

## 14. Controle de tempo — acelerar, frear, pausar

O cenário declara **`( ClockStation )` no lugar de `( Station )`** — uma `simulation::Station` com
um único *override*, vinda de [`libs/xclock`](../../../../libs/xclock/). Trocar de volta para
`( Station )` continua rodando, só sem as teclas.

Teclas (no `./app`): `+`/`=` acelera, `-`/`_` freia, `espaço`/`p` pausa, `1` volta a tempo real,
`h` ajuda. Escala em degraus `0.10x … 64x`.

**A divisão entre nativo e próprio é deliberada:**

- **Acelerar é 100 % nativo.** `Station::processTimeCriticalTasks()` já faz
  `for (jj=0; jj < getFastForwardRate(); jj++) tcFrame(dt)`, e `setFastForwardRate()` é público e
  virtual. Nada foi escrito para isso.
- **Frear não existe no framework** — `fastForwardRate` é `unsigned int` (só multiplica) e não há
  setter público de `tcRate` em runtime. É a **única** coisa acrescentada: abaixo de `1x`, um
  `tcFrame(dt * fator)` com o `dt` encurtado. Passo de integração menor, nunca maior.
- **Pausar é nativo, por um caminho não óbvio.** Não existe `Simulation::pause()`; o que existe é o
  flag de freeze do `base::Component`, e **ele não se propaga para os filhos** — a cascata é por
  *consulta*, no sentido inverso: `Player::isFrozen()` testa o próprio flag **ou** o da simulação,
  `System::isFrozen()` testa o próprio **ou** o do ownship, e `Player::dynamics()` repassa ao
  `DynamicsModel`, que põe a JSBSim em hold. Por isso `setPaused()` age em `getSimulation()`,
  **não** na `Station`.

> **Armadilha confirmada rodando:** marcar o freeze **não para o relógio de execução**.
> `Simulation::updateTC()` faz `execTime += dt` **antes** do teste de freeze, e com o `dt` cru.
> Medido: mundo parado, `sim=` ainda subindo — vazaria para o Tacview, que data cada linha ACMI com
> `exec_time`. Correção: quando pausado, **não chamar `tcFrame()`**. O flag continua marcado porque
> é ele que congela o *outro* caminho, o de background.

**A decisão para junto com a simulação, porque ela mora dentro do frame.** Ao contrário de um
agente hipotético decidindo em `updateData()` (que continuaria avaliando sobre um mundo estático
enquanto pausado, porque `ubf::Agent::updateData()` não consulta `isFrozen()`), `FlightAgentTC`
decide na fase 3 do `tcFrame()` — pausar a simulação já para a produção de decisões junto, sem
nenhum código extra.

`-deterministic` **não é afetado**: chama `station->tcFrame(dt)` direto, sem passar por
`processTimeCriticalTasks()`.

Sem TTY (pipe, redirecionamento, CI) o `tcgetattr()` de `ConsoleKeyboard` falha, `isActive()` fica
`false` e a simulação roda normalmente, só sem teclado.

---

## 15. O que foi medido rodando

### 15.1 As quatro decisões acontecem em quatro threads diferentes

Evidência de uma execução real deste cenário (`data/logs/flight_*.log`, gerado por
`libs/xlog` — uma cópia por processo, alimentada pelo `LOG(...)` de `FlightAction::execute()`
dentro do plugin, sem nenhuma ponte): os batimentos periódicos (a cada 500 decisões, ver
[§7.4](#74-camada-4--ubf-percepção-decisão-atuação)) mostram os quatro falcons em **quatro** threads
distintas do pool T/C, estáveis pela duração inteira da captura:

```
[FlightAction] falcon1: 500 decisoes atuadas, em 'PATROL' (thread 1)
[FlightAction] falcon2: 500 decisoes atuadas, em 'PATROL' (thread 3)
[FlightAction] falcon3: 500 decisoes atuadas, em 'PATROL' (thread 0)
[FlightAction] falcon4: 500 decisoes atuadas, em 'PATROL' (thread 2)
```

`threadTag()` é `thread_local` — a mesma thread física sempre reporta o mesmo número (ver
[§7.2](#72-camada-2--xnative-o-utilitário-de-runtime)), então essa distribuição por si só já é a
prova de que os quatro agentes decidem em paralelo, não em sequência numa thread só.

### 15.2 O batimento prova cadência sem custar um contador novo

Cada linha de batimento sai a cada 500 decisões **atuadas** — não a cada N segundos de parede — e a
cadência acompanha a taxa de decisão real do player, não um relógio próprio do log. `dec=`/`thr=`
(no dump e no status) vêm do mesmo `xboard::Readout` que essas linhas leem, então as duas fontes
(log e dump) nunca podem discordar sobre quantas decisões um player já tomou.

### 15.3 A primeira decisão de cada aeronave é o valor inicial do `.edl`

Toda execução deste cenário começa com as quatro transições `-- -> PATROL`, mostrando exatamente o
rumo/altitude/velocidade iniciais declarados (`falcon1: hdg=87.7deg alt=1750m vel=350kt`, e assim
por diante) — o `xboard::Readout` default é `label="--"`, e a primeira decisão de cada falcon é a
primeira linha a trocar esse valor. É a confirmação mais direta de que a leitura do `.edl` chegou
intacta até a política.

---

## 16. Como verificar tudo

```bash
# build + execução. 'make build' ENCADEIA as três etapas:
#   sdk (dist/include+lib) -> models (o plugin) -> host (o executável)
make configure && make build && ./build/app/src/app -f src/poc/dis/flight/configs/scenario.edl.in

# as DUAS suítes (a do modelo e a do host)
meson configure build -Dtests=true && make build && make test-models && make test

# determinismo isolado: 1, 2 e 4 threads produzem o mesmo estado (fixture hermética, derivada)
./tests/determinism/check_determinism.sh ./build/app/src/app flight 2000 flight

# vazamento: LeakSanitizer, com as supressões dos vazamentos conhecidos do framework
make test-asan

# o terreno chegou? elev= e agl= têm de ser plausíveis e NÃO-ZERO
./build/app/src/app -f src/poc/dis/flight/configs/scenario.edl.in -deterministic 300 | grep "^frame=300 "

# o piso de terreno está vivo? (controle negativo, sem recompilar)
sed 's/terrainClearance: ( Meters 800 )/terrainClearance: ( Meters 0 )/' \
    src/poc/dis/flight/configs/scenario.edl.in > /tmp/sem-piso.edl.in
./build/app/src/app -f src/poc/dis/flight/configs/scenario.edl.in -deterministic 12000 | grep '^frame=' > /tmp/com.txt
./build/app/src/app -f /tmp/sem-piso.edl.in -deterministic 12000 | grep '^frame=' > /tmp/sem.txt
diff -q /tmp/com.txt /tmp/sem.txt      # DEVEM diferir

# o que o replay recebeu
grep -o "Name=[^,]*,Type=[^,]*,Color=[^,]*,CallSign=[^,]*" \
     src/poc/dis/flight/data/recordings/mission.acmi | sort -u

# a decisão está mesmo amarrada ao frame? dec avança na mesma taxa que frame
./build/app/src/app -f src/poc/dis/flight/configs/scenario.edl.in -threads 1 -deterministic 500 \
  | grep 'frame=500 player=falcon1' | grep -o 'dec=[0-9]*'

# vazamento sem ferramenta externa: os contadores de instância do próprio MIXR (rodar com -threads 1)
./build/app/src/app -f src/poc/dis/flight/configs/scenario.edl.in -threads 1 -deterministic 1000 | grep "^meta="
```

> **Atenção ao rodar os comandos acima com `-deterministic` e o cenário de produção:** o bloco
> `networks:` faz o processo ingerir PDUs DIS de quem estiver na rede, então uma instância de
> [`../bandit/`](../bandit/) no ar muda o resultado. Os testes de
> [`tests/`](../../../../tests/README.md) contornam isso gerando fixtures sem o bloco de rede.

O status impresso a cada 2 s traz, por aeronave: altitude, **elevação do terreno**, **AGL**, rumo,
banco, velocidade, empuxo, mach, G, combustível, o rótulo do comportamento vencedor, o que o
`Autopilot` está comandando, a pista mais próxima (já filtrada por lado) e o alerta recebido:

```
falcon2  alt= 2022m elev= 906m agl= 1117m hdg= 51deg roll=-30deg spd=141kt ...
         bt=EVADE    ap(hdg=132,alt=5595ft,spd=185) pista=bandit1@12.5NM dec=1204 thr=2
```

---

## 17. O que a poc responde

- **O framework dirige pela herança e encontra pela árvore de componentes**: o `.edl` diz *onde* o
  objeto está, a classe-base diz *quando* ele é chamado, e o `virtual` diz *o que* roda.
- **Herdar tudo o que dá deixa o modelo com nove classes próprias** — nenhuma delas player,
  dinâmica, controle ou sensor — e traz junto coisas que ninguém escreve por gosto: equação do
  radar, correlação de pistas, transporte de datalink, banco de elevação, e um pool de threads de
  tempo crítico de verdade.
- **Mover a decisão para dentro do frame custa ~50 linhas de C++ e três armadilhas do framework**
  (`AgentTC` não registrado por nenhuma factory, `updateTC()` chamando `controller()` em toda fase,
  `Agent::updateData()` decidindo de novo em background) — e o que se compra com isso é
  determinismo em **tempo real**, não só em passo fixo: a decisão é parte da mesma barreira de fase
  que já protege a física.
- **O preço aparece nas bordas**: a aeronave tem que ser uma que o autopilot nativo consiga
  comandar (e mesmo assim, os limites de taxa do `.edl` são decorativos para `JSBSimModel` — quem
  limita de verdade é o `<autopilot>` da própria aeronave), a classe da dinâmica é `final`, o radar
  não filtra lado, o datalink não filtra alcance, a consulta de terreno mente quando está fora da
  célula, e não sobra lugar natural para guardar o estado que é da aplicação (daí `libs/xboard`).
- **O que é decisão continua sendo nosso**: percepção, política, atuação, e as regras de
  fusão/determinismo que fazem quatro agentes decidindo em paralelo, em threads diferentes,
  produzirem o mesmo resultado byte a byte. O UBF define os papéis; ele não os preenche.
- **E o mundo físico impõe limites que a política não pode ignorar**: o piso anti-CFIT é correto
  desde a primeira versão, mas só vira comportamento observável quando a manobra de fato alcança o
  chão — e sem um número confiável de "quanto a aeronave desce por segundo" (porque esse número não
  é mais um slot do `.edl`, é uma propriedade da física do A-4), medir isso exige rodar, não ler o
  cenário.
