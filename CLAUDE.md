# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> Documentação, comentários de código e mensagens de console deste repositório são em
> **português do Brasil**; identificadores, nomes de slot e nomes de fábrica ficam em inglês
> (originais do MIXR). Siga essa convenção ao escrever código novo.

## O que é este projeto

Prova de conceito para desenvolver **novos modelos de simulação** sobre o framework
[MIXR](https://mixr.dev) (fork empacotado como pacote Conan `mixr/1.0.5`) e sobre o
**BehaviorTree.CPP v3** (`behaviortree.cpp.asa/3.5.6`). O MIXR **não** é o objeto de
desenvolvimento — é dependência binária.

O repositório tem hoje **dois subprojetos irmãos** em `src/`, cada um um executável
independente, e os dois compilam juntos. É o **mesmo modelo** nos dois — mesmo cenário, mesma
pilha nativa, mesmos comportamentos: a única diferença é o **agente do UBF**, isto é, *onde a
decisão roda*.

| subprojeto | agente | onde a decisão roda |
|---|---|---|
| `src/single-thread/` | `( SimAgent )` nativo, componente da **`Station`** | `updateData()`, thread de **background**: os 4 agentes decidem **em sequência**, numa thread só, a 10 Hz |
| `src/multi-thread/` | `( FlightAgentTC )` próprio, componente do **`Player`** | **fase 3** do frame, thread de **tempo crítico**: os 4 decidem **em paralelo**, um por thread do pool, a 50 Hz |

> **O nome diz onde a DECISÃO roda — não como a simulação roda.** As duas pocs declaram
> `numTcThreads` e distribuem os players pelo pool de threads de tempo crítico do framework, e
> as duas passam nos checks de determinismo com 1, 2 e 4 threads. Nenhuma delas roda a
> simulação inteira numa thread só.

`make compare-single-multi` mostra a diferença: fora o agente, as duas pastas são iguais.

Há um **terceiro subprojeto**, `src/bandit-dis/`, de natureza diferente das duas pocs irmãs
acima: não tem agente UBF nenhum, é só o `bandit1` (o intruso que as duas pocs perseguem) rodando
sozinho, num processo à parte, pilotado por joystick ou por `Autopilot` de fallback, emitindo seu
estado via **DIS nativo do MIXR** (`mixr::dis`) para quem quiser recebê-lo — hoje, `single-thread`/
`multi-thread`, que não têm mais um `bandit1:` local e o recebem só pela rede. Ver a seção própria
mais abaixo.

Antes destas duas o repositório foi uma progressão numerada (`01-flying-aircraft` …
`12-jsbsim-ubf`), citada como história ao longo dos textos ("a poc/12 fazia isso à mão"). Essas
pastas **não existem mais** e o prefixo numérico **não é mais convenção**: subprojeto novo ganha
uma pasta com nome descritivo.

O fork empacotado é **headless**: não publica `mixr_graphics`/`glut`/`instruments`/`ighost`.
Por isso não existe `GlutDisplay` aqui e toda visualização é feita por **Tacview Real-Time
Telemetry** (`shared/xtacview`).

## Build & Run

Toolchain: **Conan 2.x** → **Meson/Ninja** → **Makefile** (orquestra).

```bash
make configure   # conan install (Debug) + meson setup --reconfigure
make build       # meson compile -C build -j$(nproc) — builda TODAS as pocs
make install     # meson install -> dist/
make clean       # remove build/ e dist/
make help        # lista os alvos (comentários ## do Makefile)
```

Binários ficam em `build/src/<nome>/src/<nome>` — o executável tem o **mesmo nome da pasta** —,
e cada um tem um alvo `run-<nome>` no Makefile.

**Todos os binários leem `configs/`/`data/` por caminho relativo (`./src/<nome>/...`) e devem
ser executados a partir da raiz do repositório:**

```bash
./build/src/single-thread/src/single-thread
```

Opções de linha de comando, aceitas pelas duas pocs: `-f <arquivo>` (cenário alternativo),
`-threads <N>` (quantas threads de tempo crítico) e `-deterministic <N>` (N frames de passo fixo).

**Não há suíte de testes.** A verificação automatizada existente é de **determinismo** (alvos
`check-single-thread` e `check-multi-thread`): roda N frames de passo fixo com 1, 2 e 4 threads
T/C e compara os dumps `frame=` — todos devem ser idênticos. Vale para as **duas** pocs, a
`single-thread` inclusive: ela também roda os players no pool de threads T/C, só decide fora
dele. Os mesmos alvos servem de modelo para validar qualquer poc nova que use multithread.
O alvo `compare-single-multi` lista o que difere entre as duas pastas (deve ser só o agente).

**AddressSanitizer**: `meson configure build -Dasan=true && make build` — liga ASan apenas na
`single-thread` (único alvo que consome `asan_cpp_args`/`asan_link_args`).

## Onde consultar o framework

`contexts/` tem duas camadas: os `.md` destilados (leitura rápida) e o **código-fonte completo
das libs** em `contexts/src/` (a verdade).

**Camada 1 — destilação (`contexts/*.md`):**

| Arquivo | Responde |
|---|---|
| `contexts/MIXR-CONTEXT.md` | como o MIXR funciona por dentro (classes, macros, ciclo de vida, EDL, recorder) |
| `contexts/MIXR-PATTERN-CONTEXT.md` | como se escreve uma aplicação MIXR (padrões dos exemplos oficiais; §0 lista o que do fork **não** existe) |
| `contexts/BTCPP-CONTEXT.md` | BehaviorTree.CPP **v3.5.6** — nada vale para a v4 |

**Camada 2 — fonte (`contexts/src/`), onde confirmar qualquer coisa que a destilação não cobre
ou que pareça contraditória:**

- `contexts/src/mixr/` — árvore completa do fork **v1.0.5** (`MIXR_VERSION 170600`), a mesma
  que gera o pacote Conan consumido aqui: `src/` (313 `.cpp` — as **implementações**, que os
  headers não mostram), `include/mixr/`, `deps/{jsbsim,openrti}`, `doc/`, e
  `src/recorder/proto/DataRecord.proto` (schema do recorder). **Não tem `examples/`** — os
  padrões extraídos deles vivem só no `MIXR-PATTERN-CONTEXT.md`.
- `contexts/src/BehaviorTree.CPP/` — fonte da 3.5.6 com `examples/`, `sample_nodes/` e `tests/`.

É aqui que se responde "por que esse token não chega no handler?" ou "o que esse slot faz de
verdade": ler o `.cpp` do framework, não adivinhar pelo header. Ex.: as armadilhas do recorder
(seção do xtacview) saem de `contexts/src/mixr/src/recorder/DataRecorder.cpp`.

**Atenção:** `contexts/src/` inteiro é **git-ignored** — são cópias locais das árvores de
fonte, não vêm num clone limpo. Se a pasta não existir, caia nos headers instalados pelo Conan:
`~/.conan2/p/b/mixr*/p/include/mixr/...`, `~/.conan2/p/b/mixr*/p/include/DataRecord.pb.h` (o
`.pb.h` gerado fica na **raiz** do include, não em `mixr/recorder/`) e
`<prefix>/include/behaviortree_cpp_v3/`. Em caso de divergência entre a árvore de `contexts/src/`
e o pacote Conan, **quem vale é o pacote** — é ele que está linkado.

## Arquitetura

### Estrutura de um subprojeto

```
src/<nome>/
├── meson.build            # só faz subdir('./src')
├── configs/scenario.epp   # cenário EDL (+ .xml das árvores de comportamento)
│                          # nas duas pocs é um .epp.in — ver app/ScenarioTemplate
├── data/                  # dados vendorizados (jsbsim/, recordings/)
│                          # exceção: o tile SRTM mora em shared/data/terrain/ —
│                          # é do cenário, que é o mesmo nas duas pocs
├── include/
│   ├── app/               # as etapas da aplicação, uma questão por arquivo (ver abaixo)
│   ├── domain/            # regras de negócio puras — sem MIXR, sem BT — testável isolado
│   ├── bt/ | ubf/         # adaptadores da lib externa (nós da árvore, comportamentos UBF)
│   ├── x<nome>/           # classes MIXR próprias (namespace mixr::x<nome>) + factory própria
│   └── mixr_factory.hpp   # factory dos objetos MIXR deste subprojeto
└── src/
    ├── meson.build        # define o executable() — nome = nome da pasta
    ├── main.cpp           # FINO: só orquestra (chama os módulos de app/ na ordem)
    └── ... (espelha include/)
```

Regra geral: "o que fazer" mora em `domain/`; "como conectar" mora nas factories/adaptadores;
`main.cpp` não implementa comportamento. `src/single-thread/` é a referência completa do padrão
(a `src/multi-thread/` é a mesma árvore, trocando só o agente).

**Um arquivo, uma questão.** O que antes era um `main.cpp` de ~450 linhas está quebrado em
`app/`, no namespace `app`, e cada header abre com o "por que" daquele passo:

| módulo | questão |
|---|---|
| `app/Options.*` | `argv` → struct (`-f`, `-threads`, `-deterministic`) |
| `app/TerrainData.*` | garante o `.hgt` em disco, com o tamanho que o `SrtmHgtFile` aceita |
| `app/ScenarioTemplate.*` | `.epp.in` → `.epp` (resolve `@NUM_TC_THREADS@` antes do parse) |
| `app/StationBuilder.*` | `.epp` → `Station` de pé (`edl_parser`, `RESET_EVENT`, `WorldModel`) |
| `app/Fleet.*` | acha os players por nome e fixa a potência de cruzeiro |
| `app/StatusReport.*` | o formato da linha de status legível |
| `app/DeterministicDump.*` | o formato do dump `frame=` que os `make check-*` comparam |
| `app/DeterministicRun.*` | o laço de passo fixo |
| `app/RealTimeRun.*` | o laço de tempo real, o teclado e o `Ctrl+C` |

Vale o mesmo dentro de `xnative/` e `ubf/`: a tabela de slots do `BtBehavior` (a fronteira com o
EDL) fica em `ubf/BtBehaviorSlots.cpp` e os valores que ela ajusta em `ubf/BtTuning.hpp`,
separados do arquivo que trata da decisão; utilitários de runtime são um por questão
(`xnative/ThreadTag`, `xnative/Log`, `xnative/BehaviorBoard`).

### O modelo MIXR em uma tela

- Tudo herda de `mixr::base::Object` (ref-counting + RTTI própria):
  `DECLARE_SUBCLASS(Classe, Base)` no `.hpp`, `IMPLEMENT_SUBCLASS(Classe, "FactoryName")` no `.cpp`.
- Parâmetros configuráveis por EDL são **slots**: `BEGIN_SLOTTABLE`/`END_SLOTTABLE` +
  `BEGIN_SLOT_MAP`/`ON_SLOT`/`END_SLOT_MAP`, com `setSlotX()` privados.
- **Factory por nome**: cada `mixr_factory.cpp` encadeia as factories na ordem
  `local → xtacview → simulation → models → recorder → base` — **a primeira que retorna
  não-nulo vence**, então a factory própria vem sempre antes das do framework.
- **Estrutura vem do `.epp` (EDL), comportamento vem do C++**: reconfigurar o cenário
  (players, sensores, taxas, threads) não recompila nada.
- Hierarquia: `Station` → `WorldModel` → `players` → `Player` (`Aircraft`, `SpaceVehicle`, …)
  agregando `dynamicsModel`, sensores (`Antenna`/`RfSensor`/`Gimbal`/`Autopilot`/`Datalink`)
  e navegação (`Route`/`Steerpoint`).
- **Frame de tempo crítico por fases** — o que roda em qual fase importa e é o que permite o
  paralelismo determinístico: fase 0 `dynamics()`, fase 1 `transmit()`, fase 2 `receive()`,
  fase 3 `process()`. Decisão (tick da árvore/UBF) vai na fase 3.
- `station->updateData(dt)` no laço principal é o que **drena a fila do gravador** para a
  cadeia de `OutputHandler` — sem isso o Tacview não recebe nada.

### `shared/xtacview` — exportação para o Tacview

Biblioteca compartilhada no padrão `shared/x<nome>` do MIXR (factory própria + classes em
`mixr::xtacview`, exposta como `xtacview_dep`). **É a única exportação Tacview do repo; nenhum
`main.cpp` monta stream ACMI.** `TacviewOutput` é um `recorder::OutputHandler` de verdade,
declarado na cadeia nativa do slot `dataRecorder` da `Station`:

```
dataRecorder: ( DataRecorder
   outputHandler: ( RecorderOutputHandler
      components: {
         ( TacviewOutput port: 1234 callsign: "poc-mixr/<nome>"
           fileName: "./src/<nome>/data/recordings/mission.acmi"
           typeMap: { ... } colorMap: { ... } modelMap: { ... } )
      } ) )
```

**Armadilhas já confirmadas rodando — não redescobrir** (detalhes nos comentários de
`shared/xtacview/TacviewOutput.cpp`):

1. `dataLogTime` é slot do **`Player`** e nasce **zero**; sem `dataLogTime: ( Seconds 0.1 )` o
   player **nunca** emite `REID_PLAYER_DATA` e some do Tacview. Parece bug do handler, não é.
2. `PlayerState.pos`/`.angles` são **ECEF/geocêntricos**, não geodésicos: converter com
   `base::nav::convertEcef2Geod()` e `convertEcefAngles2GeodAngles()`.
3. `REID_NEW_TRACK` (81) **nunca chega** (degradado para `REID_UNHANDLED_ID_TOKEN`); use
   `REID_TRACK_DATA` (83) e deduza o primeiro contato pela primeira amostra de cada `track_id`.
4. `REID_WEAPON_RELEASED` (61) **aborta o processo** (bug do `DataRecorder` nativo, reproduzível
   com `TabPrinter` puro). Workaround da poc/09: `enabledList: [ 43 42 ]` — `disabledList` não basta.
5. Tokens de usuário (1000+) também não têm handler: eventos próprios são gravados como
   `REID_MARKER` (só dois `uint32`, sem texto).
6. `REID_PLAYER_DATA` traz `PlayerId` **parcial** (só `id` e `name`; sem `ac_type`/`side`) e
   `REID_NEW_PLAYER` nunca é emitido para players declarados no `.epp`. `typeMap`/`colorMap`/
   `modelMap` tentam a chave `ac_type` e caem no `name` — na prática **quem resolve é o nome**.
7. O handler **não alcança a `Station`/`WorldModel`** por `findContainerByType()` (o
   `DataRecorder` não encadeia `container()` no objeto do slot `outputHandler`).

Protocolo: handshake `XtraLib.Stream.0\nTacview.RealTimeTelemetry.0\n<username>\n\0` — **todas**
as linhas terminam em `\n`, inclusive a última (a doc oficial sugere o contrário e não conecta);
o `\0` é um byte separado. Bind em `0.0.0.0` (não `127.0.0.1`): com Tacview no Windows e binário
no WSL2 o loopback depende de localhost forwarding — se falhar, use `hostname -I`. Porta 1234
(1235 no `bandit-dis`).

**Alcançar de uma TERCEIRA máquina, na rede local (não o host, não o Windows do WSL2)** — nenhuma
das três configs declara o slot `host:`, então o `TacviewOutput` já sobe no default `"0.0.0.0"`:
o servidor já escuta em qualquer interface, o que falta é só o caminho de rede até a porta.

- **Linux nativo:** `hostname -I`/`ip addr show` no host dá o IP da LAN de verdade (não o
  interno de VM nenhuma); libere a porta no firewall se houver um ativo (`ufw allow 1234/tcp` ou
  equivalente da distro); a outra máquina conecta em `<IP-da-LAN>:1234`.
- **Binário dentro do WSL2 — tem um salto a mais.** `hostname -I` **dentro** do WSL2 devolve o IP
  interno da rede NAT da VM (tipicamente `172.x.x.x`) — **não alcançável** de outra máquina da
  LAN, mesmo com o Windows nela. É preciso encaminhar a porta no **Windows host**, como
  Administrador:
  ```powershell
  netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=1234 connectaddress=<IP-WSL2> connectport=1234
  New-NetFirewallRule -DisplayName "Tacview WSL2" -Direction Inbound -Protocol TCP -LocalPort 1234 -Action Allow
  ```
  (conferir com `netsh interface portproxy show v4tov4`); a terceira máquina conecta no IP da LAN
  **do Windows**, não do WSL2. **Armadilha:** o IP interno do WSL2 muda a cada reinício da
  VM/máquina — o `portproxy` fica apontando para um IP morto. Refazer o `netsh interface
  portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=1234` seguido do `add` acima sempre
  que o WSL2 reiniciar.

### `shared/xclock` — controle de velocidade do tempo (acelerar / frear / pausar)

Mesmo padrão `shared/x<nome>` do `xtacview` (factory própria, classes em `mixr::xclock`, exposta
como `xclock_dep`). O cenário declara **`( ClockStation )` no lugar de `( Station )`** — é uma
`simulation::Station` com um único override. Usada pelas duas pocs; trocar de volta para
`( Station )` continua rodando, só sem as teclas (o `main.cpp` avisa e segue).

Teclas (em `TimeControls`): `+`/`=` acelera, `-`/`_` freia, `espaço`/`p` pausa, `1` volta a
tempo real, `h` ajuda. Escala em degraus `0.10x … 64x`. A linha de status mostra tempo de parede
e tempo simulado lado a lado (`[t=24s sim=8.2s PAUSADO (1x)]`) — é a diferença entre os dois que
prova o efeito.

**A divisão entre nativo e próprio é deliberada:**

- **Acelerar é 100% nativo.** `Station::processTimeCriticalTasks()` já faz
  `for (jj=0; jj < getFastForwardRate(); jj++) tcFrame(dt);` (`Station.cpp:506-511`), e
  `setFastForwardRate()` é público e virtual. Nada foi escrito para isso.
- **Frear não existe no framework** — `fastForwardRate` é `unsigned int` (só multiplica) e não
  há setter público de `tcRate` em runtime (o rate da `base::PeriodicThread` é fixado na
  construção). É a **única** coisa acrescentada: abaixo de `1x`, um `tcFrame(dt * fator)` com o
  `dt` encurtado. Passo de integração menor, nunca maior — não degrada o JSBSim.
- **Pausar é nativo, por um caminho não óbvio.** Não existe `Simulation::pause()`; o que existe
  é o flag de freeze do `base::Component`. **Ele não se propaga para os filhos** — a cascata é
  por *consulta*, no sentido inverso: `Player::isFrozen()` testa o próprio flag **ou** o da
  simulação (`Player.cpp:445-448`), `System::isFrozen()` testa o próprio **ou** o do ownship
  (`System.cpp:52-56`), e `Player::dynamics()` repassa ao `DynamicsModel` (`Player.cpp:2773`),
  que põe a JSBSim em hold (`JSBSimModel.cpp:657`). Por isso `setPaused()` age em
  `getSimulation()`, **não** na `Station`.

**Armadilha confirmada rodando — não redescobrir:** marcar o freeze **não para o relógio de
execução**. `Simulation::updateTC()` faz `execTime += dt` na **linha 462**, *antes* do
`if (isFrozen()) dt0 = 0.0` da linha 498, e com o `dt` cru. Medido: mundo parado, `sim=` ainda
subindo. Vazaria para o Tacview, que data cada linha ACMI com `exec_time`
(`TacviewOutput.cpp:373`) — replay avançando com as aeronaves paradas. Correção: quando pausado,
**não chamar `tcFrame()`**. O flag de freeze continua marcado porque é ele que congela o *outro*
caminho, o de background (`Simulation::updateData()`, linha 625), que não passa por
`processTimeCriticalTasks()`.

**Limite conhecido:** `ubf::Agent::updateData()` chama `controller(dt)` sem consultar
`isFrozen()` (`Agent.cpp:59-62`) — com a simulação pausada, o `SimAgent` da poc/single-thread continua
avaliando sobre um mundo estático (nada se move; a decisão só não para). O `FlightAgentTC` da
poc/multi-thread decide na fase 3, dentro do frame, então para junto.

`-deterministic` **não é afetado**: chama `station->tcFrame(dt)` direto, sem passar por
`processTimeCriticalTasks()`. `make check-single-thread`/`check-multi-thread` seguem valendo.

Sem TTY (pipe, redirecionamento, CI) o `tcgetattr()` de `ConsoleKeyboard` falha, `isActive()`
fica `false` e a simulação roda normalmente, só sem teclado.

### `shared/xjoystick` — controle do `bandit1` (ownship) por joystick físico

Mesmo padrão `shared/x<nome>` do `xtacview`/`xclock` (factory própria + classes em
`mixr::xjoystick`, exposta como `xjoystick_dep`). Usa só mecanismo **nativo** do MIXR — o
`mixr::linkage` (`IoHandler`/`IoData`/`IoDevice`/adapters), já parte da `mixr_dep` via
`Requires: mixr-linkage` do `mixr.pc` — e o `UsbJoystick` nativo (Linux), que lê
`/dev/js%d`/`/dev/input/js%d` com `<linux/joystick.h>` cru (ioctl + `read()` não bloqueante).
**Nenhuma dependência nova** (nem SDL, nem evdev) e nada no Conan mudou.

**Desde que o `bandit1` virou o processo `src/bandit-dis` (ver a seção própria mais abaixo), é lá
que o `ioHandler:` mora** — `single-thread`/`multi-thread` não declaram mais nenhum. O cenário
declara o dispositivo no slot **nomeado e específico** que `simulation::Station` já tem para
isso — `ioHandler:` (`Station.hpp:34`, mesmo padrão do `dataRecorder:` do xtacview):

```
ioHandler: ( JoystickIoHandler
   player: "bandit1"
   inputData: ( IoData numAI: 4 )
   devices: {
      ( UsbJoystick deviceIndex: 0
         adapters: {
            ( AnalogInput ai: 1  channel: 0 )                          // ROLL_AI
            ( AnalogInput ai: 2  channel: 1 )                          // PITCH_AI
            ( AnalogInput ai: 3  channel: 2 )                          // RUDDER_AI
            ( AnalogInput ai: 4  channel: 3  offset: 1.0  gain: -0.5 ) // THROTTLE_AI
         }
      )
   }
)
```

Os 4 `channel:` acima são os do **Logitech Extreme 3D** (6 eixos/12 botões,
`deviceIndex: 0`), confirmados com `tools/joystick_mapper.py` (script Python temporário, fora do
build — mesmo protocolo cru do `UsbJoystick_linux.cpp`, então o canal que ele mostra é o mesmo
que vai no `channel:` do EDL). Trocar de joystick é só remapear estes 4 números.

`JoystickIoHandler` (`shared/xjoystick/JoystickIoHandler.hpp`) é a subclasse concreta de
`linkage::IoHandler` que a aplicação tem de fornecer (o framework não traz uma pronta —
`inputDevicesImpl(dt)`/`outputDevicesImpl(dt)` são os dois únicos métodos a sobrescrever). Ela
lê os canais do `IoData` e aplica em `AirVehicle::setControlStick()`/`setRudderPedalInput()`/
`setThrottles()` do player nomeado no slot `player:`.

`app/RealTimeRun.cpp` (de `bandit-dis`) sonda o handler no mesmo lugar e na mesma taxa do
`xclock::TimeControls::poll()` das outras pocs (laço de background, 10 Hz) —
`ioHandler->inputDevices(dt)`, chamado só se o cenário declarou `ioHandler:` (`nullptr` é aviso,
não erro fatal, mesmo raciocínio do `clockStationOf`).

**Armadilhas confirmadas — não redescobrir:**

1. **`bandit1` já tem `pilot: ( Autopilot headingHoldMode/altitudeHoldMode/velocityHoldMode:
   true )`**, e o `Autopilot` reimpõe esses modos a cada fase do frame de tempo crítico (até
   50 Hz) — muito mais rápido que a sondagem do joystick (10 Hz). Escrever o stick só no
   `AirVehicle` sem desengatar os hold modes faz o `Autopilot` sobrescrever de volta antes da
   próxima leitura. Por isso `JoystickIoHandler::inputDevicesImpl()` localiza o `Autopilot` do
   player (`Player::getPilotByType`) e desliga os três hold modes antes de aplicar o stick
   direto no `AirVehicle` — não pelo `Autopilot`. **Mas só quando há joystick de verdade** — ver
   armadilha 7.
2. **Numeração dos canais é assimétrica**: `ai:`/`di:` (canal lógico do `IoData`) é **1-based**
   (`IoData.hpp:19-21`); `channel:` (canal físico do dispositivo) é **0-based**. Os números do
   `ai:` têm de bater com `shared/xjoystick/ChannelMap.hpp` — repetidos no `.epp.in` com
   comentário, não por `#include`: este fork do parser EDL não roda o pré-processador C (mesmo
   motivo já registrado no comentário do padrão de ganho do radar, mais acima neste arquivo).
3. **Sem hardware, o `UsbJoystick` degrada sozinho** — `UsbJoystick::reset()` (Linux) só loga
   `"UsbJoystick::reset(): Joystick device not found"` em `stderr` quando `/dev/input/jsX` não
   existe; os canais ficam em zero e `getAnalogInput()` retorna `false`, sem exceção/abort. Mas
   essa degradação é **muda** — quem chama `getAnalogInput()` não descobre que o valor é "zero
   porque não há dispositivo" e não "zero porque o piloto centralizou o manche". Ver a armadilha
   7 para o porquê disso importar.
4. **Sinal do manete do Extreme 3D é invertido em relação ao `setThrottles()` nativo** —
   confirmado rodando: o eixo 3 (slider) sai em `-1.0` no batente de **potência plena** e `+1.0`
   no de **cutoff**, enquanto `Player::setThrottles()` espera `0.0` (idle) a `1.0` (plena
   potência) — faixa **unidirecional**, ao contrário de roll/pitch/pedal (`-1..1`, sem
   transformação nenhuma: o sinal do device já bate com o do MIXR). É por isso que só o `ai: 4`
   (`THROTTLE_AI`) leva `offset: 1.0 gain: -0.5` — o `AnalogInput` calcula
   `t = (raw - offset) * gain`, e essa combinação inverte E reescala de `[-1,1]` para `[0,1]` no
   mesmo passo (raw=-1 → t=1.0; raw=+1 → t=0.0). Ver `AnalogInput.hpp:16-35` para a fórmula.
5. **WSL2 não repassa USB por padrão.** O binário é o mesmo nos dois ambientes; o que muda é
   operacional: em WSL2 é preciso `usbipd-win` no host Windows
   (`usbipd attach --wsl --busid <id>`) para o joystick aparecer em `/dev/input/js*` dentro da
   VM. Em Linux nativo basta o módulo de kernel `joydev` carregado (a maioria das distros já
   carrega ao conectar o dispositivo).
6. `mixr::linkage::factory` **não** é encadeada por nenhuma das outras factories nativas
   (`simulation`/`models`/`terrain`/`recorder`) — sem `mixr::linkage::factory(name)` no
   `mixr_factory.cpp` de cada poc, o `devices: { ( UsbJoystick ... ) }` do `ioHandler:` não
   constrói nada, em silêncio (mesma armadilha do `mixr::terrain::factory`, documentada acima).
7. **Fallback gracioso para o `Autopilot`, adicionado quando o `bandit1` virou `src/bandit-dis`
   (voando sozinho, sem as outras aeronaves por perto para "segurar" o cenário se ninguém
   pilotasse).** Como a armadilha 3 registra, `IoData::getAnalogInput()` não distingue "sem
   dispositivo" de "manche centralizado" — sem tratar isso à parte, um `bandit1` sem joystick
   físico ficaria voando em manual com entradas zeradas (manete na metade) em vez de manter o
   `Autopilot` scripted. `JoystickIoHandler` ganhou um slot próprio `deviceIndex` (tem que bater
   com o `deviceIndex:` do `( UsbJoystick )` dentro de `devices:`) e um `hasRealJoystick()` que
   confere a EXISTÊNCIA do arquivo de dispositivo ele mesmo (`mixr::base::doesFileExist()`,
   mesma ordem de busca do `UsbJoystick_linux.cpp`: `/dev/js<N>` depois `/dev/input/js<N>`) —
   sem o arquivo, `inputDevicesImpl()` retorna sem tocar em nada, e o `Autopilot` segue no
   controle exatamente como ficaria sem nenhuma seção `ioHandler:`. A checagem roda todo frame
   (um `stat()`, custo desprezível): plugar o joystick no meio de uma execução já em andamento
   troca para controle manual sem reiniciar nada — testado rodando.

### `shared/xlog` — sistema de log `LOG(NIVEL) << ...;` persistido em arquivo

Mesmo padrão `shared/x<nome>` das outras libs, com uma diferença: **sem `factory.cpp`**. Não há
nada aqui para o parser EDL construir — `mixr::recorder::PrintHandler` (o sink por trás do log)
é instanciado direto em C++, nunca aparece num `.epp`.

**Por que não é o `mixr::recorder` "de verdade" (`DataRecorder`/`OutputHandler`/`recordData()`)
— investigado antes de escrever uma linha de código.** O schema `DataRecord.proto` é fechado:
nenhuma mensagem por-evento tem campo de texto livre (nem o `MarkerMsg`, a mais próxima — só
`id`/`source_id`, dois `uint32`; confirmado em `DataRecorder::recordMarker()`,
`DataRecorder.cpp:180-197`), e o único ponto de entrada público do gravador,
`AbstractDataRecorder::recordData(id, base::Object* pObjects[4], double values[4])`
(`AbstractDataRecorder.hpp:39-43`), não tem overload de string. Um token de evento próprio
(1000+) sem handler registrado nem preserva os dois `uint32` do marker — cai em
`processUnhandledId()` (`DataRecorder.cpp:118-134`) e vira `UnknownIdMsg{id}`, perdendo tudo.
Carregar texto livre por ali exigiria remendar o `.proto` vendorizado do MIXR (`extend
DataRecord {...}` nos campos reservados 1000-9999, regenerar o `.pb.cc` do pacote Conan) —
invasivo e contra a premissa do projeto de que o MIXR é dependência binária, não objeto de
desenvolvimento.

**O que É reaproveitado do recorder, então:** `mixr::recorder::PrintHandler`
(`include/mixr/recorder/PrintHandler.hpp`, base de `TabPrinter` e companhia) — mas usado **por
fora** do pipeline `recordData()`/REID/protobuf. `printToOutput(const char*)`
(`PrintHandler.cpp:273-291`) escreve direto num `std::ofstream` que ele mesmo abre (preguiçoso,
no primeiro uso), configurado por `setFilename()`/`setPathName()` — métodos públicos comuns, não
só slots de EDL: dá para `new mixr::recorder::PrintHandler()` direto em C++, sem `Station`, sem
factory. `processRecordImp()` (o método que receberia um `DataRecordHandle` do pipeline) é
no-op vazio na classe base — usado assim, isolado, o `PrintHandler` nunca esbarra no schema
fechado. Já é dependência transitiva de `mixr_dep` (`mixr-recorder` no `Requires:` do `mixr.pc`,
a mesma lib que o `xtacview` já linka) — nenhuma dependência nova.

**Uso:**
```cpp
#include "xlog/Log.hpp"

LOG(WARNING) << "algo aconteceu: " << valor;
```
`Level` é `enum class` (`DEBUG`/`INFO`/`WARNING`/`ERROR`), não `#define`s soltos — evita colisão
com macros de sistema (`ERROR`/`DEBUG` são armadilhas clássicas no Windows; irrelevante aqui,
mas o desenho já nasce sem essa pegadinha). `mixr::xlog::Stream` é o objeto RAII por trás da
macro: acumula em `operator<<` e escreve tudo — console **e** arquivo — no destrutor.

**Armadilhas confirmadas — não redescobrir:**

1. **`data/logs/` precisa existir no disco antes do `init()`** — `PrintHandler::openFile()` não
   cria diretório, só abre o `ofstream`; sem o diretório, falha em silêncio exatamente como o
   `TacviewOutput` falha hoje sem `data/recordings/` (`"falha ao abrir .../mission.acmi para
   gravacao"`, mesma causa). Por isso `data/logs/.gitkeep` existe em cada poc — diretório vazio
   não versiona em git.
2. **Mutex único em `Stream::~Stream()`** — a poc `multi-thread` decide em paralelo, um
   `FlightAgentTC` por thread do pool de tempo crítico; sem lock as linhas se entrelaçam no
   `std::ofstream` (que não é thread-safe sozinho) exatamente como o `xnative::Log` antigo já
   precisava de mutex só para o console. Testado com um `treeFile:` inexistente nas duas pocs:
   4 linhas limpas, sem entrelaçamento, no console e no arquivo.
3. **`-deterministic` desliga o log** (`xlog::setLoggingEnabled(false)`, chamado em `main.cpp`
   quando `opts.isDeterministic()`) — linhas de log carregam timestamp de parede, fora do modo
   comparável; mesmo raciocínio que já existia para o `xnative::Log` (hoje substituído por
   este). `make check-single-thread`/`check-multi-thread` não são afetados.
4. **Substituiu `xnative::Log`** (`logLine(string)` + `setLoggingEnabled(bool)`, duplicado à mão
   nas duas pocs, só console, sem nível) — os 2 pontos de uso (`ubf/BtBehavior.cpp`, mensagem
   vazia/exceção ao carregar a árvore) migraram para `LOG(WARNING)`/`LOG(ERROR)`.

### `src/bandit-dis` — o `bandit1` num processo próprio, emitindo DIS nativo do MIXR

Terceiro subprojeto, de natureza diferente dos dois primeiros: não é uma pilha nova nem um
agente novo, é **onde o `bandit1` mora agora** — antes um player local em `single-thread`/
`multi-thread`, hoje um processo à parte, pilotado por joystick físico (`shared/xjoystick`, com
fallback pro `Autopilot` scripted — armadilha 7 da seção `xjoystick` acima) e **emitido via DIS
nativo do MIXR** (`mixr::dis` — namespace real da lib, apesar do caminho do header ser
`mixr/interop/dis/`) para quem quiser recebê-lo. `single-thread`/`multi-thread` não têm mais um
`bandit1:` local: recebem esse player **só pela rede**, via `networks:`, exatamente como
qualquer outra prova de interoperabilidade DIS de verdade — duas ou mais instâncias separadas,
não um truque de processo único.

**É bidirecional.** O `bandit-dis` também **recebe** falcon1..4 (que as duas pocs gêmeas emitem
de volta) — o próprio Tacview do `bandit-dis` (porta 1235) mostra as quatro falcons, não só o
`bandit1`. Cada `DisNetIO` só precisa dos dois blocos que fazem sentido pro seu lado:
`outputEntityTypes:` pra publicar os players locais, `inputEntityTypes:` pra materializar os da
rede — os dois usam o **mesmo** `disEntityType`, porque `bandit1` e `falcon1..4` são a mesma
classe/`type:` (`Aircraft`/`"C310"`) e o casamento de entrada é só pelo código numérico do fio,
não pela string — não há ambiguidade porque cada `DisNetIO` só ouve tráfego de quem só emite UMA
coisa (`bandit-dis` só emite `bandit1`; cada poc gêmea só emite `falcon1..4`).

**Por que dá pra confiar que o radar/UBF das falcons reage a um contato que só existe na rede —
investigado antes de desenhar isto, não depois de quebrar:**
`interop::NetIO::createIPlayer()` (`contexts/src/mixr/src/interop/common/NetIO.cpp:639-711`), ao
receber o primeiro PDU de uma entidade nova, clona o `template:` do `Ntm` que casou o tipo —
`templatePlayer->clone()`, um clone **completo** via `Player::copyData()`/`Component::copyData()`
(`Player.cpp:273-404`, `Component.cpp:70-99`): `signature` (`SigSphere`), `dataLogTime`, tudo
sobrevive. Só posição/atitude são sobrescritas na criação e depois mantidas por *dead reckoning*
a cada PDU (`Player::deadReckonPosition()`, `Player.cpp:3084-3094`). O clone entra na **mesma**
lista que `Simulation::getPlayers()` devolve (`NetIO.cpp:699-700`, `addNewPlayer()`) — a mesma
que `AirTrkMgr`/`Antenna` já varriam para achar o `bandit1` nativo. **Nenhum player local
precisa existir no lado receptor** — só o `Ntm` com o `template:` já basta
(`Ntm::getTemplatePlayer()`, `Ntm.hpp:69`). `dynamicsModel`/`pilot` do template do lado receptor
não precisam reproduzir `JSBSimModel`/`Autopilot`: a posição do fantasma nunca é simulada ali, só
*dead reckoning* — `side:` também é irrelevante, quem manda é o Force ID do próprio PDU
(`Nib_entity_state.cpp:463-469`), sobrescrito logo após o clone (`NetIO.cpp:684`).

**Testado rodando, ponta a ponta, nas duas pocs, nos dois sentidos**: `bandit-dis` sozinho (sem
joystick — o `Autopilot` de fallback mantém `hdg=225` scripted) + `single-thread`/`multi-thread`
cada um por vez → as falcons produzem `pista=bandit1@13.6NM`, `bt=EVADE`/`alerta<-falcon1(bandit1)`
se propagando, `bt=SUPPORT`, depois `bt=BREAK` — a cadeia UBF/BehaviorTree inteira reagindo a uma
aeronave que não existe em processo nenhum além do `bandit-dis`. **E na volta**: a gravação
`.acmi` do `bandit-dis` mostra os cinco `CallSign=` (`bandit1` + `falcon1..4`) — confirmando que
as falcons chegaram por DIS, não só o intruso. O nome de cada fantasma saiu **exatamente** igual
ao do player original (aparentemente do campo Marking do PDU), então o `modelMap`/`typeMap`/
`colorMap` do `TacviewOutput` de cada poc — incluindo as entradas `falcon1..4` acrescentadas no
`bandit-dis` — não precisou de ajuste nenhum além de existir.

**Esquema de portas** (mesma receita do exemplo do próprio MIXR — `MIXR-PATTERN-CONTEXT.md`
§6.7 — estendida para 3 processos no mesmo host): todo mundo **escuta** em `3000`; cada processo
**emite** de uma porta local diferente e ignora essa mesma porta como origem
(`ignoreSourcePort:` == o próprio `localPort:`), pra ninguém ouvir o próprio eco.
`bandit-dis`: `3001`. `single-thread`: `3002`. `multi-thread`: `3003`. Os três `DisNetIO`
declaram `outputEntityTypes:` **e** `inputEntityTypes:` — nenhum é receive-only — mas
`netOutput:`/`netInput:` são obrigatórios de qualquer forma, mesmo num `DisNetIO` puramente
receptor (ver armadilha 2), já que `initNetwork()` inicializa os dois incondicionalmente.
`disEntityType:` é um código de 7 números inventado (não é enumeração SISO-REF-010 real) — só
precisa ser **idêntico** nos dois lados de cada par emissor/receptor.

**Armadilhas confirmadas rodando — não redescobrir:**

1. **`DisNetIO`/`DisNtm` são construtíveis direto em EDL** — ao contrário do
   `linkage::IoHandler` do `xjoystick` (abstrato, exigiu a subclasse `JoystickIoHandler`),
   `dis::NetIO`/`dis::Ntm` já sobrescrevem todos os métodos virtuais puros da base (factory
   names `"DisNetIO"`/`"DisNtm"`, `dis/factory.cpp:20-22`). Nenhum `.cpp` de classe MIXR nova
   foi escrito para esta PoC — só EDL e reaproveitamento do `shared/xtacview`.
2. **`initNetwork()` inicializa `netInput`/`netOutput` incondicionalmente** — os dois `netInput:`/
   `netOutput:` têm que ser `NetHandler`s válidos mesmo que um `DisNetIO` só use um dos sentidos
   (confirmado rodando: sem os dois presentes, a inicialização falha). Aqui os três processos
   acabaram sendo bidirecionais de qualquer forma (ver acima), mas a armadilha vale igual para
   um `DisNetIO` deliberadamente unidirecional.
3. **Processamento de rede roda dentro de `updateData()`, não do frame de tempo crítico** —
   `Station::processNetworkInputTasks()`/`processNetworkOutputTasks()` são chamadas de dentro de
   `updateData()` (ou de uma thread própria, só se o slot `netRate` pedir uma > 0); como o laço
   de tempo real de todas as pocs já chama `station->updateData(dt)` a cada frame de
   background, **nenhuma chamada nova foi necessária** — nem em `bandit-dis`, nem nas outras.
   `createNetworkProcess()` existe (`Station.hpp:248`) mas não é preciso chamá-lo aqui.
4. **`mixr::dis::factory` não é encadeada por nenhuma outra factory nativa** — mesma armadilha
   do `terrain`/`linkage` já documentada acima; sem `mixr::dis::factory(name)` no
   `mixr_factory.cpp` de cada poc (as três, incluindo `bandit-dis`), `networks: ( DisNetIO
   ... )` não constrói nada, em silêncio.
5. **Comentários em `.epp`/`.epp.in` têm que ser ASCII puro.** Descoberto quebrando: um único
   caractere acentuado (`"só"`) dentro de um comentário `//`, em outro ponto do arquivo, sem
   relação nenhuma com o texto ao redor, fez o `edl_parser` (bison/flex) recusar o arquivo
   inteiro com `"syntax error"` — apontando a linha certa, mas sem dizer o motivo. É por isso
   que toda a base já escreve "não"/"está"/"é" sem acento em comentário: não é só estilo, é
   requisito do parser. Confirmado: `scenario.epp.in` das duas pocs gêmeas são ASCII puro
   (`file` confirma); o arquivo desta poc também é, depois da correção.
6. **`applyCruiseThrottle` tem que ser replicado aqui** — o mesmo problema documentado em
   `app/Fleet.hpp` das pocs gêmeas (o autopilot do c310 fecha malha de rumo/altitude mas não de
   velocidade; sem manete fixo a aeronave perde velocidade e estola) se aplicava ao `bandit1`
   antes também — ele estava na `Fleet` das duas pocs e recebia a mesma correção. Como
   `bandit-dis` não tem `Fleet` (um player só), `main.cpp` aplica `setThrottles(0.95, 1)` uma vez
   direto no `bandit1`, achado por nome via `getPlayers()`.

### Terreno (elevação) — `mixr_terrain`, e o que ele muda no modelo

O banco de elevação é **100% nativo**: `libmixr_terrain.so` já vinha linkado (o `mixr.pc` do
Conan lista `mixr-terrain` em `Requires:`), o `WorldModel` já tinha o slot `terrain`, e o
`Player` já tinha `getTerrainElevationM()`/`getAltitudeAglM()`/`updateElevation()`. **Nada foi
escrito do lado do framework e nenhuma linha de meson mudou.** O que faltava eram três coisas:

1. **A factory.** `models::factory` **não** encadeia a de terreno — sem
   `mixr::terrain::factory(name)` no `mixr_factory.cpp`, o `( SrtmHgtFile )` do `.epp` não
   constrói nada e o `WorldModel` fica sem terreno, em silêncio.
2. **O dado.** `shared/data/terrain/srtm/S23W043.hgt.gz` — tile SRTM1 da Serra do Mar (RJ),
   recuperado do histórico do git (era da `poc/05-formation-flight`). Fica em `shared/` e não
   em `src/<nome>/data/` de propósito: são 12 MB do **cenário**, que é o mesmo nas duas pocs
   gêmeas — é a única exceção à regra de `data/` por subprojeto.
3. **A ponte até a decisão.** `ubf::FlightState::updateState()` copia `terrainElevM`,
   `altitudeAglM` e `terrainValid` para o `Snapshot`; daí em diante é regra pura em
   `domain/TerrainFloor.hpp`, consumida pela `domain::ThreatPolicy` (piso da evasão) e pelo
   `xnative::AltitudeSafetyBehavior` (piso AGL).

**Armadilhas confirmadas lendo o fonte — não redescobrir:**

1. **`Player::updateElevation()` ignora o retorno de `getElevation()`**
   (`Player.cpp:3205-3206`). Fora da célula do tile, `el` fica `0.0` e
   `setTerrainElevation(0.0)` liga `tElevValid = true`. **`isTerrainElevationValid()` não é
   guarda de cobertura.** Por isso o piso de `domain/TerrainFloor.hpp` mantém uma camada
   absoluta embaixo da camada de terreno, e por isso o cenário fica no miolo da célula.
2. **`getAltitudeAgl()` não consulta `tElevValid`** (`Player.inl:262-266`): sem banco
   carregado, AGL == altitude HAE, silenciosamente.
3. **Os slots são `path` e `file`** — não `pathname`/`filename`, que é como se chamam os
   setters (`Terrain.cpp:24-32`). Nome errado dá `slot not found` e o tile não carrega.
4. **`SrtmHgtFile` não lê `.gz`** e valida o **tamanho exato em bytes** (2.884.802 = SRTM3,
   25.934.402 = SRTM1; `SrtmHgtFile.cpp:154-207`). Qualquer outro tamanho falha com
   *"ERROR in determining SRTM type"*, sem dizer qual arquivo. O nome é lido por **posição fixa
   nos últimos 11 caracteres** (`S23W043.hgt`). `app/TerrainData` descomprime e **confere o
   tamanho**, que é o antídoto para o diagnóstico inútil.
5. **`CRASH_EVENT` deixa de ser letra morta.** `Player.cpp:2811` dispara com `AGL < 0`, e
   `crashNotification()` (`Player.cpp:2451-2484`) faz `setMode(CRASHED)` e manda `KILL_EVENT`
   a todos os subcomponentes — o avião **congela e para de decidir**, porque `updateTC`/
   `updateData` só rodam com `mode == ACTIVE`. Hoje isso nunca acontecia porque `tElev` era
   sempre 0. É a razão de a altitude de cada falcon sair do **pico do próprio circuito + 300 m**.
   Escape hatch, se algum dia precisar: `crashOverride: true` no player (slot nativo 26).
6. **`terrainElevReq` tem de continuar `false`** (default). Com `true`, `updateElevation()`
   **pula** a consulta ao banco e fica esperando um gerador de imagem externo empurrar o valor.
7. `getMinElevation()`/`getMaxElevation()` do `SrtmHgtFile` estão **errados** — refletem só a
   última linha lida (`SrtmHgtFile.cpp:232-233,253-254`). O `DtedFile` tem o mesmo defeito por
   coluna. Não usar esses dois.
8. `QuadMap` aceita **no máximo 4** filhos e descarta o excedente **em silêncio**; e
   `QuadMap::clone()` devolve `nullptr` (`IMPLEMENT_ABSTRACT_SUBCLASS`), o que faz
   `WorldModel::copyData()` estourar. Aqui se usa um `SrtmHgtFile` direto — nenhum dos dois
   se aplica.
9. `WorldModel::reset()` imprime `"Loading Terrain Data..."` em `stdout`, incondicionalmente.
   Inofensivo: os `check-*` filtram por `grep '^frame='`.
10. **Não acrescentar tokens ao `enabledList: [ 43 42 ]`** do `dataRecorder`.
    `crashNotification()` grava `REID_PLAYER_CRASH`; mantê-lo fora da lista mantém o handler
    nativo (da mesma família dos que estouram) fora do caminho.

**Onde a elevação é consultada, e o que isso implica:** `Player::updateElevation()` roda em
`updateData()` (`Player.cpp:630`), ou seja na fase de **background** — não numa das quatro
fases do frame de tempo crítico. Na `multi-thread`, que decide na fase 3 a 50 Hz contra um
background de 10 Hz, o valor pode estar até 100 ms velho (~8 m percorridos). Continua
determinístico: em `-deterministic` o laço faz `tcFrame()` e `updateData()` em sequência no
mesmo passo, e `check-single-thread`/`check-multi-thread` passam com 1, 2 e 4 threads T/C.

**Limite medido, e vale saber antes de ajustar o cenário:** quem limita a manobra é a
**aeronave**, não o terreno. Com `maxClimbRateMps: 8.0` e `evadeHold: 30 s`, o C310 desce
~330 m por engajamento. Para o piso anti-CFIT ser alcançado, `terrainClearance` tem de ficar
**dentro de ~330 m do AGL de cruzeiro** — daí a folga de 800 m contra um cruzeiro de ~930 m
AGL. Com folgas "bonitas" (300–500 m) o piso está correto e roda, mas recorta um alvo que a
aeronave nunca alcançaria: o dump sai idêntico ao do controle negativo.

## Ao adicionar um subprojeto novo

1. Criar `src/<nome>/` — nome descritivo, sem prefixo numérico, e é ele que vira o nome do
   executável — seguindo a estrutura acima; sempre com `include/mixr_factory.hpp` +
   `src/mixr_factory.cpp` (a factory **não** fica inline no `main.cpp`).
2. Adicionar `subdir('./<nome>')` em [src/meson.build](src/meson.build) e o alvo no `summary()`
   de Build Artifacts do [meson.build](meson.build) raiz.
3. Adicionar o alvo `run-<nome>` no [Makefile](Makefile) — apontando para
   `$(BUILD_DIR)/src/<nome>/src/<nome>`.
4. **Declarar o rpath no `executable()`**: `link_args: rpath_link_args` +
   `install_rpath`/`build_rpath` com `mixr_libdir`, `bt_libdir`, `mixr_bt_libdir` ou
   `mixr_bt_jsbsim_libdir` conforme as dependências usadas (variáveis definidas no `meson.build` raiz).
5. Para exportar ao Tacview: `dataRecorder:` na `Station` com `( TacviewOutput ... )`,
   `dataLogTime:` em **cada** player, `mixr::xtacview::factory` + `mixr::recorder::factory`
   encadeadas no `mixr_factory.cpp`, e `xtacview_dep` no `meson.build`.

### Gotcha: rpath das dependências Conan (`dist/` vs `build/`)

As `.so` do MIXR/BT.CPP/JSBSim vivem no cache do Conan, fora de qualquer caminho do loader.

- **O Meson descarta o `build_rpath` no `meson install`** (por design): um alvo sem
  `install_rpath` roda de `build/` e falha em `dist/bin/<nome>` com
  `error while loading shared libraries`.
- **`-Wl,--disable-new-dtags` (em `rpath_link_args`) não é decorativo**: força `DT_RPATH` em vez
  de `DT_RUNPATH`. RPATH é herdado pelas dependências transitivas, RUNPATH não — sem ele o loader
  resolve as libs nomeadas no executável mas falha entre elas (`libmixr_models.so` → `libmixr_base.so`).

Conferência após `make install`: `ldd dist/bin/<nome> | grep 'not found'` (silêncio = ok) e
`readelf -d dist/bin/<nome> | grep -E 'RPATH|RUNPATH'`.

`dist/` **não é auto-contido** (rpath com hash do cache Conan + configs por caminho relativo).

### Gotchas de unidades e de modelo

- `DynamicsModel::setCommandedAltitude()` espera **metros** (converter pés com
  `base::distance::FT2M`); `setCommandedHeadingD()` é graus; `setCommandedVelocityKts()` é nós.
- Os `setCommanded*` só funcionam com `RacModel`. O `F4N` do JSBSim não tem autopilot próprio —
  comandar via `setControlStick(roll, pitch)` + `setThrottles(...)` (entradas normalizadas -1..1).
- `JSBSimModel::reset()` chama `RunIC()` mas **não** roda `FGTrim`: a aeronave começa destrimada e
  há transiente energético nos primeiros segundos.

## Estado atual / pendências conhecidas

- A renomeação `poc/` → `src/` foi propagada aos caminhos de arquivo (defaults dos `main.cpp`,
  `.epp`/`.epp.in` e alvos do `Makefile`). **Comentários e banners de console ainda dizem
  `poc/<nome>`** — é só prosa, nenhum caminho depende disso.
- `docs/` está vazio.
- `build/`, `dist/` e `contexts/src/` não são versionados (`.gitignore`); `build/` já foi
  destrackeado com `git rm -r --cached`.
- Limitação conhecida na poc/09: chaff/flare saem no Tacview como `Misc`/`Grey` em vez de
  `Misc+Decoy+Chaff`/`+Flare` — soma das armadilhas 4, 6 e 7 do xtacview.
