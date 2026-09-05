# multi-thread

> **ATUALIZAÇÃO — esta poc não tem mais executável próprio.** A camada de aplicação
> (`include/app/` + `src/app/` + `mixr_factory`, ~1.500 linhas que eram copiadas byte a byte em
> cada poc) saiu daqui: quem executa agora é o **`./app`**, o runner único —
> `app -scenario multi-thread`, ou `make run-multi-thread`. O que sobra nesta pasta é o **cenário**
> (`configs/`), os dados de execução (`data/`) e este README. Trechos abaixo que citam
> `src/app/…`, `main.cpp` ou `build/src/poc/…` descrevem a estrutura ANTERIOR — a explicação de
> cada etapa continua valendo, só que os arquivos moram em `app/src/app/`. Ver
> [src/poc/meson.build](../../meson.build) para o porquê e para a prova de neutralidade (os dumps
> saíram byte-idênticos).

A [single-thread](../single-thread/) **inteira**, com **uma** diferença: o agente do UBF é um
`AgentTC` próprio, que decide na **fase 3 do frame de tempo crítico**, no lugar do
`( SimAgent )` nativo, que decide em `updateData()` — na thread de background.


> **ONDE MORA O QUÊ, depois que o modelo virou plugin.** Esta pasta é o **host**: `main.cpp`,
> `mixr_factory.cpp` e os dez módulos de `app/` — o laço, o cenário, a `Station`, a exportação e
> o dump. Mais nada.
>
> A **política** descrita acima — `domain/`, `bt/`, `ubf/`, `xnative/` — não está aqui e nem é
> compilada junto: mora em **[models/A4/](../../models/A4/)**, um projeto
> Meson independente, construído numa etapa **anterior** e carregado com `dlopen` durante o parse
> do cenário. O que este README descreve das seções 7 a 10 continua valendo, só que os arquivos
> ficam lá.
>
> Isso não é arrumação: é o que torna verificável o cenário de um terceiro entregar só o binário.
> Ver [models/README.md](../../models/README.md) para escrever um modelo novo, e
> [shared/xplugin/README.md](../../shared/xplugin/README.md) para o contrato.

```bash
make build
make run-multi-thread          # Tacview Real-Time Telemetry na porta 1234; Ctrl+C encerra
make check-multi-thread        # verifica o determinismo (1, 2 e 4 threads T/C)
make compare-single-multi      # lista o que difere entre os dois subprojetos
```

> **Rode sempre a partir da raiz do repositório**: cenário, dados do JSBSim, tile SRTM e gravação
> `.acmi` são resolvidos por caminho relativo (`./src/poc/dis/multi-thread/...`, `./shared/data/...`).

**Por que ela existe.** A [seção 12 do README da single-thread](../single-thread/README.md#12-determinismo)
termina numa ressalva: lá o determinismo é propriedade do *harness* — do laço de
`app/DeterministicRun.cpp` serializando `tcFrame()` e `updateData()` —, e não do modelo. Esta poc
paga o preço de escrever o agente para que passe a ser propriedade do **modelo**, e **mede** a
diferença em vez de argumentar sobre ela.

---

## Índice

1. [A diferença, em uma tela](#1-a-diferença-em-uma-tela)
2. [O que é compartilhado (e onde ler)](#2-o-que-é-compartilhado-e-onde-ler)
3. [Dissecação da `FlightAgentTC`, linha por linha](#3-dissecação-da-flightagenttc-linha-por-linha)
4. [Onde a decisão entra no frame](#4-onde-a-decisão-entra-no-frame)
5. [O que muda no `.edl`](#5-o-que-muda-no-epp)
6. [O que muda na aplicação](#6-o-que-muda-na-aplicação)
7. [Determinismo — o ponto da poc](#7-determinismo--o-ponto-da-poc)
8. [O que o terreno muda aqui](#8-o-que-o-terreno-muda-aqui)
9. [O que foi medido rodando](#9-o-que-foi-medido-rodando)
10. [Qual das duas usar](#10-qual-das-duas-usar)
11. [Como verificar tudo](#11-como-verificar-tudo)

---

## 1. A diferença, em uma tela

| peça | single-thread | multi-thread |
|---|---|---|
| player / 6-DOF / controle / radar / datalink / terreno | `Aircraft` + `JSBSimModel` + `Autopilot` + `Gimbal/Antenna/Tws/AirTrkMgr` + `AlertDatalink` + `SrtmHgtFile` | **idêntico** |
| percepção / decisão / atuação | `FlightState` / `BtBehavior` + `AltitudeSafetyBehavior` / `FlightAction` | **idêntico** |
| árbitro | `( UbfArbiter )` nativo | **idêntico** |
| árvore de comportamento | `configs/flight_tree.xml` | **idêntico** |
| regras puras (`domain/`) | `PatrolPlan`, `RtbPlan`, `ThreatPolicy`, `TerrainFloor`, `geometry` | **idêntico** |
| histerese da evasão | `domain::ThreatPolicy` + slot `evadeHold` | **idêntico** |
| **agente** | **`( SimAgent )`** — nativo, componente da **Station**, ator por nome | **`( FlightAgentTC )`** — nosso, componente do **player**, ator = container |
| **quando decide** | `updateData()` → thread de **background**, na taxa do laço do `main` (10 Hz) | `updateTC()` fase 3 → thread de **tempo crítico**, todo frame (50 Hz) |
| **os 4 agentes** | **em sequência**, numa thread só | **em paralelo**, um por thread do pool |
| **de onde o alerta é emitido** | do background | da fase 3 — ver [7.3](#73-o-paralelismo-continua-ligado) |

> A linha *histerese da evasão* existe porque sem ela as duas pocs voavam "batendo asa". O
> diagnóstico e a correção estão na [seção 8 do README da single-thread](../single-thread/README.md#8-a-cadeia-de-decisão-ubf--behaviortree)
> — e valem igual aqui, porque o modelo é o mesmo: **a taxa de decisão não tinha nada a ver com o
> problema** (1, 2 ou 4 threads, 10 Hz ou 50 Hz, os números eram os mesmos).

`make compare-single-multi` mostra o tamanho real da mudança — hoje, com o modelo já vivendo em
`models/A4/` como plugin (ver a nota no topo deste README), a lista é bem menor do que quando
`FlightAgentTC` ainda era um arquivo do HOST:

```
Only in src/poc/dis/single-thread/configs: scenario_missile_demo.edl.in
Files .../configs/scenario.edl.in differ            ← os 4 agentes mudam de lugar e de classe;
                                                        libflight.so vs. libflight_tc.so
Files .../src/app/ScenarioTemplate.cpp differ        ← teto padrão de threads T/C diferente
                                                        (8 aqui, 4 na single-thread -- ver §6.1)
Files .../src/main.cpp differ                        ← banner e comentários de cabeçalho
Files .../src/meson.build differ                     ← só o nome do executable()
Files .../README.md differ                           ← este arquivo
```

**Nenhum arquivo de `domain/`, `ubf/`, `bt/` ou dos outros modelos de `xnative/` foi tocado** — a
classe `FlightAgentTC` mora em `models/A4/`, compilada nos dois `.so` do mesmo `meson.build`
(`libflight.so` sem ela, `libflight_tc.so` com ela, sob `-DFLIGHT_TC_AGENT`; ver "O MODELO é um
plugin" no `CLAUDE.md`). `app/StatusReport.cpp` e `app/DeterministicDump.cpp` são hoje
**byte-idênticos** nas duas pocs: os dois leem `dec=`/`thr=` do mesmo `xboard::Readout`, publicado
no mesmo ponto (`ubf::FlightAction::execute()`) para as duas pilhas — ver a nota de §3.5 e a
tabela da §6. É essa a demonstração: **onde a decisão roda é uma escolha de integração do
cenário/modelo, não do host.**

> **Sobre o namespace:** continua `mixr::xnative`, igual ao da single-thread, de propósito —
> é o que permite que `diff -r` entre os dois subprojetos mostre exatamente a diferença que a poc
> quer discutir, sem ruído de renomeação. São executáveis separados, então não há conflito.

---

## 2. O que é compartilhado (e onde ler)

Este README é escrito como **delta**. Tudo o que não é o agente está dissecado no README da
gêmea, e vale aqui palavra por palavra:

| assunto | onde |
|---|---|
| o que vem do framework e o que sobra para escrever | [§1](../single-thread/README.md#1-o-que-vem-do-framework-e-o-que-é-nosso) |
| anatomia de um frame (as 4 fases) | [§2](../single-thread/README.md#2-anatomia-de-um-frame) |
| como o framework chama o nosso código | [§3](../single-thread/README.md#3-como-o-framework-chama-o-nosso-código) |
| a árvore de objetos do `.edl` | [§4](../single-thread/README.md#4-a-árvore-de-objetos-do-cenário) |
| padrões dos exemplos oficiais | [§5](../single-thread/README.md#5-padrões-dos-exemplos-oficiais-usados-aqui) |
| cada peça nativa (Aircraft, JSBSim, Autopilot, radar, datalink) | [§6](../single-thread/README.md#6-cada-peça-nativa-uma-a-uma) |
| **dissecação de todos os arquivos em ordem de dependência** | [§7](../single-thread/README.md#7-dissecação-o-repositório-em-ordem-de-dependência) |
| a cadeia UBF + BehaviorTree | [§8](../single-thread/README.md#8-a-cadeia-de-decisão-ubf--behaviortree) |
| interação entre players (os 3 canais, a receita) | [§9](../single-thread/README.md#9-interação-entre-players) |
| elevação de terreno | [§10](../single-thread/README.md#10-elevação-de-terreno) |
| Tacview e a semântica ACMI | [§11](../single-thread/README.md#11-tacview) |
| armadilhas do JSBSim, do gravador e do datalink | [§13](../single-thread/README.md#13-armadilhas-encontradas-rodando) |
| controle de tempo (acelerar/frear/pausar) | [§14](../single-thread/README.md#14-controle-de-tempo--acelerar-frear-pausar) |

**Comece por lá.** O que segue aqui é só o que é diferente.

Uma nota sobre a [dissecação em ordem de dependência](../single-thread/README.md#7-dissecação-o-repositório-em-ordem-de-dependência):
ela vale integralmente para este subprojeto, com **uma** inserção. `FlightAgentTC` entra na
**camada 3** (`xnative/`, classes derivadas do MIXR) — depende de `xnative/ThreadTag` e dos
headers de `models`, e é registrada na **camada 6** (`xnative/factory.cpp`). Nada em `domain/`,
`ubf/` ou `bt/` sabe que ela existe.

---

## 3. Dissecação da `FlightAgentTC`, linha por linha

[`include/xnative/FlightAgentTC.hpp`](../../models/A4/include/xnative/FlightAgentTC.hpp) ·
[`src/xnative/FlightAgentTC.cpp`](../../models/A4/src/xnative/FlightAgentTC.cpp) — 43 linhas de código, e **cada
bloco delas existe por causa de uma armadilha do framework**. É o resumo mais honesto do que
custa mover uma decisão para dentro do frame.

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

### 3.1 Armadilha 1 — `UbfAgentTC` existe, mas nenhuma factory do MIXR o constrói

`base::ubf::AgentTC` está lá, pronto, no mesmo header do `Agent`. Mas `base/factory.cpp` registra
apenas `"UbfAgent"` e `"UbfArbiter"` — escrever `( UbfAgentTC ... )` no EDL **não constrói nada**.

**Um agente de tempo crítico é, na prática, código da aplicação**: a classe é do framework, o
registro é seu. Uma linha em [`src/xnative/factory.cpp`](../../models/A4/src/xnative/factory.cpp):

```cpp
else if ( name == FlightAgentTC::getFactoryName() )  obj = new FlightAgentTC();
```

> **Detalhe do slottable.** A classe usa `EMPTY_SLOTTABLE`, e ainda assim `state:` e `behavior:`
> funcionam no `.edl`: a busca de slot **sobe a hierarquia**, e esses dois slots são de
> `ubf::Agent`. `EMPTY_SLOTTABLE` aqui significa "não acrescento slot nenhum", não "não tenho
> slots".

### 3.2 Armadilha 2 — `AgentTC::updateTC()` chama `controller()` em **toda** fase

O agente é um `base::Component` dentro do player, e a lista de players é percorrida **4× por
frame** — uma vez por fase, com `dt/4`. `AgentTC::updateTC()` não filtra nada:

```cpp
void AgentTC::updateTC(const double dt) { controller(dt); }   // framework
```

Sem filtro, a decisão rodaria **4 vezes por frame**, três delas nas fases erradas (dinâmica,
transmissão, recepção) — lendo um estado meio atualizado e comandando o autopilot antes de o
`AirTrkMgr` ter processado as pistas. O filtro é o mesmo que `models::System` aplica às suas
quatro fases:

```cpp
const models::WorldModel* const world{player->getWorldModel()};
if (world == nullptr) return;

if (world->phase() != 3) return;      // a decisão pertence à fase "lógica e controle"

lastThreadTag.store(threadTag(), std::memory_order_relaxed);
BaseClass::controller(dt * 4.0);      // e com o dt do frame INTEIRO, não dt/4
decisions.fetch_add(1, std::memory_order_relaxed);
```

**O `dt * 4.0` não é detalhe.** O `domain::PatrolPlan` integra o relógio da perna com esse `dt`, e
a `domain::ThreatPolicy` envelhece a histerese com ele. Passar `dt/4` faria a patrulha e a
histerese andarem 4× mais devagar — e nada acusaria o erro.

### 3.3 Armadilha 3 — `Agent::updateData()` **também** chama `controller()`

Esta não estava documentada em lugar nenhum e é a que morde. `AgentTC` **acrescenta** `updateTC()`
mas **não desliga** o caminho de background herdado de `Agent`:

```cpp
void Agent::updateData(const double dt) { controller(dt); }   // framework — continua valendo
```

E `Player::updateData()` propaga para a lista de componentes (`Player.cpp:636` →
`Component::updateData()`). Ou seja: um agente dentro do player decidiria **duas vezes por
frame** — uma no tempo crítico e outra no background.

**E o filtro de fase não salva:** ao fim do `tcFrame()` a fase corrente **fica em 3**, então a
chamada de background passa direto pelo `if`. Por isso:

```cpp
void FlightAgentTC::updateData(const double) { }   // no-op deliberado
```

Não se está suprimindo trabalho: o `Agent` nativo **não** repassa `updateData()` ao
`state`/`behavior` — é a mesma armadilha que obriga o `BtBehavior` a configurar os planos
preguiçosamente ([§13.7 da gêmea](../single-thread/README.md#137-o-agent-não-propaga-o-ciclo-de-componentes)) —, então não há nada a propagar.

### 3.4 O ator vem do container

```cpp
void FlightAgentTC::initActor()
{
   if (getActor() != nullptr) return;
   const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
   if (player != nullptr) setActor(player);
}
```

O `SimAgent` nativo faz o contrário: mora na `Station` e resolve o ator **por nome**, procurando
na lista de players do `WorldModel` (slot `actorPlayerName`). Aqui basta **subir a cadeia de
containers** — e a consequência prática é que **o bloco EDL fica idêntico para as quatro
aeronaves**, sem nenhum nome a amarrar. Um slot `*Name:` a menos é uma classe inteira de erro
silencioso a menos ([§3 da gêmea](../single-thread/README.md#3-como-o-framework-chama-o-nosso-código)).

### 3.5 Os dois contadores, e por que um deles não entra no dump

```cpp
std::atomic<long> decisions{};      // observável E determinístico: 1 por frame
std::atomic<int>  lastThreadTag{-1}; // depende do escalonador
```

`decisions` entra no dump de determinismo — é ele que **prova** que a decisão está amarrada ao
frame, porque tem de bater com o número de frames em 1, 2 ou 4 threads. `lastThreadTag` **não**
entra: depende do escalonador, e o contrato do
[`DeterministicDump`](include/app/DeterministicDump.hpp) proíbe. Ele aparece só no status humano,
onde serve para mostrar que os quatro agentes rodam em threads diferentes.

Os dois são `std::atomic` porque são escritos na thread T/C e lidos no laço de background.
`memory_order_relaxed` basta: são contadores de diagnóstico, não sincronizam nada.

> **Nota de arquitetura, mais recente que o parágrafo acima.** `getDecisionCount()`/
> `getLastThreadTag()` continuam existindo, mas hoje **nada os chama** — `app/StatusReport.cpp` e
> `app/DeterministicDump.cpp` (do host) leem `dec=`/`thr=` de `mixr::xboard::Readout`, publicado
> por `ubf::FlightAction::execute()` (`xboard::bumpDecisionCount()`/`setThreadTag()`), o mesmo
> ponto de atuação que a `single-thread` usa. `FlightAgentTC::controller()` ainda chama
> `xboard::setThreadTag()` a mais, redundante mas inofensivo (ver a "oitava passada" do `./app`
> no `CLAUDE.md`). Os dois atômicos desta classe ficaram como contadores internos, verificáveis
> só por quem tem acesso direto ao objeto — não são mais a fonte do que aparece na tela.

### 3.6 `findFlightAgent()` — a busca por tipo

```cpp
const FlightAgentTC* findFlightAgent(const models::AirVehicle* air)
{
   const base::Pair* const pair{air->findByType(typeid(FlightAgentTC))};
   ...
}
```

Como o agente é componente do **player**, a busca é a mesma que o framework usa para os
subsistemas: **por tipo**, na lista de componentes. Devolve `nullptr` se a aeronave não declarar
um `( FlightAgentTC )` no `.edl` — o caso de qualquer player sem agente próprio (hoje, nenhum dos
`falcon1..4` cai nisso; o intruso `bandit1` nem é mais um player local aqui, ver a nota no
[README do single-thread](../single-thread/README.md#4-a-árvore-de-objetos-do-cenário) sobre
`src/poc/dis/bandit`).

---

## 4. Onde a decisão entra no frame

Compare com a [anatomia do frame da gêmea](../single-thread/README.md#2-anatomia-de-um-frame):
lá a caixa da decisão está na thread de background; aqui ela está **dentro** da fase 3.

```
thread de tempo crítico (dt = 1/tcRate FIXO — 50 Hz)
└─ Station::tcFrame(dt) → Simulation::updateTC(dt)
   │      (players fatiados entre numTcThreads, com BARREIRA por fase)
   ├─ FASE 0  Player::dynamics() → JSBSimModel  (passo do 6-DOF)
   ├─ FASE 1  Antenna/Tws transmitem
   ├─ FASE 2  Radar::receive() · AlertDatalink::receive() (promove o alerta)
   └─ FASE 3  AirTrkMgr::process()      cria/atualiza as pistas
              FlightAgentTC::controller()  ←←← A DECISÃO ESTÁ AQUI
                 ├─ FlightState::updateState(ator)
                 ├─ UbfArbiter::genAction()  → AltitudeSafety (90) / BtBehavior (50)
                 └─ FlightAction::execute(ator) → Autopilot + AlertDatalink
              Autopilot::process()      consome o comando recém-escrito

thread de background (app/RealTimeRun.cpp, 10 Hz)
└─ Station::updateData(dt)
   ├─ Player::updateData() → updateElevation()    ← elevação do terreno (continua no background)
   └─ DataRecorder::processRecords() → TacviewOutput
        (nenhum agente aqui: FlightAgentTC::updateData() é no-op)
```

**A ordem dentro da fase 3 é ordem de declaração no `.edl`.** O `agent:` é declarado **por
último** na lista de componentes do player, depois de `obc:` — de propósito: assim ele decide
sobre as pistas que o `AirTrkMgr` acabou de criar **neste mesmo frame**, e não sobre as do frame
anterior. Inverter a ordem no EDL custaria um frame de latência, sem uma linha de C++ mudar.

---

## 5. O que muda no `.edl`

A `Station` **perde** o bloco `components:` inteiro (os quatro `( SimAgent )` com
`actorPlayerName`), e cada caça **ganha** um `agent:` no fim da sua lista de componentes:

```diff
  ( ClockStation
-    components: {
-       agent1: ( SimAgent  actorPlayerName: falcon1
-          state: ( FlightState )
-          behavior: ( UbfArbiter ... ) )
-       ... agent2, agent3, agent4 ...
-    }
     simulation: ( WorldModel
        latitude/longitude: -22.25 / -42.48
        terrain: ( SrtmHgtFile path/file: S23W043.hgt )
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
+                   behavior: ( UbfArbiter ... ) )   ← mesmos números da single-thread
              } )
```

O conteúdo do agente — estado, árbitro, votos, os **16** parâmetros do `BtBehavior` e os **5** do
`AltitudeSafetyBehavior` — é **idêntico, linha por linha**. Muda a classe, o lugar na árvore e o
fato de não haver mais `actorPlayerName`.

Todo o resto do `.edl` também é igual: a mesma referência geográfica, o mesmo
`terrain: ( SrtmHgtFile ... )`, as mesmas altitudes derivadas do pico de cada circuito, o mesmo
`dataRecorder` com `enabledList: [ 43 42 ]`, os mesmos `interpolateTerrain: true`.

---

## 6. O que muda na aplicação

Quase nada — e é esse o ponto:

| arquivo | single-thread | multi-thread |
|---|---|---|
| `app/RealTimeRun.cpp` | **idêntico** — mas `station->updateData(dt)` drena o gravador **e** roda os agentes | **idêntico** — `updateData(dt)` só drena o gravador |
| `app/DeterministicRun.cpp` | **idêntico** — `tcFrame()` + `updateData()` em lockstep é o que dá o determinismo | **idêntico** — o determinismo já vem do frame; `updateData()` está lá só para o Tacview |
| `app/TerrainData.cpp` | **idêntico** | **idêntico** |
| `app/ScenarioTemplate.cpp` | teto padrão de `numTcThreads` = **4** | teto padrão = **8** — única diferença real de código entre os dois hosts (ver §6.1) |
| `app/StatusReport.cpp` | **idêntico** — `dec=`/`thr=` vêm do mesmo `xboard::Readout` | **idêntico** |
| `app/DeterministicDump.cpp` | **idêntico** — conta pelo `xboard::Readout`, no ponto da atuação (`FlightAction::execute`) | **idêntico** |
| `app/MetaObjectReport.cpp` | **idêntico** — contadores de instância do MIXR, para detectar vazamento | **idêntico** |
| `mixr_factory.cpp`/`src/meson.build` do HOST | **idêntico** — as 6 classes registradas são as mesmas; `FlightAgentTC` é registrada dentro de `models/A4/`, não aqui | **idêntico** |

Os dois laços são o **mesmo arquivo** nas duas pocs, byte a byte: quem muda é onde o agente está
declarado no `.edl` (e qual `.so` o `( PluginModule )` carrega), não o código do host.

`dec`/`thr` deixaram de ser uma diferença de código entre as pocs — as duas os publicam pelo mesmo
`shared/xboard`. A única diferença de código que sobra no HOST, hoje, é o teto padrão de threads
T/C em `ScenarioTemplate.cpp` (linha acima) — um número de calibração, não uma mudança de
comportamento do agente.

### 6.1 A diferença real e residual: o teto de `numTcThreads`

`app/ScenarioTemplate.cpp` resolve `@NUM_TC_THREADS@` contra `std::min(maxByCpu, N)` quando o
usuário não passa `-threads`. A `single-thread` usa `N=4` (ela decide fora do pool, então mais
threads T/C só serve para tocar a física em paralelo); a `multi-thread` usa `N=8` (aqui os quatro
agentes **também** competem pelo pool, então um teto maior dá mais chance de cada aeronave cair
numa thread própria). Os dois continuam limitados por `maxByCpu` — pedir mais threads do que há
núcleos só acrescenta troca de contexto — e os dois aceitam `-threads N` explícito, que ignora o
teto. Não é uma inconsistência a corrigir: é a única calibração que de fato diverge entre as duas
pocs, porque é a única que depende de quantos players competem pelo pool.

---

## 7. Determinismo — o ponto da poc

### 7.1 O que a single-thread tinha de ressalva

Lá a decisão roda em `updateData()`, numa thread com relógio próprio (10 Hz) enquanto a física
roda em outra (50 Hz). Em tempo real isso dá três defeitos de tempo: ponto de amostragem
arbitrário dentro do frame, número variável de decisões por segundo simulado, e leitura do
`Player` concorrente com a escrita da física. O modo `-deterministic` os remove **colapsando as
duas threads numa só** — `tcFrame()` e depois `updateData()`, em lockstep. **Determinismo do
harness.**

### 7.2 O que muda aqui

A decisão é **parte do frame**. A fase 3 já é um trecho ordenado e com barreira: nenhum player
entra nela antes que todos tenham terminado a fase 2, e nenhum sai do frame antes que todos
tenham terminado a fase 3. Os três defeitos somem **por construção** — sem depender de como a
aplicação chama as coisas:

| defeito | single-thread | multi-thread |
|---|---|---|
| ponto de amostragem no frame | arbitrário (removido pelo lockstep) | sempre a fase 3 |
| decisões por segundo simulado | varia com o jitter do `msleep` | exatamente 1 por frame |
| leitura concorrente do player | existe (removida pelo lockstep) | não existe: a decisão *é* o frame |

O laço de `app/RealTimeRun.cpp` poderia sumir inteiro (só o Tacview pararia de receber) que a
simulação continuaria evoluindo do mesmo jeito, frame a frame.

### 7.3 O paralelismo continua ligado

`make check-multi-thread` roda 2000 frames com **1, 2 e 4 threads T/C** (mais uma repetição de 4)
e compara os dumps: **byte a byte idênticos**. Ou seja, a decisão foi para dentro do pool de
threads — quatro agentes decidindo em paralelo, em threads diferentes — e o resultado não muda.

Isso funciona pelas mesmas três razões da single-thread, que continuam valendo palavra por
palavra: passo fixo (`dt = 1/rate`, calculado uma vez), **fusão comutativa** dos alertas (vence o
contato mais próximo; empate exato, o emissor de menor id) e **escolha de pista sem depender da
ordem** da lista do `TrackManager`. A diferença é que agora elas protegem também o caminho da
decisão, que antes nem estava no jogo.

E há uma quarta, que o terreno acrescentou: **a consulta de elevação é uma leitura de um banco
imutável depois de carregado**, feita sempre no mesmo ponto do passo — os campos `elev=`/`agl=`
entram no dump e continuam idênticos com 1, 2 e 4 threads.

---

## 8. O que o terreno muda aqui

O banco de elevação, o piso anti-CFIT e o piso AGL são **idênticos** aos da gêmea — mesmo tile,
mesmos slots, mesmos números. A dissecação completa está na
[§10 da single-thread](../single-thread/README.md#10-elevação-de-terreno).

**Há uma única diferença, e ela é de tempo.** `Player::updateElevation()` roda em
`Player::updateData()` — na fase de **background** —, não numa das quatro fases do frame:

| | quando a elevação é escrita | quando a decisão a lê | defasagem |
|---|---|---|---|
| single-thread | `updateData()`, 10 Hz | `updateData()`, no mesmo passo | ~0 |
| **multi-thread** | `updateData()`, 10 Hz | fase 3 do `tcFrame()`, 50 Hz | **até 100 ms** |

Cem milissegundos a 82 m/s são ~8 m de deslocamento — irrelevante para um piso com 800 m de
folga. E **não afeta o determinismo**: em `-deterministic` o laço faz `tcFrame()` e `updateData()`
em sequência no mesmo passo, então a defasagem é sempre exatamente um frame, com qualquer número
de threads T/C. `make check-multi-thread` confirma.

> Se algum dia essa defasagem passar a importar (um seguidor de terreno rigoroso, por exemplo), a
> saída é consultar o banco **direto** na percepção — `FlightState::updateState()` tem acesso a
> `air->getWorldModel()->getTerrain()`, e `getElevation()` é `const`. Custa uma consulta por
> aeronave por ciclo de decisão e devolve um valor fresco e em fase — mas troca a pilha nativa por
> uma consulta própria, que é exatamente o que estas duas pocs existem para evitar.

---

## 9. O que foi medido rodando

Tudo abaixo saiu do binário, não de dedução.

### 9.1 Uma decisão por frame, em 1, 2 e 4 threads

```
$ make check-multi-thread
  OK   threads-4 == threads-4b
  OK   threads-1 == threads-2
  OK   threads-1 == threads-4
  OK   uma decisao por frame, por aviao, nas 3 configuracoes
determinismo (multi-thread): OK
```

**A contagem de decisões deixou de ser impressa e virou asserção.** Antes o `Makefile` mostrava
`frame=2000 dec=2001` e cabia a quem lesse conferir; hoje o
[script de verificação](../../tests/determinism/check_determinism.sh) falha sozinho.

Mas repare no que se afirma: **não** é `dec == frames`. `dec=2001` em 2000 frames está correto — a
decisão a mais é o `tcFrame()` de aquecimento que
[`app/StationBuilder.cpp`](src/app/StationBuilder.cpp) dispara logo depois do `RESET_EVENT`. Isso
é *offset* de partida, não perda de vínculo com o frame. O que se afirma é que `dec` avança na
**mesma taxa** que `frame` entre dois dumps consecutivos — mede a propriedade que interessa e
ignora o *offset*.

A `single-thread` também ganhou o campo `dec=`, que antes só existia aqui. Como lá a decisão roda
no laço de background, quem conta é o `BehaviorBoard`, no ponto da atuação — e a mesma asserção
vale para os dois lados. Na suíte, os dois casos são nomeados por **onde a decisão roda**:
`determinism-critico` e `determinism-nao-critico`.

### 9.2 Em passo fixo, as duas pocs dão **praticamente** o mesmo estado

```
$ ./build/src/poc/dis/single-thread/src/single-thread -threads 1 -deterministic 2000 | grep 'frame=2000 player=falcon1'
frame=2000 player=falcon1 n=... e=... alt=... elev=... agl=... bt=EVADE ...

$ ./build/src/poc/dis/multi-thread/src/multi-thread   -threads 1 -deterministic 2000 | grep 'frame=2000 player=falcon1'
frame=2000 player=falcon1 n=... (mesmos campos) ... dec=2001
```

Comparando **todos** os campos de **todos** os players ao longo de 2000 frames (40 s simulados),
a divergência máxima medida é:

| campo | divergência máxima |
|---|---|
| `n` (posição norte) | 4,4 × 10⁻⁶ m |
| `trackRange` | 3,3 × 10⁻⁶ m |
| `e` (posição leste) | 1,9 × 10⁻⁶ m |
| `elev` / `agl` | ~1,2 × 10⁻⁶ m |
| `alt`, `hdg`, `roll`, `pitch`, `spd`, `mach`, `fuel` | ≤ 2,4 × 10⁻⁷ |

Nenhum campo **textual** diverge: `bt=`, `track=` e `alert=` são iguais em todos os 2000 frames —
ou seja, **as duas pocs tomam exatamente as mesmas decisões**, na mesma ordem.

A divergência aparece já no frame 100 (1,1 × 10⁻⁸ m) e cresce devagar e linearmente — é o efeito
de a decisão cair **um frame de distância** nos dois casos: aqui ela acontece na fase 3 do frame
N e o `Autopilot` a consome no mesmo frame; lá ela acontece depois do frame N e o `Autopilot` a
consome no frame N+1. Em regime de comando constante isso é invisível; em transiente, deixa esse
resíduo. **Quatro micrômetros em quarenta segundos de voo** está muitas ordens de grandeza abaixo
de qualquer significado físico.

> É o melhor resumo do que esta poc isola: **o modelo é o mesmo; o que muda é quem garante a
> ordem**. Em lockstep, o harness garante — e os dois resultados praticamente coincidem. Fora do
> lockstep, só a multi-thread continua garantindo.
>
> Cada poc, isoladamente, continua **bit a bit determinística** com 1, 2 e 4 threads — que é o
> que os `make check-*` verificam. A comparação **entre** as duas é outra pergunta, e a resposta
> é "iguais até ~10⁻⁶ m", não "bit a bit".

### 9.3 Em tempo real: 50 Hz contra 10 Hz

Status da multi-thread rodando de verdade (`dec` a cada 2 s):

```
[t=2s]  falcon1 ... dec=145 thr=0
[t=4s]  falcon1 ... dec=245 thr=0        +100 decisões em 2 s  =  ~50 Hz  (taxa do tcRate)
[t=10s] falcon1 ... dec=545 thr=0
```

Na single-thread essa mesma conta daria ~10 Hz — a taxa do laço de background (`bgRate` em
`app/RealTimeRun.cpp`). **A decisão ficou 5× mais frequente** sem que nenhum parâmetro do modelo
mudasse.

Efeito colateral que vale conhecer: **o que a decisão dispara acompanha a taxa.** O
`ReportAndEvade` transmite um `TacticalAlert` por decisão, então em tempo real a multi-thread
emite ~50 alertas/s onde a single-thread emitia ~10. Nada quebra (a fusão é comutativa e o
`holdTime` do datalink absorve), mas se o custo por decisão importar, **o lugar de resolver isso
é a política** — um período mínimo entre transmissões no nó da árvore —, não o agente.

### 9.4 As quatro decisões acontecem em quatro threads

```
[t=10s]
   falcon1 ... bt=PATROL   ... dec=545 thr=0
   falcon2 ... bt=SUPPORT  ... dec=545 thr=1
   falcon3 ... bt=SUPPORT  ... dec=545 thr=2
   falcon4 ... bt=SUPPORT  ... dec=545 thr=4
```

Mesmo número de decisões, **threads diferentes**: os agentes rodam dentro do pool T/C, em
paralelo, um por player — e ainda assim o dump é idêntico com 1, 2 ou 4 threads ([7.3](#73-o-paralelismo-continua-ligado)).
Os índices de thread não são contíguos porque a numeração é por ordem de primeira chamada
([`xnative::ThreadTag`](../../models/A4/include/xnative/ThreadTag.hpp)), e o laço de background também pega um
índice.

---

## 10. Qual das duas usar

| | `( SimAgent )` — single-thread | `( FlightAgentTC )` — multi-thread |
|---|---|---|
| custo de escrita | **zero** (classe do framework) | ~43 linhas + registro na factory |
| taxa de decisão | a do laço de background | a do frame (`tcRate`) |
| determinismo em tempo real | **não** | **sim**, por construção |
| determinismo em passo fixo | sim, se o laço mantiver o lockstep | sim, **independente** do laço |
| onde o agente mora | `Station`, ator por nome | dentro do player, ator = container |
| custo por frame | fora do caminho crítico | **dentro** do caminho crítico |
| frescor dos dados de background (ex.: terreno) | em fase | até um período de background de atraso |

Regra prática que sai daqui:

- **Decisão lenta e tolerante a jitter** (planejamento, lógica de missão, um agente que pensa a
  cada segundo) → `SimAgent` resolve, e mantém o trabalho fora do frame.
- **Decisão que fecha malha com a física** (é o caso desta poc: a árvore comanda o autopilot) ou
  qualquer coisa que precise ser reproduzível fora do laço de teste → `AgentTC` próprio.
- O caminho do meio existe: manter o `AgentTC` e **decimar** por dentro (decidir a cada k frames,
  contando na própria classe). Continua determinístico, e sem pagar a árvore inteira a 50 Hz.

---

## 11. Como verificar tudo

```bash
make build

# a diferença de código entre os dois subprojetos
make compare-single-multi

# determinismo com 1, 2 e 4 threads + a contagem de decisões por frame
make check-multi-thread

# tempo real (Tacview na porta 1234; Ctrl+C encerra) -- olhe 'dec' e 'thr' no status
make run-multi-thread

# a decisão está mesmo amarrada ao frame? dec tem de ser (frames + 1)
./build/src/poc/dis/multi-thread/src/multi-thread -threads 1 -deterministic 500 \
  | grep 'frame=500 player=falcon1' | grep -o 'dec=[0-9]*'

# as duas pocs, lado a lado, em passo fixo (iguais até ~1e-6 m -- ver 9.2)
./build/src/poc/dis/single-thread/src/single-thread -threads 1 -deterministic 2000 | grep 'frame=2000 '
./build/src/poc/dis/multi-thread/src/multi-thread   -threads 1 -deterministic 2000 | grep 'frame=2000 '
```

Para tudo o mais — anatomia do frame, o que vem do framework, a dissecação arquivo por arquivo, o
radar nativo, o terreno, as armadilhas do JSBSim e do gravador, a semântica ACMI do Tacview e o
controle de tempo — vale integralmente o
**[README da single-thread](../single-thread/README.md)**: aqui nada disso mudou.
