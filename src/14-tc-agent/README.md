# 14-tc-agent

A [poc/13](../13-native-stack/) **inteira**, com **uma** diferença: o agente do UBF é um
`AgentTC` próprio, que decide na **fase 3 do frame de tempo crítico**, no lugar do `( SimAgent )`
nativo, que decide em `updateData()` — na thread de background.

```bash
make build
make run-tc-agent          # Tacview Real-Time Telemetry na porta 1234; Ctrl+C encerra
make check-tc-agent        # verifica o determinismo (1, 2 e 4 threads T/C)
make compare-13-14         # lista o que difere entre os dois subprojetos
```

> **Rode sempre a partir da raiz do repositório**: cenário, dados do JSBSim e gravação `.acmi`
> são resolvidos por caminho relativo (`./src/14-tc-agent/...`).

**Por que ela existe.** A seção 8 do README da poc/13 termina numa ressalva: lá o determinismo é
propriedade do *harness* (o laço de `app/DeterministicRun.cpp` serializando `tcFrame()` e
`updateData()`), não do
modelo. Esta poc paga o preço de escrever o agente para que passe a ser propriedade do **modelo** —
e mede a diferença em vez de argumentar sobre ela.

---

## Índice

1. [A diferença, em uma tela](#1-a-diferença-em-uma-tela)
2. [A classe `FlightAgentTC`](#2-a-classe-flightagenttc)
3. [Onde a decisão entra no frame](#3-onde-a-decisão-entra-no-frame)
4. [O que muda no `.epp`](#4-o-que-muda-no-epp)
5. [O que muda na aplicação (`main.cpp` + `app/`)](#5-o-que-muda-na-aplicação-maincpp--app)
6. [Determinismo — o ponto da poc](#6-determinismo--o-ponto-da-poc)
7. [Interação entre players](#7-interação-entre-players)
8. [O que foi medido rodando](#8-o-que-foi-medido-rodando)
9. [Qual das duas usar](#9-qual-das-duas-usar)
10. [Como verificar tudo](#10-como-verificar-tudo)
11. [Controle de tempo — acelerar, frear, pausar](#11-controle-de-tempo--acelerar-frear-pausar)

---

## 1. A diferença, em uma tela

| peça | poc/13 | poc/14 |
|---|---|---|
| player / 6-DOF / controle / radar / datalink | `Aircraft` + `JSBSimModel` + `Autopilot` + `Gimbal/Antenna/Tws/AirTrkMgr` + `AlertDatalink` | **idêntico** |
| percepção / decisão / atuação | `FlightState` / `BtBehavior` + `AltitudeSafetyBehavior` / `FlightAction` | **idêntico** |
| árbitro | `( UbfArbiter )` nativo | **idêntico** |
| árvore de comportamento | `configs/flight_tree.xml` | **idêntico** |
| **agente** | **`( SimAgent )`** — nativo, componente da **Station**, ator por nome | **`( FlightAgentTC )`** — nosso, componente do **player**, ator = container |
| **quando decide** | `updateData()` → thread de **background**, na taxa do laço do `main` | `updateTC()` fase 3 → thread de **tempo crítico**, todo frame |
| **de onde o alerta é emitido** | do background: os 4 emissores rodam **em sequência**, numa thread só | da fase 3: os 4 rodam **em paralelo**, um por thread do pool — ver [7.7](#77-o-caminho-desta-poc-fim-a-fim--e-o-que-a-poc14-muda-nele) |

`make compare-13-14` mostra o tamanho real da mudança:

```
Only in src/14-tc-agent/include/xnative: FlightAgentTC.hpp     ← a classe nova (87 linhas, 24 de código)
Only in src/14-tc-agent/src/xnative:     FlightAgentTC.cpp     ← 92 linhas, 43 de código
Files .../src/xnative/factory.cpp differ                       ← +1 linha: registra a classe
Files .../src/meson.build differ                               ← +1 linha: compila o .cpp
Files .../configs/scenario.epp.in differ                       ← os 4 agentes mudam de lugar e de classe
Files .../src/app/StatusReport.cpp differ                      ← +1 bloco: os campos 'dec'/'thr'
Files .../src/app/DeterministicDump.cpp differ                 ← +1 campo: 'dec'
Files .../src/main.cpp differ                                  ← banner e comentários de cabeçalho
Files .../README.md differ                                     ← este arquivo
```

Nenhum arquivo de `domain/`, `ubf/`, `bt/` ou dos outros modelos de `xnative/` foi tocado, e do
lado da aplicação só mudou quem *imprime* os dois contadores novos — e é essa a demonstração: **onde a decisão roda é uma escolha de integração, não do modelo**.

> **Sobre o namespace:** continua `mixr::xnative`, igual ao da poc/13, de propósito — é o que
> permite que `diff -r` entre os dois subprojetos mostre exatamente a diferença que a poc quer
> discutir, sem ruído de renomeação. São executáveis separados, então não há conflito.

---

## 2. A classe `FlightAgentTC`

[`include/xnative/FlightAgentTC.hpp`](include/xnative/FlightAgentTC.hpp) ·
[`src/xnative/FlightAgentTC.cpp`](src/xnative/FlightAgentTC.cpp) — 43 linhas de código, e cada
bloco delas existe por causa de uma armadilha do framework.

### 2.1 `UbfAgentTC` existe, mas nenhuma factory do MIXR o constrói

`base::ubf::AgentTC` está lá, pronto, no mesmo header do `Agent`. Mas `base/factory.cpp` registra
apenas `"UbfAgent"` e `"UbfArbiter"` — escrever `( UbfAgentTC ... )` no EDL não constrói nada.
**Um agente de tempo crítico é, na prática, código da aplicação**: a classe é do framework, o
registro é seu.

```cpp
// src/xnative/factory.cpp
else if ( name == FlightAgentTC::getFactoryName() )  obj = new FlightAgentTC();
```

### 2.2 `AgentTC::updateTC()` chama `controller()` em **toda** fase

O agente é um `base::Component` dentro do player, e a lista de players é percorrida **4× por
frame** — uma vez por fase, com `dt/4`. `AgentTC::updateTC()` não filtra nada:

```cpp
void AgentTC::updateTC(const double dt) { controller(dt); }   // framework
```

Sem filtro, a decisão rodaria **4 vezes por frame**, três delas nas fases erradas (dinâmica,
transmissão, recepção). O filtro é o mesmo que `models::System` aplica às suas quatro fases:

```cpp
if (world->phase() != 3) return;      // a decisão pertence à fase "lógica e controle"
BaseClass::controller(dt * 4.0);      // e com o dt do frame INTEIRO, não dt/4
```

### 2.3 `Agent::updateData()` **também** chama `controller()`

Esta não estava documentada em lugar nenhum e é a que morde: `AgentTC` **acrescenta** `updateTC()`
mas **não desliga** o caminho de background herdado de `Agent`:

```cpp
void Agent::updateData(const double dt) { controller(dt); }   // framework — continua valendo
```

E `Player::updateData()` propaga para a lista de componentes (`Player.cpp:636` →
`Component::updateData()`). Ou seja: um agente dentro do player decidiria **duas vezes por
frame** — uma no tempo crítico e outra no background. E o filtro da fase **não** salva: ao fim do
`tcFrame()` a fase corrente **fica em 3**, então a chamada de background passa direto pelo `if`.

Por isso:

```cpp
void FlightAgentTC::updateData(const double) { }   // no-op deliberado
```

Não se está suprimindo trabalho: o `Agent` nativo **não** repassa `updateData()` ao `state`/
`behavior` (é a armadilha 10.7 do README da poc/13), então não há nada a propagar.

### 2.4 O ator vem do container

```cpp
void FlightAgentTC::initActor()
{
   if (getActor() != nullptr) return;
   const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
   if (player != nullptr) setActor(player);
}
```

O `SimAgent` faz o contrário: mora na `Station` e procura o player **por nome** no `WorldModel`
(slot `actorPlayerName`). Consequência prática no EDL: aqui o bloco do agente é **igual** para as
quatro aeronaves — não há nome a amarrar, e copiar um player não deixa um agente órfão apontando
para o nome errado.

---

## 3. Onde a decisão entra no frame

```
thread de tempo crítico (dt = 1/tcRate = 0.02 s), pool de N threads
└─ Station::tcFrame(dt) → Simulation::updateTC(dt)
   ├─ FASE 0  Player::dynamics() → JSBSimModel     ← passo do 6-DOF
   ├─ FASE 1  Antenna/Tws transmitem
   ├─ FASE 2  Radar::receive(), AlertDatalink::receive()  ← promove o alerta encenado
   └─ FASE 3  na ORDEM da lista de componentes do player:
        Autopilot::process()          → comandos do frame anterior viram deflexões
        AirTrkMgr::process()          → pistas deste frame
        FlightAgentTC::controller()   → PERCEPÇÃO → DECISÃO → ATUAÇÃO   ★ aqui
              ├─ FlightState::updateState(player)
              ├─ UbfArbiter::genAction()   (AltitudeSafetyBehavior 90 | BtBehavior 50)
              └─ FlightAction::execute()   → Autopilot + AlertDatalink

laço de app/RealTimeRun.cpp (10 Hz)
└─ Station::updateData(dt)
   └─ DataRecorder::processRecords() → TacviewOutput     ← só isso; nenhuma decisão
```

Compare com a poc/13: lá a caixa ★ ficava na segunda coluna, junto com o gravador.

**Por que o agente é o último da lista de componentes.** Três coisas rodam na fase 3, na ordem
declarada no `.epp`. Declarado por último, o agente enxerga as pistas **já atualizadas** deste
frame, e o comando que ele grava no `Autopilot` vale a partir do frame seguinte — 20 ms de
latência, fixa e igual para todos. É a mesma disciplina do alerta de datalink (escreve numa fase,
publica na fronteira, lê na fase seguinte). Declarado antes do `pilot:`, seria o contrário:
comando no mesmo frame, pistas com um frame de atraso.

---

## 4. O que muda no `.epp`

A `Station` **perde** o bloco `components:` inteiro (os quatro `( SimAgent )` com
`actorPlayerName`), e cada caça **ganha** um `agent:` no fim da sua lista de componentes:

```diff
  ( Station
-    components: {
-       agent1: ( SimAgent  actorPlayerName: falcon1
-          state: ( FlightState )
-          behavior: ( UbfArbiter ... ) )
-       ... agent2, agent3, agent4 ...
-    }
     simulation: ( WorldModel
        players: {
           falcon1: ( Aircraft
              components: {
                 dynamicsModel: ( JSBSimModel ... )
                 pilot:         ( Autopilot ... )
                 datalink:      ( AlertDatalink ... )
                 antennas:      ( Gimbal ... )   sensors: ( SensorMgr ... )
                 obc:           ( OnboardComputer ... )
+                agent: ( FlightAgentTC
+                   state: ( FlightState )
+                   behavior: ( UbfArbiter ... ) )        ← mesmos números da poc/13
              } )
```

O conteúdo do agente (estado, árbitro, votos, os ~14 parâmetros do `BtBehavior`) é **idêntico**,
linha por linha. Muda a classe, o lugar na árvore e o fato de não haver mais `actorPlayerName`.

---

## 5. O que muda na aplicação (`main.cpp` + `app/`)

Quase nada — e é esse o ponto:

| arquivo | poc/13 | poc/14 |
|---|---|---|
| `app/RealTimeRun.cpp` | **idêntico** — mas `station->updateData(dt)` drena o gravador **e** roda os agentes | **idêntico** — `station->updateData(dt)` só drena o gravador |
| `app/DeterministicRun.cpp` | **idêntico** — `tcFrame()` + `updateData()` em lockstep é o que dá o determinismo | **idêntico** — o determinismo já vem do frame; `updateData()` está lá só para o Tacview |
| `app/StatusReport.cpp` | — | acrescenta `dec=` (decisões do agente) e `thr=` (thread que decidiu) |
| `app/DeterministicDump.cpp` | — | acrescenta `dec=` ao dump comparável |

Os dois laços são o **mesmo arquivo** nas duas pocs, byte a byte: quem muda é onde o agente está
declarado no `.epp`, não o código que roda o laço.

A observabilidade nova é de propósito: `dec` é a contagem que prova a mudança de taxa, e `thr` é o
índice da thread do pool T/C que decidiu por último. `dec` entra no dump de determinismo (é
determinístico: uma decisão por frame); `thr` **não** entra (depende do escalonador).

---

## 6. Determinismo — o ponto da poc

### 6.1 O que a poc/13 tinha de ressalva

Lá a decisão roda em `updateData()`, numa thread com relógio próprio (10 Hz) enquanto a física
roda em outra (50 Hz). Em tempo real isso dá três defeitos de tempo: ponto de amostragem
arbitrário dentro do frame, número variável de decisões por segundo simulado, e leitura do
`Player` concorrente com a escrita da física. O modo `-deterministic` os remove **colapsando as
duas threads numa só** — `tcFrame()` e depois `updateData()`, em lockstep. Determinismo do
harness.

### 6.2 O que muda aqui

A decisão é **parte do frame**. A fase 3 já é um trecho ordenado e com barreira: nenhum player
entra nela antes que todos tenham terminado a fase 2, e nenhum sai do frame antes que todos
tenham terminado a fase 3. Os três defeitos somem por construção — **sem depender de como a
aplicação chama as coisas**:

| defeito | poc/13 | poc/14 |
|---|---|---|
| ponto de amostragem no frame | arbitrário (removido pelo lockstep) | sempre a fase 3 |
| decisões por segundo simulado | varia com o jitter do `msleep` | exatamente 1 por frame |
| leitura concorrente do player | existe (removida pelo lockstep) | não existe: a decisão *é* o frame |

O laço de `app/RealTimeRun.cpp` poderia sumir inteiro (só o Tacview pararia de receber) que a
simulação continuaria evoluindo do mesmo jeito, frame a frame.

### 6.3 O paralelismo continua ligado

`make check-tc-agent` roda 2000 frames com **1, 2 e 4 threads T/C** (mais uma repetição de 4) e
compara os dumps: **byte a byte idênticos**. Ou seja, a decisão foi para dentro do pool de threads
— quatro agentes decidindo em paralelo, em threads diferentes — e o resultado não muda.

Isso funciona pelas mesmas três razões da poc/13, que continuam valendo palavra por palavra:
passo fixo (`dt = 1/rate`, calculado uma vez), **fusão comutativa** dos alertas (vence o contato
mais próximo; empate exato, o emissor de menor id) e **escolha de pista sem depender da ordem** da
lista do `TrackManager`. A diferença é que agora elas protegem também o caminho da decisão, que
antes nem estava no jogo.

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
**agir** através dele, e o motivo é a [anatomia do frame](#3-onde-a-decisão-entra-no-frame).

A lista de players é varrida **4 vezes por frame**, uma por fase (`Simulation.cpp:544-568`), e os
players podem estar em **threads diferentes** do pool T/C. Então, do seu componente:

- **ler** o outro player é corrida — ele pode estar no meio de `dynamics()` em outra thread, com
  a posição meio escrita;
- **escrever** nele é pior — você escreve fora da ordem determinística do frame, e o resultado
  passa a depender do escalonador.

`event()` sozinho **não** resolve a corrida: ele também roda na thread do emissor. O que ele dá é
um **ponto único de entrada**, que o receptor pode disciplinar. É exatamente o que o
`AlertDatalink` faz, e é o que sustenta o `make check-tc-agent`:

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
  e a pista aparece no **mesmo** frame. Inverta os dois no EDL e ela atrasa um frame. É
  exatamente a razão pela qual o `agent:` desta poc é declarado por **último** na lista de
  componentes do player (ver [seção 4](#4-o-que-muda-no-epp)).

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

### 7.7 O caminho desta poc, fim a fim — e o que a poc/14 muda nele

É o mecanismo pedido pelo enunciado: um evento de um player que muda o comportamento dos outros.
A peça é **a mesma** da poc/13 (`AlertDatalink` é byte a byte idêntico nas duas). O que muda é
**de qual thread ela é acionada** — e isso muda o que a disciplina de 7.2 está protegendo.

```
falcon3 detecta bandit1 (radar nativo -- 7.3)
   └─ FASE 3: FlightAgentTC::controller()                  ← na poc/13 isto era updateData()
        ├─ FlightState::updateState()   → vê a pista deste mesmo frame
        ├─ UbfArbiter::genAction()      → árvore: ContactDetected → ReportAndEvade
        └─ FlightAction::execute()
             └─ AlertDatalink::broadcastAlert()
                  └─ Datalink::sendMessage(TacticalAlert*)     [NATIVO -- 7.4]
                       └─ ... → player->event(DATALINK_MESSAGE, msg)
                            └─ AlertDatalink::onDatalinkMessageEvent()   ← nosso gancho
                                 └─ ENCENA o alerta (fusão comutativa)
   ... fronteira de fase ...
   fase 2 do frame seguinte: AlertDatalink::receive() promove encenado → corrente
   fase 3 do frame seguinte: FlightState vê hasAlert → árvore: AlertReceived → SupportAlert
```

**A diferença que importa** — quem emite, e de onde:

| | poc/13 | poc/14 |
|---|---|---|
| de onde sai o `broadcastAlert()` | `SimAgent::controller()`, em `updateData()` | `FlightAgentTC::controller()`, **fase 3 do frame** |
| thread do emissor | a **única** thread de background | uma das **N** threads do pool T/C |
| os quatro emissores entre si | **em sequência**, na ordem da lista de componentes da `Station` | **em paralelo**, um por thread — medido em [8.4](#84-as-quatro-decisões-acontecem-em-quatro-threads) |
| ordem de chegada de dois alertas no mesmo frame | determinística (a da lista) | **do escalonador** |

Ou seja: as três decisões de modelagem abaixo já estavam certas na poc/13, mas duas delas ainda
não estavam sendo **exercidas**. Aqui passam a ser o que sustenta o `make check-tc-agent`.

1. **O handler roda na thread do EMISSOR.** O `sendMessage()` nativo chama `event()` direto no
   destino. Por isso o handler só *encena* o alerta, sob um mutex curto, e nunca mexe no estado
   corrente. *(Já valia na poc/13: lá o emissor estava no background e o receptor podia estar na
   fase 2, em outra thread. Aqui os dois estão no pool T/C.)*
2. **A entrada não é fila FIFO.** A fusão é **comutativa** — vence o contato de menor distância;
   em empate exato, o emissor de menor id — então o resultado independe da ordem de chegada.
   *(É esta que muda de estatuto: na poc/13, com um único emissor serial, a ordem era estável de
   qualquer jeito; aqui ela é genuinamente do escalonador, e a comutatividade é o que impede o
   dump de 1, 2 e 4 threads de divergir.)*
3. **A promoção acontece numa fronteira de fase.** O alerta encenado só passa a valer na fase 2
   do frame seguinte, dando **latência fixa de um frame** para todos, em vez de "às vezes no
   mesmo frame, às vezes no próximo".

É a mesma disciplina que o framework usa entre as suas 4 fases: escreve numa fase, publica na
fronteira, lê na fase seguinte.

> **Efeito colateral da taxa.** Como a decisão passou de ~10 Hz para ~50 Hz, o `ReportAndEvade`
> passa a transmitir ~50 alertas/s onde a poc/13 transmitia ~10 — números em
> [8.3](#83-em-tempo-real-50-hz-contra-10-hz). Nada quebra (a fusão é comutativa e o `holdTime`
> absorve), mas se o custo por transmissão importar, o lugar de resolver é a **política** — um
> período mínimo entre transmissões no nó da árvore —, não o canal.

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

## 8. O que foi medido rodando

Tudo abaixo saiu do binário, não de dedução.

### 8.1 Uma decisão por frame, em 1, 2 e 4 threads

```
$ make check-tc-agent
  OK   threads-4 == threads-4b
  OK   threads-1 == threads-2
  OK   threads-1 == threads-4
  decisões por frame (tem que bater com o número de frames):
frame=2000  dec=2001
determinismo: OK (estado idêntico em todas as execuções)
```

`dec=2001` em 2000 frames: a decisão a mais é o `tcFrame()` de aquecimento que
`app/StationBuilder.cpp` dispara logo depois do `RESET_EVENT`.

### 8.2 Em passo fixo, as duas pocs dão o **mesmo** estado

```
$ ./build/src/13-native-stack/src/native-stack -threads 1 -deterministic 300 | grep 'frame=300 player=falcon1'
frame=300 player=falcon1 n=9255.883978869 e=458.464915958 alt=3000.003703427 hdg=92.740379131 ... sent=199 recv=0

$ ./build/src/14-tc-agent/src/tc-agent       -threads 1 -deterministic 300 | grep 'frame=300 player=falcon1'
frame=300 player=falcon1 n=9255.883978869 e=458.464915958 alt=3000.003703427 hdg=92.740379131 ... sent=199 recv=0 dec=301
```

Idênticos até o último decimal (o `dec=` é o campo novo do status). **E faz sentido:** no laço de
passo fixo da poc/13, `updateData()` roda imediatamente depois do `tcFrame()` e com o mesmo `dt` —
e como a fase 3 é a última do frame, "no fim da fase 3" e "logo após o frame" são o mesmo instante.

É o melhor resumo do que esta poc isola: **o modelo é o mesmo; o que muda é quem garante a
ordem**. Em lockstep, o harness garante — e os dois resultados coincidem. Fora do lockstep, só a
poc/14 continua garantindo.

### 8.3 Em tempo real: 50 Hz contra 10 Hz

Status da poc/14 rodando de verdade (`dec` a cada 2 s):

```
[t=2s]  falcon1 ... dec=145 thr=0
[t=4s]  falcon1 ... dec=245 thr=0        +100 decisões em 2 s  =  ~50 Hz  (taxa do tcRate)
[t=10s] falcon1 ... dec=545 thr=0
```

Na poc/13 essa mesma conta daria ~10 Hz — a taxa do laço de background (`bgRate` em
`app/RealTimeRun.cpp`).
**A decisão ficou 5× mais frequente** sem que nenhum parâmetro do modelo mudasse.

Efeito colateral que vale conhecer: o que a decisão dispara acompanha a taxa. O
`ReportAndEvade` transmite um `TacticalAlert` por decisão, então em tempo real a poc/14 emite
~50 alertas/s onde a poc/13 emitia ~10. Nada quebra (a fusão é comutativa e o `holdTime` do
datalink absorve), mas se o custo por decisão importar, o lugar de resolver isso é a política —
um período mínimo entre transmissões no nó da árvore —, não o agente.

### 8.4 As quatro decisões acontecem em quatro threads

```
[t=10s]
   falcon1 ... bt=PATROL   ... dec=545 thr=0
   falcon2 ... bt=SUPPORT  ... dec=545 thr=1
   falcon3 ... bt=SUPPORT  ... dec=545 thr=2
   falcon4 ... bt=SUPPORT  ... dec=545 thr=4
```

Mesmo número de decisões, **threads diferentes**: os agentes rodam dentro do pool T/C, em
paralelo, um por player — e ainda assim o dump é idêntico com 1, 2 ou 4 threads (7.1). Os índices
de thread não são contíguos porque a numeração é por ordem de primeira chamada
(`xnative::ThreadTag`), e o laço de background também pega um índice.

---

## 9. Qual das duas usar

| | `( SimAgent )` — poc/13 | `( FlightAgentTC )` — poc/14 |
|---|---|---|
| custo de escrita | **zero** (classe do framework) | ~40 linhas + registro na factory |
| taxa de decisão | a do laço de background | a do frame (tcRate) |
| determinismo em tempo real | **não** | **sim**, por construção |
| determinismo em passo fixo | sim, se o laço mantiver o lockstep | sim, independente do laço |
| onde o agente mora | `Station`, ator por nome | dentro do player, ator = container |
| custo por frame | fora do caminho crítico | **dentro** do caminho crítico |

Regra prática que sai daqui:

- **Decisão lenta e tolerante a jitter** (planejamento, lógica de missão, um agente que pensa a
  cada segundo) → `SimAgent` resolve, e mantém o trabalho fora do frame.
- **Decisão que fecha malha com a física** (é o caso desta poc: a árvore comanda o autopilot) ou
  qualquer coisa que precise ser reproduzível fora do laço de teste → `AgentTC` próprio.
- O caminho do meio existe: manter o `AgentTC` e **decimar** por dentro (decidir a cada k frames,
  contando na própria classe). Continua determinístico, e sem pagar a árvore inteira a 50 Hz.

---

## 10. Como verificar tudo

```bash
make build

# a diferença de código entre os dois subprojetos
make compare-13-14

# determinismo com 1, 2 e 4 threads + a contagem de decisões por frame
make check-tc-agent

# tempo real (Tacview na porta 1234; Ctrl+C encerra) -- olhe 'dec' e 'thr' no status
make run-tc-agent

# o mesmo estado em passo fixo nas duas pocs (deve ser idêntico, fora o campo 'dec')
./build/src/13-native-stack/src/native-stack -threads 1 -deterministic 300 | grep 'frame=300 '
./build/src/14-tc-agent/src/tc-agent         -threads 1 -deterministic 300 | grep 'frame=300 '
```

Para tudo o mais — anatomia do frame, o que vem do framework, o radar nativo, as armadilhas do
JSBSim e do gravador, a semântica ACMI do Tacview — vale integralmente o
**[README da poc/13](../13-native-stack/README.md)**: aqui nada disso mudou.

---

## 11. Controle de tempo — acelerar, frear, pausar

Igual à poc/13, e pelo mesmo código (`shared/xclock/`, uma cópia só para as duas):

| tecla | efeito |
|---|---|
| `+` `=` | acelera (próximo degrau) |
| `-` `_` | freia (degrau anterior; abaixo de `1x` é câmara lenta) |
| `espaço` `p` | pausa / retoma |
| `1` | volta a tempo real e retoma |
| `h` `?` | reimprime a ajuda |

O cenário declara `( ClockStation )` no lugar de `( Station )`, e a linha de status passou a
mostrar o tempo simulado ao lado do tempo de parede (`[t=24s sim=8.2s PAUSADO (1x)]`).

Validado nesta poc: com a simulação pausada, oito amostras consecutivas de `falcon1` ao longo de
16 s de parede saíram byte a byte idênticas, e o relógio simulado parou em 8,2 s. O modo
`-deterministic` não é afetado — ele chama `tcFrame()` direto, sem passar pelo controle de tempo.

Um detalhe é próprio desta poc: aqui a decisão roda **dentro** do frame de tempo crítico
(`FlightAgentTC`, fase 3), então ela para junto com a pausa e acelera junto com o
`fastForwardRate` — ao contrário do `SimAgent` da poc/13, que decide no laço de background e
continua avaliando com a simulação congelada.

Por que acelerar é nativo, por que frear não é, e a armadilha do `execTime += dt` que acontece
*antes* do teste de freeze: **[seção 14 do README da poc/13](../13-native-stack/README.md#14-controle-de-tempo--acelerar-frear-pausar)**.
