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

**São TRÊS projetos Meson, em três diretórios de build, e a ordem é obrigatória.** O modelo
(`domain/`, `bt/`, `ubf/`, `xnative/`) não é mais um alvo do host: é um plugin construído numa
etapa **anterior**, e o host só consome o `.so` instalado.

```bash
make configure   # conan install (Debug) + meson setup do HOST -> build/
make sdk         # publica o SDK em dist/{include,lib,lib/pkgconfig}
make models      # projetos à parte -> build-flight/, build-missile/ e build-stub/ -> dist/lib/mixr-plugins/
make build       # o HOST (depende de models, que depende de sdk) -> build/
make install     # meson install -> dist/
make test        # as DUAS suítes: a do modelo e a do host
make clean       # remove os três build/ e dist/
make help        # lista os alvos (comentários ## do Makefile)
```

`make build` já dispara `models` e `sdk`, então o fluxo normal continua sendo
`make configure && make build`. Os alvos separados existem para quando se quer refazer só uma
etapa — e é o `make models` que se roda ao mexer no modelo.

**Três armadilhas do Meson que a etapa do SDK esconde, todas medidas:**

1. `meson compile` resolve alvo por **nome**; o `ninja` cru resolve por **caminho de saída**.
   `ninja -C build xboard` dá *"unknown target"*.
2. `meson install --tags sdk` **não instala os headers** — `install_headers()` não aceita
   `install_tag` no Meson 1.2, então eles ficam com a tag automática `devel`. Tem de ser
   `--tags sdk,devel`, e o sintoma da falta aparece dois alvos depois, como
   *"PluginAbi.hpp: No such file or directory"*.
3. O `PKG_CONFIG_PATH` **de ambiente é descartado** quando o native-file do Conan fixa
   `pkg_config_path`. Tem de ser `-Dpkg_config_path=` na linha de comando — e o separador de lista
   do Meson é **vírgula**, não dois-pontos.

Binários ficam em `build/src/<nome>/src/<nome>` — o executável tem o **mesmo nome da pasta** —,
e cada um tem um alvo `run-<nome>` no Makefile.

**Todos os binários leem `configs/`/`data/` por caminho relativo (`./src/<nome>/...`) e devem
ser executados a partir da raiz do repositório:**

```bash
./build/src/single-thread/src/single-thread
```

Opções de linha de comando, aceitas pelas duas pocs: `-f <arquivo>` (cenário alternativo),
`-threads <N>` (quantas threads de tempo crítico) e `-deterministic <N>` (N frames de passo fixo).

**A suíte de testes vive em `tests/`** (`make test`; ver a seção própria mais abaixo). Além dela,
a verificação de **determinismo** tem alvos próprios (`check-single-thread` e `check-multi-thread`): roda N frames de passo fixo com 1, 2 e 4 threads
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
├── data/                  # dados de RUNTIME (recordings/, logs/, messages/) —
│                          # gitignored, escritos pelo próprio binário
│                          # duas exceções, nenhuma vendorizada aqui: o tile SRTM
│                          # mora em shared/data/terrain/ (é do cenário, o mesmo
│                          # nas duas pocs) e a aeronave JSBSim mora em
│                          # models/flight/data/jsbsim/ (é do MODELO — ver
│                          # a seção "O MODELO é um plugin" mais abaixo)
├── include/
│   ├── app/               # as etapas da aplicação, uma questão por arquivo (ver abaixo)
│   └── mixr_factory.hpp   # factory dos objetos MIXR deste subprojeto
└── src/
    ├── meson.build        # define o executable() — nome = nome da pasta
    ├── main.cpp           # FINO: só orquestra (chama os módulos de app/ na ordem)
    └── app/               # espelha include/app/
```

> **O MODELO não está aqui.** `domain/`, `bt/`, `ubf/` e `xnative/` moram em
> `models/flight/`, um projeto Meson independente construído numa etapa **anterior**
> (`make models`) e carregado com `dlopen`. O host só consome o `.so` — ver `models/README.md`.
> A guarda `tests/guard/check_host_opaco.sh` trava esse invariante.

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
(`xnative/ThreadTag` no modelo; o log virou `shared/xlog` e o quadro de status, `shared/xboard`).

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

## Testes automatizados (`tests/`)

`make test` roda tudo (`meson test`); exige `configure` com `-Dtests=true`. O framework e o
**GTest**, declarado como `test_requires` no `conanfile.py` — nenhum binario da aplicacao linka
gtest. Cinco camadas, da mais isolada para a mais integrada:

| suite | o que prova | custo |
|---|---|---|
| `domain` | as regras puras de `domain/` (histerese, alvo fixo, lado da quebra, piso anti-CFIT, pernas da patrulha) | 42 testes, ~10 ms |
| `tree` | a maquina de estados, carregando o **`flight_tree.xml` de producao** contra um `FakeDecisionContext` — sem `Station` | 15 testes, ~10 ms |
| `native` | as classes MIXR do modelo: coerencia da fabrica, slots com tipo **e unidade**, e a fronteira de fase do `AlertDatalink` — sem `Station` | 9 testes, ~10 ms |
| `scenario` | o binario de verdade, com fixture, afirmando comportamento sobre as linhas `frame=` | 3 modos × 2 pocs |
| `memory` | vazamento, pelos contadores de instancia do proprio MIXR | 2 execucoes por poc |
| `determinism` | mesmo estado com 1, 2 e 4 threads, **nos dois lacos de decisao**; e o CONTROLE NEGATIVO que prova de onde o determinismo vem | 4 execucoes por poc + `onde-a-decisao-roda` |
| `plugin` | o contrato de carga dinamica, os 7 modos de falha, a prova de hot-swap e **o cenario de producao rodando com um modelo DESCONHECIDO** | 5 testes, ~3 s |
| `guard` | `domain/`, `bt/` e a fiacao de plugin continuam byte-identicos entre as duas pocs | instantaneo |

`make test-asan` e complementar e fica fora da suite: reconfigura com ASan, roda sob
LeakSanitizer e reverte. Ver as supressoes em `tests/memory/asan.supp`.

**O que mudou no codigo para isso ser possivel** (duas mudancas mecanicas, provadas neutras — o
dump deterministico saiu identico ao de antes, byte a byte):

1. `FlightState::Snapshot` virou `domain::WorldView` (`include/domain/WorldView.hpp`), com
   `using Snapshot = domain::WorldView;` mantendo todos os call sites. A estrutura nunca teve tipo
   do MIXR; o que a prendia ao framework era so morar dentro de uma classe que herda de
   `AbstractState`.
2. `bt/NodeContext.hpp` deixou de carregar um `BtBehavior*` concreto e passou a apontar para
   `bt_nodes::DecisionContext` (`include/bt/DecisionContext.hpp`), a interface com os 8 getters que
   os nos ja usavam. `BtBehavior` a implementa sem um metodo novo. Resultado: `bt/nodes/*.cpp`
   compilam com BT.CPP + `domain/` apenas — `ldd` no binario de teste da arvore mostra **zero**
   libs do MIXR.

**Armadilhas confirmadas rodando — nao redescobrir:**

1. **O modo `-deterministic` NAO e hermetico com o cenario de producao.** O bloco `networks:` abre
   a porta DIS 3000 e ingere PDUs de quem estiver na LAN: com um `bandit-dis` de outra sessao no
   ar, duas execucoes identicas divergem e o `check-single-thread` acusa falso nao-determinismo
   (medido: `frame=600 falcon1` deu `PATROL` com 1 thread e `SUPPORT` com 2 — porque o intruso da
   rede apareceu em uma e nao na outra). Por isso **todas** as fixtures de teste removem
   `networks:`, e os alvos `check-*` passaram a rodar em cenario hermetico. Com o cenario
   hermetico as duas pocs passam com 1, 2 e 4 threads em 2000 frames.
2. **As fixtures sao DERIVADAS do cenario de producao** (`tests/scenario/make_fixture.py`), nao
   copias versionadas: uma copia comecaria certa e envelheceria em silencio. O modo `intruder`
   reintroduz um `bandit1:` **local** — sem ele nao ha como exercitar `EVADE`/`SUPPORT` num
   processo so, ja que o intruso hoje mora em `src/bandit-dis` e chega apenas por DIS.
3. **Os contadores de instancia do MIXR nao sao atomicos.** `MetaObject.count/mc/tc`
   (`MetaObject.hpp:31-33`) sao mantidos por `STANDARD_CONSTRUCTOR`/`STANDARD_DESTRUCTOR` com
   `int` cru (`macros.hpp:247-255`); com os agentes decidindo em paralelo no pool T/C os
   incrementos correm entre si. O teste de vazamento roda com `-threads 1`.
4. **Vazamento so se prova comparando duas duracoes.** Um retrato unico nao distingue "vazando" de
   "retido de proposito": o teste roda 500 e 1000 frames e exige `count` igual e `tc` crescendo.
   Medido saudavel: `FlightAction` faz `tc` = 4 × frames (uma decisao por aviao por frame), com
   `count=0` e `mc=1`.
5. **O ASan acusa 896 bytes em 22 alocacoes que NAO sao do modelo** — todas de partida, dentro do
   framework: `JSBSimModel::setSlotRootDir`/`setSlotModel` clonam uma `base::String` de slot e
   nunca a liberam, `PrintHandler::setFullFilename` (alcancado por `xlog::init`) e
   `DataRecorder::setSlotEventName`. Sem as supressoes de `tests/memory/asan.supp` o alvo nasce
   vermelho e vira ruido. Nenhuma delas cresce com os frames — confirmado por outro caminho pela
   suite `memory`.
6. **`dec=` agora existe nas DUAS pocs.** A `multi-thread` ja o tinha, do `FlightAgentTC`; a
   `single-thread` decide no laco de background, entao quem conta e o `shared/xboard`, no ponto da
   atuacao (`FlightAction::execute`). A assercao **nao** e `dec == frames`: a `multi-thread` decide
   uma vez a mais na inicializacao (601 em 600 frames, identico nas 3 configuracoes de thread). O
   que se afirma e que `dec` avanca na **mesma taxa** que `frame` entre dumps consecutivos — mede a
   propriedade certa e ignora o offset de partida.
7. **`app/ScenarioTemplate` grava o cenario expandido sempre no mesmo caminho**
   (`src/<poc>/configs/scenario.generated.epp`, nao configuravel por linha de comando), entao dois
   testes que rodem um binario ao mesmo tempo disputam o arquivo. Todos os `test()` que executam
   uma poc sao `is_parallel: false`.
8. **`wrap180()` tem borda em -180, nao em +180.** O header documenta `(-180, 180]`, mas
   `wrap180(180) == -180` (`fmod(360,360)==0`). Nao e inofensivo: `ThreatPolicy` escolhe o lado da
   quebra por `relBearingDeg >= 0`, entao um contato exatamente a re quebra sempre para o mesmo
   lado. O teste trava o comportamento observado, nao o comentario.

### `shared/xmsg` — mensagens configuráveis por EDL

Escolher **o que** sai da simulação e **quando** sai vira configuração, não recompilação. Um
`( MsgFeed )` no `components:` da `Station` amostra os players, avalia condições e entrega as
mensagens a uma lista de destinos. Classes: `MsgFeed`, `MsgReport`, as condições
(`MsgChanged`/`MsgThreshold`/`MsgRate`) e `MsgFileSink` (NDJSON).

```
msgFeed: ( MsgFeed
   trackManager: twsTrkMgr   maxPlayers: 64   healthEvery: ( Seconds 10 )
   sinks:    { ( MsgFileSink fileName: "./src/<poc>/data/messages/mission.jsonl"
                             flushEvery: ( Seconds 2 ) ) }
   messages: {
      ( MsgReport name: telemetria players: { } labels: { player side mode }
         fields: { latDeg lonDeg altMslM speedKts machNum fuelFrac }
         every: ( Seconds 1.0 ) )
      ( MsgReport name: mudanca-altitude players: { falcon1 }
         fields: { altMslM climbMps }
         when: { ( MsgChanged field: altMslM by: ( Meters 100 ) ) } )
   } )
```

**Por que NÃO usa o `mixr::recorder`, que seria a resposta óbvia.** O schema `DataRecord.proto`
é fechado: `PlayerState` carrega só `pos`/`angles`/`vel` (ECEF) e `damage`. **Não há combustível,
motor, Mach, G nem AGL** — justamente as grandezas pedidas. Tokens REID de usuário (1000-9999)
são descartados em silêncio (`AbstractDataRecorder::recordDataImp()` devolve `true`
incondicionalmente, então `processUnhandledId()` nunca dispara para token desconhecido). E não há
primitiva nenhuma de mudança/limiar/histerese (grep por `hysteresis|Schmitt|Threshold|Debounce`
em `include/mixr/`: zero). Remendar o `.proto` está vetado — mesma decisão registrada na seção
`shared/xlog`. O que se reaproveita é a **forma** (EDL declarativo, cadeia de destinos com filtro
por assinante, trabalho fora do frame T/C); o `Player` é lido direto, como
`ubf/FlightState::updateState()` e `TacviewOutput::updateRadarScan()` já fazem.

**`rules/` é livre de MIXR** (`Schmitt`, `Deadband`, `RateWindow`, `EmitGate`) e é testado no alvo
`test-domain`, sem levantar simulação — mesmo movimento do `domain::WorldView`.

**Armadilhas confirmadas rodando — não redescobrir:**

1. **Lista `{ a b c }` no EDL põe o nome no OBJETO, não no slot.** Descoberto quebrando:
   `fields: { latDeg lonDeg }` chegava como os campos `"1"` e `"2"`. Itens sem `:` são
   **anônimos** — o parser numera os slots automaticamente (`edl_parser.y:144-155`) e o nome real
   vai no valor, como `base::Identifier`. Já `{ chave: valor }` (o `typeMap:` do xtacview) põe o
   nome no slot. Ler o **objeto primeiro** e cair para o slot cobre as duas formas — e cobre de
   quebra a armadilha irmã de valor sem aspas ser `Identifier` e não `String`.
2. **Acumulador de tempo simulado precisa de tolerância.** Medido: `0.1` somado 10 vezes dá
   `0.9999999999999999` (fica **abaixo** de 1.0), enquanto `0.02` somado 50 vezes dá
   `1.0000000000000004` (fica **acima**). Nenhum dos dois `dt` é representável em binário, e o
   erro **muda de sinal** entre o laço de 10 Hz e o de 50 Hz: sem tolerância, um
   `hold: ( Seconds 1.0 )` arma no passo 50 em `-deterministic` e só no passo 11 em tempo real.
   Ver `rules/timeTolerance.hpp`. A `domain::ThreatPolicy` tem o mesmo padrão
   (`holdTimer_ -= dt; if (holdTimer_ <= 0.0)`), mas ali o `<=` faz o erro cair para o lado
   seguro — não há bug hoje.
3. **`getEngRPM()` é polimórfico na UNIDADE.** `JSBSimModel.cpp:234-281`: pistão e elétrico
   devolvem RPM absoluto (`getRPM()`), turbina devolve `GetN2()` em **percentual**, turboprop
   devolve `GetN1()`, foguete devolve 0 — e o `default:` **não escreve no array** enquanto ainda
   conta o índice no retorno. O c310 destas pocs é `engIO470D`, **pistão, maxrpm 2625**; medido em
   voo: **2700 RPM**, não ~95. Por isso o campo chama-se `engRpmRaw`, os arrays são zerados antes
   da chamada, e o sinal **primário** de pane é `engThrustAsymFrac` = (max−min)/max entre motores
   — adimensional, sem calibração por aeronave (medido saudável: 0,0015).
4. **Os motores levam segundos para subir.** O `JSBSimModel` nativo nunca os liga (armadilha 13.2
   do README da poc); quem liga é `data/jsbsim/systems/engine-autostart.xml`. Medido: em t≈1 s o
   empuxo é ~0 e o RPM é 1,16; em t≈7 s são ~400 lbf e 2700 RPM. **É por isso que `hold:` existe**
   — sem ele, todo gatilho de motor dispara no transiente de partida.
5. **Grupo inválido vira `null`, nunca 0.0.** O `bandit1` recebido por DIS é clonado de um
   `template:` **sem `dynamicsModel`**, então toda grandeza de motor dele lê zero — e zero é um
   valor plausível de empuxo. Condição sobre campo inválido **não avalia, não gera borda e congela
   o nível**; sem essa regra, uma mensagem de pane com `players: { }` acusaria falha permanente
   no intruso.
6. **O amostrador NÃO pode morar no `Player`.** Seria o padrão do `models::CollisionDetect` e
   daria 50 Hz, mas `Player::updateTC()` (`Player.cpp:536`) e `Player::updateData()`
   (`Player.cpp:619`) são os dois guardados por `mode == ACTIVE || PRE_RELEASE`, e
   `crashNotification()` faz `setMode(CRASHED)` — o observador **emudece exatamente na borda que
   existe para reportar**. Preço aceito: resolução de 10 Hz (tempo real) / 50 Hz
   (`-deterministic`).
7. **`sinks:` e `messages:` ficam em SLOT, nunca em `components:`.** É isso que garante que
   `Component::updateTC()` — que só desce para a lista de componentes — não tenha caminho até
   eles. O preço é encaminhar `reset()` e `shutdownNotification()` à mão. (`Station::reset()`
   **chama** `BaseClass::reset()` no fim, então o `MsgFeed` em `components:` recebe reset normal.)
8. **`suppressed` só conta para mensagem de EVENTO.** Numa mensagem periódica o `every:` é um
   limitador de taxa e não emitir a cada ciclo é o comportamento pedido — contar isso como
   supressão enchia o `msgHealth` de milhares (medido: 4900 em 24 s) e afogava o número que
   importa, que é a borda de evento adiada.
9. **`every:` adia, não descarta.** Uma borda que cai dentro do piso é emitida assim que ele
   vence. Descartar perderia justamente a borda de `crashedFlag` sob carga, e em silêncio.
10. **`MsgFileSink` não reusa o `recorder::PrintHandler`**, ao contrário do `xlog`:
    `printToOutput()` termina em `std::endl`, ou seja **um flush por linha**
    (`PrintHandler.cpp:284`). A dezenas de linhas/s isso vira tempestade de syscall no mesmo laço
    que também drena o gravador do Tacview. `ofstream` próprio, flush periódico.
11. **O diretório `data/messages/` tem de existir** — mesma causa da falha muda do `TacviewOutput`
    sem `data/recordings/`. Há um `.gitkeep` em cada poc.

**O que fica de fora**: injeção de pane (`class JSBSimModel final` — não há subclasse possível;
a rota real seria `Player::setThrottles()` com valor `< 0.0`, e é peça de modelo, não de
mensageria), destinos de console e de rede (uma classe + uma linha na factory cada), e eventos
discretos "no instante exato" — o que se alcança é o que se deriva por amostragem.

### O MODELO é um plugin, construído numa etapa PRÉVIA

**`src/<poc>/` é só o host.** `domain/`, `bt/`, `ubf/` e `xnative/` **não estão em `src/`** — moram
em `models/flight/`, que é um **projeto Meson independente**, construído antes do host
(`make models`). O host só consome o `.so` instalado em `dist/lib/mixr-plugins/`.

Isso não é arrumação: é o que torna **verificável** o cenário de um terceiro entregar só o binário.
Enquanto o modelo era um alvo do host, o `files()` dele listava os 24 `.cpp` e o `meson setup` do
host exigia o fonte — o oposto do que se queria provar. A guarda
`tests/guard/check_host_opaco.sh` trava o invariante.

```
models/
├── flight/              # projeto meson proprio -> build-flight/ -- O MODELO de producao
│   ├── include/{domain,bt,ubf,xnative}/   src/...
│   ├── configs/flight_tree.xml            # a arvore de comportamento
│   ├── data/jsbsim/                       # a aeronave (ver abaixo)
│   ├── tests/{domain,tree}/               # as duas camadas que testam o MODELO
│   └── meson.build      # UMA arvore, DOIS artefatos:
│                        #   libflight.so     (single-thread)
│                        #   libflight_tc.so  (multi, -DFLIGHT_TC_AGENT)
├── missile/              # projeto meson proprio -> build-missile/ -- SEGUNDO modelo,
│                         # demo academica (ver a secao "Demo: missil guiado" abaixo)
└── fixtures/
    └── stub/             # projeto meson proprio -> build-stub/ -- NAO e producao, e
        ├── src/stub.cpp  # um FIXTURE de teste (fica em fixtures/ de proposito)
        └── CONTRATO.md   # ~270 linhas, escritas SO contra o SDK -- o que um modelo
                           # TEM de fazer
```

**A duplicação entre as gêmeas foi dissolvida por construção.** Antes eram ~3.100 linhas copiadas
sustentadas por um teste de guarda; agora é uma árvore só, e a única diferença (o `FlightAgentTC`)
fica atrás de um `#ifdef`. O `make compare-single-multi` caiu de **11 para 5** arquivos.

**A aeronave é dado do MODELO, não do cenário — e por isso mora aqui, não em `src/<poc>/data/`.**
As três pocs (`single-thread`, `multi-thread`, `bandit-dis`) pilotavam a mesma cópia
byte-idêntica de `data/jsbsim/` (o c310), vendorizada três vezes. Não é coincidência: o próprio
`domain/`/`bt/` deste modelo é calibrado **para o c310** especificamente —
`maxClimbRateMps`/`maxRateOfTurnDps` do `Autopilot`, a folga de `TerrainFloor` contra o piso
anti-CFIT (~330 m por engajamento, ver a seção "Terreno" abaixo), os limiares de combustível —
trocar de aeronave sem recalibrar o modelo já não faria sentido. `install_subdir()` publica
`data/jsbsim/` em `dist/share/mixr-plugins/flight/jsbsim/` junto com `flight_tree.xml`, e
**todo** `rootDir:` de `( JSBSimModel )` nos três cenários — inclusive o de `src/bandit-dis`, que
não carrega o plugin nenhum, mas pilota a mesma aeronave — aponta para lá. `make models` publica
antes de `make build` precisar (ordem `deps -> sdk -> models -> build`), então a ordem normal já
garante o arquivo no lugar.

### O SDK de plugin

Publicado pelo projeto do host em `dist/`: o contrato (`xplugin/PluginAbi.hpp`, header-only) e as
**três `.so` que atravessam a fronteira** — `libxboard` (o quadro de leitura), `libxlog` e
`libxtrack` (o `TrackQuery` — o **contato detectado**, disputado pelo `track=` do dump e pela
percepção do modelo; não confundir com o `RadarScan`, que é o **apontamento da antena** e é do
modelo).
Mais um `poc-mixr-sdk.pc`, que é como o projeto do modelo o consome.

Essas três são as **únicas** `shared_library()` de `shared/`. As outras cinco seguem estáticas:
`xtacview`, `xclock`, `xjoystick` e `xmsg` — que um plugin **não pode** linkar, ou ganharia cópia
privada dos estáticos delas — e `xplugin`, que é o registro e só entra no executável (o plugin usa
o `xplugin_abi_dep`, header-only).

### O que o contrato NÃO cobria — e o que se fez a respeito

`PluginDescV1` é contrato de **empacotamento**. O que a aplicação de fato exige de um modelo estava
espalhado e não escrito: os 6 nomes de fábrica **e os slots deles**, as classes base obrigatórias,
e — a mais fácil de esquecer — o dever de **escrever no `xboard`**.

O cenário de falha que motivou o conserto: um terceiro entrega um `.so` que responde pelos 6 nomes,
deriva das bases certas, mas nunca chama o `xboard`. O host sobe, o cenário parseia, os aviões voam
pelo `Autopilot` nativo — e o dump sai com `bt=--` e `dec=0` **com todos os outros testes verdes**.

Duas peças fecham isso:

- **`models/fixtures/stub/CONTRATO.md`** — a lista escrita, incluindo a obrigação do `xboard`.
- **`models/fixtures/stub/`** — um modelo de ~270 linhas escrito **só contra o SDK**, sem árvore de
  comportamento e sem uma linha de `domain/`, que registra os mesmos 6 nomes com os mesmos slots.
  O teste `plugin-modelo-estranho` roda o cenário de **produção** contra ele trocando **apenas** o
  `file:` do `( PluginModule )`. É o único teste que pode falhar por *"o contrato não basta"* —
  todos os outros carregam o mesmo modelo compilado do mesmo fonte.

  Ele já pagou por si duas vezes durante a implementação: revelou que `Player::getInitHeading()`
  não existe, e é o que garante que a lista de slots do cenário está completa.

**Armadilhas confirmadas rodando — não redescobrir:**

1. **A BehaviorTree.CPP é ESTÁTICA** (`.a`, 35 MB, 447 símbolos `T` globais). O host largou
   `behavior_tree_dep` (linkada dos dois lados duplicaria o contador de `BT::getUID()`), e o alvo
   do plugin **precisa de `-Wl,--exclude-libs,ALL`** — `gnu_symbol_visibility: 'hidden'` **não se
   aplica a objetos vindos de um `.a`**. Medido: com a flag, o `.so` de 11 MB exporta **um** símbolo.
2. **Nunca `dlclose`** — toda instância viva guarda ponteiro para o `.data` do plugin, e o destrutor
   **escreve** `metaObject.count--` lá. `RTLD_NODELETE` torna um `dlclose` acidental um no-op.
3. **`RTLD_LOCAL`, contra o prior art do BT.CPP** — `type_info::operator==` cai em `strcmp` no ELF,
   e o escopo de um objeto `RTLD_LOCAL` já inclui o global. `GLOBAL` traria interposição silenciosa
   entre plugins (`Player::metaObject` é símbolo `GLOBAL OBJECT`).
4. **`meson test` devolve rc=0 para suíte VAZIA** ("No tests defined."), e o default de `-Dtests` é
   `false`. Com duas suítes em dois diretórios de build, perder uma seria um verde silencioso — por
   isso `make test`/`make test-models` conferem a contagem com `meson introspect --tests` antes de
   rodar. Medido nos dois lados: `meson test` rc=0 com 0 testes, e a guarda pega.
5. **O host não tem aresta Meson até o `.so`** (ele vem de `dist/`, de outro projeto), então o
   frescor é afirmado à mão por `tests/guard/check_modelo_fresco.sh`. Sem isso a suíte inteira passa
   contra um plugin velho.
6. **`app/MetaObjectReport` é estruturalmente cego para classe de plugin** — `reportClass<T>()` é
   template e `getMetaObject()` é estática, não virtual. Por isso o descritor carrega os `metas`.
7. **A demonstração de hot-swap precisa de PATROL** — com o intruso da fixture a árvore vai para
   `EVADE` e o sentido da curva quase não influi; e uma perna curta demais faz os +90 e os −90
   somarem 360 e coincidirem. O teste remove o intruso e usa perna de 10 s.

**Prova de neutralidade:** com o modelo fora do executável e carregado de `dist/`, o dump `frame=`
das duas pocs saiu **byte-idêntico** ao de antes de existir plugin nenhum.

`make check-plugin-hotswap` troca o sentido da curva em `models/flight/src/domain/PatrolPlan.cpp`,
rebuilda **só** o `.so` (2 edges), confere que o executável não foi tocado e mostra o rumo do
falcon1 indo de 141° para 34°.

## Ao adicionar um subprojeto novo

1. Criar `src/<nome>/` — nome descritivo, sem prefixo numérico, e é ele que vira o nome do
   executável — seguindo a estrutura acima; sempre com `include/mixr_factory.hpp` +
   `src/mixr_factory.cpp` (a factory **não** fica inline no `main.cpp`).
2. Adicionar `subdir('./<nome>')` em [src/meson.build](src/meson.build).
3. Adicionar o alvo `run-<nome>` no [Makefile](Makefile) — apontando para
   `$(BUILD_DIR)/src/<nome>/src/<nome>`.
4. **Declarar o rpath no `executable()`**: `link_args: rpath_link_args`, `build_rpath: mixr_libdir`
   e `install_rpath: mixr_libdir + ':' + own_libs_rpath` — o `own_libs_rpath` (`$ORIGIN/../lib`) é o
   que faz o binário de `dist/bin/` achar `libxboard.so`/`libxlog.so`/`libxtrack.so` em `dist/lib/`.
   **Não há `behavior_tree_dep` nem `jsbsim_dep` na raiz**: quem precisa delas é o modelo, e ele é
   outro projeto (ver `models/README.md`).
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

## Demo: míssil guiado (segundo modelo, ativação de player em runtime, lançamento/detonação/destruição)

Exemplo acadêmico, isolado da produção: `falcon1`, no cenário de demo
`src/single-thread/configs/scenario_missile_demo.epp.in` (rodado com `-f`, binário `single-thread`
já existente — nenhum host mudou), carrega **um** míssil guiado e o lança contra um `bandit1`
LOCAL (sem `networks:`, mesmo motivo do `tests/scenario/make_fixture.py`: hermético). Existe para
exemplificar três coisas, nesta ordem:

1. **Criar um modelo novo, num plugin próprio.** `models/missile/` — cópia da receita de
   `models/fixtures/stub` (`models/README.md` §2): projeto Meson à parte, só `mixr_dep` + `sdk_dep`
   (sem `behavior_tree_dep` — o míssil não decide com árvore, só guia e voa), publicando **só**
   `GuidedMissile`. **Por que um plugin separado e não dentro do `flight`:** `provides:` no
   `.epp` é igualdade exata de conjunto contra o que a `.so` exporta — acrescentar `GuidedMissile`
   aos 6 nomes do `flight` obrigaria **todo** cenário que carrega essa `.so` (inclusive os de
   produção) a atualizar `provides:`. Um segundo `( PluginModule )` no cenário de demo, com seu
   próprio `provides: { GuidedMissile }`, evita isso — `flight` ganhou código novo (a
   política de lançamento, ver item 2) mas os 6 nomes publicados não mudaram.
2. **Ativar um player em runtime.** Não foi escrito nenhum código para isso — é o mecanismo NATIVO
   de liberação de armas do MIXR, o mesmo que chaff/flare/bombas já usam:
   `StoresMgr::releaseOneMissile()` → `Stores::releaseWeapon()` → `AbstractWeapon::release()`, que
   clona o `( GuidedMissile )` declarado em `stores:` e o enfileira via
   `Simulation::addNewPlayer()` — materializado no próximo `updatePlayerList()` do laço de
   background, exatamente como `interop::NetIO::createIPlayer()` materializa o fantasma DIS do
   `bandit1` nas duas pocs gêmeas. `xnative::FlightAction::execute()`
   (`models/flight/src/ubf/FlightAction.cpp`) é o único ponto do `flight` que toca esse
   objeto MIXR de arma: resolve o alvo por nome
   (`player->getWorldModel()->findPlayerByName(...)`), libera o míssil e chama
   `setTargetPlayer(alvo, true)`.
3. **Lançamento → detonação → destruição.** `bt/nodes/LaunchEnvelopeCondition` +
   `bt/nodes/LaunchMissileAction` (árvore de demo `flight_tree_missile_demo.xml`, cópia da de
   produção com um ramo a mais — a de produção fica intocada) decidem **quando**; detonação
   (`collisionNotification()`/`crashNotification()`/`updateTOF()`, todos nativos de
   `AbstractWeapon`) já levam o `mode` a `DETONATED` sozinhos. `shared/xmsg` já mapeia
   `AbstractPlayer::Mode` para string (`"launched"`, `"detonated"`) — é a fonte de verdade mais
   barata para ver os três eventos (o `( MsgReport name: ciclo-de-vida ... fields: { modeNum } )`
   do cenário de demo), sem depender de `REID_WEAPON_RELEASED`/`REID_WEAPON_DETONATION` nativos
   (mesma família do bug do `DataRecorder` já documentado acima — por isso **não** entram em
   `enabledList`; o míssil ainda aparece no Tacview via `REID_PLAYER_DATA`, como qualquer player).

**Armadilhas confirmadas rodando — não redescobrir:**

1. **O nome de fábrica de `SimpleStoresMgr` é `"StoresMgr"`, não `"SimpleStoresMgr"`.** O header
   documenta isso (`Factory name: StoresMgr`) mas é fácil escrever o nome da classe C++ por engano
   no EDL — o `mixrFactory` recusa com "nome de fábrica desconhecido". A classe abstrata
   `StoresMgr` não é construível via EDL nenhuma.
2. **`DETONATED` sozinho nunca remove o player da lista.** Só `AbstractWeapon::reset()` transiciona
   para `DELETE_REQUEST`, e só num reset de cenário inteiro — em jogo, quem tem de fazer essa
   transição é o próprio modelo. E ela **não pode morar em `dynamics()`**: `Player::updateTC()` só
   chama `dynamics()` quando `mode == ACTIVE || mode == PRE_RELEASE` — uma vez `DETONATED`,
   `dynamics()` para de ser chamado e um timer ali nunca avança (medido: o player ficava
   "detonated" para sempre). `AbstractWeapon::updateTC()` é diferente: ele chama
   `BaseClass::updateTC(dt)` (que tem o mesmo portão) e **depois** roda a própria lógica de fase
   (a transição `PRE_RELEASE`→`ACTIVE`, o TOF) sem checar o `mode` do míssil — é por isso que o
   framework consegue ativar um weapon em primeiro lugar. O timer de destruição tem que entrar no
   mesmo lugar (`updateTC()`), pelo mesmo motivo.
3. **`updateTC()` é chamado 4x por frame, uma vez por fase, com `dt = dt_do_frame/4`**
   (`Simulation::updateTC()` chama `updateTcPlayerList(..., dt0/4.0, ...)` dentro de um laço de 4
   fases). Um acumulador de tempo em `updateTC()` tem de (a) só somar numa fase fixa (ex.: fase 3,
   o mesmo ponto onde `AbstractWeapon` atualiza o TOF) **e** (b) multiplicar esse `dt` por 4 — sem
   os dois, o timer soma 4x mais rápido (sem a guarda de fase) ou 4x mais devagar (sem a
   multiplicação). Medido: sem a multiplicação, um timer de 2 s levava quase 8 s de tempo simulado
   para disparar.
4. **Um controlador de guiagem só-proporcional diverge nesta aeronave, mesmo com o comando
   limitado em taxa de variação.** Reduzir os momentos de inércia do `aim1.xml` na mesma proporção
   do peso (para "mais carga G") deixa o modo de arfagem/rolagem pouco amortecido demais para o
   passo fixo de 0,02 s do integrador do JSBSim — medido divergindo (mais de 100° de banco/arfagem
   e velocidade escalando para milhares de nós em menos de 0,3 s) mesmo com o comando de
   `domain::pursuit()` limitado em taxa (`GuidedMissile::guide()`). O que resolveu foi **não**
   reduzir `ixx`/`iyy`/`izz` (ficam exatamente os do c310, cujas tabelas aerodinâmicas — `Clp`,
   `Cmq` etc. — foram calibradas para essa inércia): a massa (`emptywt`) cai ~15x sozinha, o que já
   basta para "mais carga G" (G = sustentação / peso, sustentação inalterada), sem desestabilizar a
   dinâmica rotacional. Fisicamente estranho (massa cai, inércia rotacional não), mas é a
   aproximação acadêmica documentada no próprio `aim1.xml`.
5. **O envelope de lançamento tem que cobrir o alcance do PRIMEIRO contato, não um alcance de
   engajamento "realista".** Nesta geometria de cenário o primeiro contato acontece a ~13,6 NM
   (mesmo valor usado em `tests/tree/test_flight_tree.cpp`), e `ReportAndEvade` FOGE do contato
   assim que ele aparece — o alcance só cresce depois disso. Um `launchMaxRange` menor que o
   alcance do primeiro contato nunca dispara: a janela de lançamento é o primeiro frame com
   contato, ou nunca.
6. **Comentário de XML do JSBSim não aceita `--` (hífen duplo) em lugar nenhum do corpo**, mesma
   classe de armadilha do parser EDL com acento (ver a armadilha 5 da seção `bandit-dis` acima) —
   aqui é o `xmllint`/parser XML do JSBSim que recusa, não o `edl_parser`. Confirmado quebrando
   várias vezes escrevendo os comentários longos de `aim1.xml`: `xmllint --noout <arquivo>` antes
   de rodar economiza o ciclo de "recompilar só para descobrir que é um typo de comentário".
7. **`AbstractWeapon::release()` nomeia o flyout `"W%05d"`** a partir do próximo ID de arma
   liberada (`Simulation::getNewReleasedWeaponID()`, faixa default `[10001..65535]`) — com um único
   míssil no cenário, o nome sai previsível (`"W10001"`), mas não vale a pena depender disso para
   mapear `typeMap`/`colorMap`/`modelMap` do Tacview: o míssil aparece com o ícone padrão, o que já
   basta para a demo.

## `src/dashboard` — TUI de controle/monitoramento (estilo btop)

Quarto subprojeto: a MESMA pilha nativa de `single-thread` (`Aircraft`/`JSBSimModel`/`Autopilot`/
radar/`SimAgent`, o mesmo plugin `libflight.so`, **nenhuma** mudança em `models/`) — só troca
`app/RealTimeRun.cpp` (a linha de status em texto) por `app/DashboardLoop.cpp`, um painel FTXUI
(cores, borda, medidor de combustível, navegação por teclado). Três cenários **próprios**,
herméticos, em `configs/` (`scenario_patrol`/`scenario_intercept`/`scenario_intercept_missile`),
com porta de Tacview (**1236**) e diretório de dados (`./src/dashboard/data/`) próprios — dá para
rodar ao lado de `single-thread`/`multi-thread` (porta 1234) sem colidir.

**Duas decisões de arquitetura, e o "porquê" de cada uma:**

- **FTXUI é a primeira dependência nova do HOST** nesta série de mudanças (as anteriores só
  acrescentaram plugins). `conanfile.py` pede só `self.requires("ftxui/7.0.3")` — o `.pc` do
  Conan (`build/ftxui.pc`) encadeia os três componentes da lib (`screen`/`dom`/`component`) via
  `Requires:`, então `dependency('ftxui', method: 'pkg-config')` sozinho já resolve tudo (conferido
  lendo `build/ftxui-ftxui-*.pc`); não precisou de três `dependency()` separados. Declarado só em
  `src/dashboard/src/meson.build` (não no `meson.build` raiz) porque é o único consumidor.
- **"Carregar cenário"/"reiniciar"/"parar" são um `execv()` de si mesmo** (`app/Respawn.hpp`),
  nunca uma segunda `Station` no mesmo processo. `app::buildStation()` (`StationBuilder.cpp`) só
  roda `xplugin::setBuiltinFactory()` + `edl_parser()` + `xplugin::seal()` **uma vez por
  processo**, em lugar nenhum do repositório esse caminho é chamado uma segunda vez, e
  `shared/xplugin/README.md` documenta que plugins não têm hot-reload em processo vivo —
  reconstruir por cima disso seria pisar em terreno nunca exercitado. `execv()` resolve o próprio
  caminho via `/proc/self/exe` (não `argv[0]`, que pode vir relativo) e é o caminho 100% testado:
  é o que já acontece toda vez que alguém roda o binário de novo, só que sub-segundo.

**Armadilhas confirmadas rodando — não redescobrir:**

1. **`shared/xclock::TimeControls`/`ConsoleKeyboard` NÃO entram aqui.** Os dois mexem em
   `termios` (modo bruto do terminal) por fora do FTXUI — que já é dono do terminal assim que
   `ScreenInteractive::Fullscreen()` começa (alternate screen buffer, o próprio modo bruto dele).
   As teclas de controle de tempo (`app/DashboardLoop.cpp`) chamam `ClockStation::
   setTimeScale()`/`togglePaused()`/`setPaused()` **direto** — a mesma API que
   `TimeControls::apply()` já usa por baixo, só sem a camada de tecla-crua no meio. A escada de
   velocidade (`0.10x .. 64x`) foi copiada de `TimeControls.cpp` de propósito, para a mesma
   sensação de controle das outras pocs.
2. **Duas threads, pelo mesmo motivo de `ConsoleKeyboard::poll()` ser não-bloqueante nas outras
   pocs**: a simulação tem de avançar a 10 Hz independente de quando o terminal manda evento. Uma
   thread roda exatamente o corpo de `RealTimeRun.cpp` (`station->updateData(dt)`, varredura de
   radar pro Tacview, o mesmo espaçamento por relógio de parede) e, por cima, captura um
   `DashboardState` sob mutex e chama `screen.PostEvent(Event::Custom)` para pedir redesenho — uso
   documentado do FTXUI para UI tipo "top" (uma thread externa empurrando eventos). A thread
   principal só roda `screen.Loop()`.
3. **`Ctrl+C` não precisa de handler próprio.** `ftxui::App::ForceHandleCtrlC(true)` é o default —
   a lib já instala o handler e sai do `Loop()` sozinha, mesmo que o `CatchEvent` não intercepte o
   evento. `action` fica no valor padrão (`Quit`), que é exatamente o que se quer (sair limpo, sem
   reexec). Confirmado rodando sob um pty: `Ctrl+C` restaura o terminal (o `\x1b[?1049l` de saída
   do alternate screen buffer aparece na saída) igual ao `q`.
4. **`app/MetaObjectReport.cpp` copiado de `single-thread` não compila sem trim.** O original
   reporta `mixr::xmsg::MsgFeed`/`MsgReport` — mas `dashboard` não linka `xmsg_dep` (nenhum
   cenário de `configs/` declara `msgFeed:`, o próprio TUI já é o "feed"), e o link falha com
   *"undefined reference to mixr::xmsg::MsgFeed::getMetaObject()"*. As duas linhas
   `reportClass<mixr::xmsg::...>()` (e o `#include` delas) saíram da cópia.
5. **`bandit1` nunca aparece no dump `-deterministic`, nem em `intercept`/`intercept_missile`** —
   `app/DeterministicDump.cpp` só imprime a `Fleet` (`playerNames = falcon1..4` em `main.cpp`), o
   mesmo em toda poc deste repositório (nem o modo `intruder` de `single-thread`/`multi-thread`
   mostra `bandit1` no dump). `tests/scenario/run_dashboard_test.py` afirma exatamente os 4
   falcons nos três cenários, não 5 — a primeira versão do teste esperava `bandit1` também e
   falhava.
6. **`SimpleStoresMgr` (o `StoresMgr` do `scenario_intercept_missile.epp.in`) é a mesma armadilha
   de nome de fábrica já documentada na seção "Demo: míssil guiado"** — `( StoresMgr ... )` no EDL,
   não `( SimpleStoresMgr ... )`.

**Redesenho: agnóstico a tipo de modelo, três abas (Frota/Mapa/Memória), botões clicáveis.** O
dashboard nasceu lendo `mixr::models::AirVehicle*` por uma lista fixa de nomes (`falcon1..4`) — só
fazia sentido para o modelo `flight`. Virou genérico por `mixr::models::Player` (a BASE): posição
(`getPosition()`), altitude/AGL, atitude, velocidade, `getType()` (string `type:` do EDL),
`getMajorType()`/`getSide()` (bitmasks nativos, pensados pelo framework para "que espécie de
player" e "de que lado" — cobrem `AIR_VEHICLE`/`GROUND_VEHICLE`/`WEAPON`/`SHIP`/`SPACE_VEHICLE`/
`BUILDING`/`LIFE_FORM` e `BLUE`/`RED`/`YELLOW`/`CYAN`/`GRAY`/`WHITE`) e `getMode()`
(`AbstractPlayer::Mode`) já são todos da base — só combustível/G/empuxo continuam exclusivos de
`AirVehicle` (conceito aerodinâmico), preenchidos só quando o `dynamic_cast` funciona
(`app/DashboardState.cpp`). `app::discoverPlayers(WorldModel*)` (`app/Fleet.hpp`) varre
`getPlayers()` a cada amostragem (10 Hz) em vez de uma lista fixa — entidades que nascem/somem em
runtime (o míssil lançado, o intruso local) aparecem/somem sozinhas. O rótulo de comportamento
(`bt=`) deixou de ter um `switch` sobre nomes do `flight` (`PATROL`/`EVADE`/...) — vira hash
FNV-1a determinístico sobre uma paleta fixa (`app/FleetPanel.cpp`), então um modelo futuro com
vocabulário de árvore totalmente diferente já funciona sem tocar o dashboard.

Três abas (`ftxui::Container::Tab`, `F1`/`F2`/`F3` ou clique): **Frota** (lista rolável +
detalhe da entidade focada), **Mapa** (canvas navegável) e **Memória** (contadores de instância ao
vivo). Cada ação (acelerar/pausar/trocar de aba/carregar/reiniciar/parar/sair) é uma lambda
nomeada usada tanto pela tecla quanto por um `ftxui::Button` com a dica de atalho já no rótulo
(`"[+] Acelerar"`) — sem duplicar lógica.

- **Painel "Memória" é o `app/MetaObjectReport.cpp` de sempre, só que AO VIVO.**
  `mixr::xplugin::pluginMetaObjects()` (`shared/xplugin/PluginRegistry.hpp`) já devolve os
  `MetaObject*` que o(s) plugin(s) carregado(s) declararam no próprio descritor — automaticamente
  cobre `flight`/`missile`/`stub`/qualquer modelo futuro, sem um nome de classe escrito no
  dashboard. `app/MetaObjectSnapshot.hpp` amostra isso a 10 Hz numa janela deslizante de ~3 s
  (`kHistoryWindow`) por classe e marca "CRESCENDO" quando `count` nunca caiu dentro da janela e
  termina maior que começou — critério simples e testável (`tests/dashboard/
  test_meta_object_snapshot.cpp`, sem MIXR nem FTXUI). Testado rodando: lançar o míssil do cenário
  `intercept_missile` faz `GuidedMissile` aparecer na lista com `tc` crescendo — sem "CRESCENDO",
  porque o próprio ciclo de criar/destruir mantém `count` oscilando, não subindo sem parar.
- **O mapa usa um `ftxui::Canvas` de tamanho FIXO** (`kCanvasW`/`kCanvasH` em
  `app/MapPanel.cpp`), não o `Box` calculado em tempo de render — o `Box` só existe DEPOIS do
  layout, tarde demais para escolher o tamanho do `Canvas` que se desenha DENTRO dele. Fonte de
  posição é só `EntityState.northM/eastM` (`Player::getPosition()`), então o mapa já funciona para
  qualquer tipo de player sem mudança — testado com o `GuidedMissile` (`W10001`, glifo `W` de
  `WEAPON`) aparecendo no canvas ao lado dos `falcon*` (`A`, `AIR_VEHICLE`).
- **Arrastar/zoom do mapa são tratados no `CatchEvent` MAIS EXTERNO, não num `CatchEvent` local
  da aba.** Armadilha encontrada rodando sob pty: `ftxui::Container::Vertical`/`Horizontal`
  (`ContainerBase::OnEvent`, `container.cpp`) só encaminha TECLADO para o filho `Focused()` —
  mouse tem caminho próprio (`OnMouseEvent`, que baixo do capô é o `ComponentBase::OnEvent`
  padrão: percorre TODOS os filhos, sem checar foco) e por isso sempre funciona, mas teclado não.
  Com `root = Container::Vertical({toolbar, contentTab})` sem selector explícito, `ActiveChild()`
  fica preso em `toolbar` (índice 0) até o usuário navegar o foco manualmente — setas endereçadas
  ao mapa/à lista nunca chegariam ao componente certo. Fix: tratar tecla de seta/`[`/`]`/`c`
  (mapa) e seta de navegação da lista (Frota/Memória) no `CatchEvent` mais externo — o mesmo que já
  trata `+`/`-`/espaço/etc., que roda incondicionalmente ANTES de qualquer roteamento por foco
  (`CatchEventBase::OnEvent` chama o handler primeiro, sem checar `Focused()`) — em vez de
  depender da cadeia de componentes. Mexer direto em `selectedEntityIndex`/`selectedClassIndex`
  tem o MESMO efeito da navegação interna do `ftxui::Menu` (o campo `selected` dele é um ponteiro
  para essa mesma variável). Confirmado rodando sob pty: sem o fix, `ArrowDown` na aba Frota não
  movia a seleção; com o fix, sim.
- **Lista rolável é o padrão oficial `menu_in_frame.cpp` do próprio FTXUI**: `Menu(...)->Render()
  | vscroll_indicator | frame | size(...)` — não foi inventado nada novo para caber "muitas
  entidades" na tela; é o mesmo idiom que a lib já documenta para esse caso.

**Bug confirmado e corrigido: a aba Mapa "prendia" a navegação — clique em QUALQUER lugar da
tela (inclusive nos botões `[F1]`/`[F3]`) era engolido como "começar a arrastar o mapa".** A causa:
o `CatchEvent` mais externo tratava `Mouse::Left`+`Pressed` como início de arrasto sempre que
`activeTab == 1`, sem checar SE o clique caiu dentro do canvas — um clique no botão `[F1]`,
desenhado no mesmo quadro, também disparava `mapView.dragging = true` e devolvia `true`
(consumido), então o `Button` por baixo nunca via o `Pressed` e seu `on_click` nunca disparava.
Fix: `ftxui::reflect(Box&)` aplicado ao elemento do canvas (dentro de `renderMap()`, que agora
recebe `Box& outCanvasBox`) captura a caixa de tela real do mapa a cada desenho; o `CatchEvent` só
inicia um arrasto se `mapCanvasBox.Contain(m.x, m.y)` — um arrasto **em andamento** continua
processando até soltar mesmo que o cursor escape do canvas por um instante (movimento rápido),
mas só o **início** (`Pressed`) e a roda do mouse exigem estar dentro. Confirmado rodando sob pty:
clicar em `[F1]` estando na aba Mapa agora troca de aba normalmente.

**A aba Mapa ganhou: setas de rumo, rastro opcional, seleção por clique com painel de detalhe,
duas perspectivas e rotação — tudo compartilhando a MESMA infraestrutura de projeção
(`app/MapPanel.cpp`).**

- **Setas de rumo, não pontos.** Toda entidade cujo `majorType` não é `BUILDING`
  (`hasMovementDynamics()`) ganha uma seta curta (`drawArrow()`, duas farpas em ~150° do eixo)
  apontando pra `headingArrowDir()` — no TopDown é o rumo de compasso girado pela mesma rotação da
  vista; no Lateral, a horizontal é a mesma projeção lateral do rumo e a vertical usa
  `sin(pitchDeg)` como aproximação VISUAL de subida/descida (não é física rigorosa, é só a mesma
  pista de direção que a seta já dá no TopDown). `BUILDING` é o único `MajorType` sem dinâmica de
  movimento no bitmask nativo — por isso o gate.
- **Rastro é opcional e por entidade, guardado em `MapViewState::trails`** (não em
  `DashboardState`, porque é estado de UI que sobrevive entre amostras, não um fato instantâneo da
  simulação), uma janela deslizante de `(northM, eastM, altitudeM)` por `Player::getID()`
  (`kMapTrailLength` ≈ 8s a 10 Hz). `updateTrails()` só roda quando `simSec` muda — o `Renderer`
  do FTXUI dispara a cada redesenho (tecla, resize...), não só a cada amostra nova; sem a guarda
  o rastro ganharia pontos duplicados sobrepostos. Alternado por tecla (`t`/`T`) OU pelo botão
  `[t] Rastro: ON/OFF`, cujo rótulo já mostra o estado atual (o `ButtonOption::transform` roda a
  cada redesenho, não só no clique — basta capturar `mapView` por referência).
- **Clicar numa entidade no mapa abre o MESMO painel de detalhe da aba Frota** —
  `renderEntityDetail()` (`app/FleetPanel.hpp`) é função pura, chamada direto de dentro do
  `Renderer` da aba Mapa, sem precisar duplicar o componente `entityDetail` da Frota (que não daria
  pra reusar como Component: um `ComponentBase` só pode ter UM pai na árvore do FTXUI). Clicar
  muda `selectedEntityIndex` — a MESMA variável que a aba Frota usa — então as duas abas sempre
  concordam sobre "quem está selecionada", e `[c] Centralizar` na aba Mapa centraliza em quem quer
  que esteja selecionada, mesmo que a seleção tenha vindo da Frota.
  - **Clique vs. arrasto se distingue pelo deslocamento total entre `Pressed` e `Released`**
    (`MapViewState::pressX/pressY`, fixados no `Pressed`): se o `Released` acontece a ≤1 célula de
    distância, é clique — chama `hitTestEntity()` (mesma projeção de `renderMap()`, comparando em
    CÉLULAS de terminal, não pixels de canvas: a resolução do mouse já é de célula, então
    converter a posição de cada entidade por `px/2, py/4` e comparar direto é suficiente, sem
    precisar de tolerância sub-célula).
- **TopDown/Lateral compartilham a MESMA rotação (`viewYawDeg`) por design** — é o pedido de "girar
  no eixo livre em todos os casos": o eixo livre é sempre o vertical (down), seja pra girar a rosa
  dos ventos (TopDown) seja pra escolher de que rumo se está olhando a formação (Lateral, onde a
  vertical da tela vira altitude). `project()` centraliza a rotação; só a componente que vira `py`
  muda (`rotN` girado vs. `altitudeM - panAltM`). `panMap()` foi reescrita para receber deltas em
  espaço de TELA (`screenRightM`/`screenUpM`, já invertendo a rotação) em vez de E/N de mundo —
  arrastar/setas funcionam igual nas duas perspectivas sem `DashboardLoop.cpp` precisar saber qual
  está ativa.
- **O canvas tem tamanho FIXO** (`kCanvasW`/`kCanvasH`), não o `Box` calculado em tempo de render
  — o `Box` só existe DEPOIS do layout, tarde demais pra dimensionar o `Canvas` desenhado dentro
  dele (mesma armadilha já registrada na entrada anterior desta seção, agora também motivo de
  capturar o `Box` À PARTE, via `reflect`, só para hit-test/gate de mouse — não para dimensionar
  nada).

**Segunda passada no Mapa: bolinha + linha (sem seta), rótulo em caixa com linha guia, eixos/
escala por perspectiva, árvore de BT gráfica no card, e alinhamento tabular nas duas listas.**

- **`drawArrow()` virou `drawHeadingLine()`** — só o traço na direção do rumo, sem as duas farpas
  do desenho anterior (pedido explícito: "ao invés de seta, use somente uma linha"). A entidade em
  si continua uma bolinha (`DrawPointCircleFilled`), como já era.
- **Nome da entidade agora é um rótulo em caixa (`drawLabelCallout()`), ligado ao ponto por uma
  linha** — antes era `DrawText` solto ao lado do ponto, que competia visualmente com o próprio
  marcador. A caixa fica a nordeste do ponto (recortada nas bordas do canvas quando não cabe);
  todos os rótulos são desenhados numa segunda passada, DEPOIS de todos os pontos/linhas de rumo
  — senão uma caixa desenhada cedo demais ficaria por baixo do marcador de uma entidade vizinha
  desenhada depois.
- **Os eixos "x"/"y" são relativos ao PAN (o ponto que está no centro da tela agora), não ao
  referencial absoluto da simulação.** Decisão deliberada, não simplificação preguiçosa: como a
  horizontal da tela já É `rotE` por construção (ver `project()`), o valor de cada linha de grade
  sai direto de `(gx-cx)*metersPerCell`, sem nenhuma trigonometria extra — e o rótulo continua
  fazendo sentido depois de arrastar/centralizar/**girar**, sem ter que reconciliar rótulo com
  rotação. TopDown ganhou grade completa (`x`/`y` em metros, com sinal) mais uma barra de escala
  explícita (segmento de comprimento conhecido com tique nas pontas, além das marcas de grade).
  Lateral ganhou só o eixo `y` (pedido explícito: "quando visto de lado, faça uso de um eixo y com
  a altura/altitude em pés") — `mixr::base::distance::M2FT` (`mixr/base/units/distance_utils.hpp`)
  faz a conversão; não existe eixo `x` rotulado no Lateral, só a mesma grade de referência.
- **A árvore de BT agora é visualizável de verdade, dentro do card de detalhe (Frota E Mapa) —
  novo módulo `app/BehaviorTreeView.{hpp,cpp}`.** O dashboard nunca teve acesso à ESTRUTURA da
  árvore em runtime (só ao rótulo da folha vencedora, via `xboard::Readout::label`) — por isso
  `loadTreeForScenario()` lê o MESMO `.generated.epp` que o cenário já resolveu, acha a primeira
  ocorrência de `treeFile: "..."` e faz o parse do XML (parser mínimo escrito à mão, só o
  suficiente pro formato do BT.CPP — ver o cabeçalho do `.cpp`; não vale a pena puxar uma lib de
  XML pra isto). **Isto quebra a agnosticidade a MODELO só até onde o próprio projeto já quebra: é
  agnóstico a qual `.xml` o cenário aponta, mas assume que existe um, no formato do
  BehaviorTree.CPP — a tecnologia de BT é uma escolha do PROJETO, documentada em
  `contexts/BTCPP-CONTEXT.md`, não uma suposição sobre qual modelo está carregado.** Renderizado
  como árvore de texto com linhas Unicode (estilo `tree`), destacando MELHOR ESFORÇO a folha que
  bate com o rótulo ativo (`matchesLabel()`: nome da tag contém o rótulo OU vice-versa,
  normalizado). **Limite conhecido e aceito**: `ReportAndEvadeAction`/`ReturnToBaseAction`
  decidem em RUNTIME entre dois rótulos cada (`"EVADE"`/`"BREAK"`, `"RTB"`/`"HOME"` —
  `models/flight/src/bt/nodes/ReportAndEvadeAction.cpp`/`ReturnToBaseAction.cpp`), e nenhum dos
  dois é substring do nome da tag (`ReportAndEvade`, `ReturnToBase`) — a árvore ainda aparece, só
  sem destaque nesses dois casos. A alternativa seria uma tabela de mapeamento escrita à mão aqui,
  específica do modelo `flight` — o oposto do resto deste dashboard, por isso não foi feita.
  Confirmado rodando: a árvore de `intercept_missile` (com o ramo extra `LaunchEnvelope`/
  `LaunchMissile`) aparece certinha no card, diferente da árvore de produção — prova que
  `loadTreeForScenario()` está lendo o arquivo CERTO por cenário, não um caminho fixo.
- **O card de detalhe tem a MESMA largura nas duas abas** (`kDetailPanelWidth`, em
  `app/FleetPanel.hpp`) — antes a Frota usava `flex` (largura = o que sobrava ao lado da lista) e
  o Mapa usava um `size(WIDTH, LESS_THAN, 36)` solto; as duas podiam sair com larguras diferentes
  dependendo do que mais estava na mesma linha. Unificado num `size(WIDTH, EQUAL, ...)` aplicado
  nos DOIS lugares, com o mesmo valor — determinístico, não depende de quem mais está na linha.
- **As listas da Frota e da Memória alinham em COLUNA de verdade** (`size(WIDTH, EQUAL, N)` por
  campo, larguras em `kCol*` de `app/FleetPanel.hpp`/`app/MemoryPanel.hpp`) — antes cada célula
  tinha a largura do próprio conteúdo (só com padding manual na string de fallback do `Menu`, que
  nem é o que aparece de verdade — o `entries_option.transform` é quem desenha), então um nome ou
  valor mais comprido empurrava as colunas seguintes fora de alinhamento entre uma linha e outra.

**Terceira passada: "ver no mapa" a partir da Frota, barra de nível na Memória (no lugar do
gráfico de linha) e confirmação para as quatro ações disruptivas.**

- **Botão "[m] Ver no mapa" no card de detalhe da Frota** — como Frota e Mapa já COMPARTILHAM
  `selectedEntityIndex` (a mesma variável, desde a passada anterior), o botão só faz
  `gotoTab(1)`: a entidade certa já aparece selecionada do outro lado, sem precisar re-selecionar
  nada. Só aparece no card da Frota (`buildDetailPanel(e, includeViewOnMapButton)`) — no Mapa você
  já está lá. Precisou virar filho de verdade do `Renderer` (`Renderer(btnViewOnMap, fn)`, não um
  `Renderer(fn)` solto) para o clique alcançá-lo — mesma regra de sempre: broadcast de mouse desce
  por todo `Container`, mas só quem está DE FATO na árvore de componentes recebe.
- **Barra de nível na Memória, no lugar do sparkline** — `gauge(count / pico)` (`ftxui::gauge`, a
  mesma primitiva já usada na barra de combustível). `pico` (`ClassStat::mc`) é o teto: o próprio
  `MetaObject` nativo mantém esse número, ele só CRESCE (nunca encolhe), e por isso a escala "varia
  com o tempo, já que o limite não é conhecido" — não tem teto fixo escrito em lugar nenhum, é o
  recorde já observado. `count=`/`pico=`/`criados=` continuam como texto, sem remover nada — só o
  gráfico de tendência (`sparkline()`, removida — ficou sem nenhum uso depois da troca) virou
  barra.
- **`l`/`r`/`s`/`q` agora pedem confirmação — mesmo padrão do exemplo oficial
  `modal_dialog_custom.cpp` do próprio FTXUI**, não o helper `Modal()` da lib (que tem semântica
  própria não verificada; preferido reusar um padrão já lido na fonte, depois do susto do bug de
  clique-trava documentado acima). `Container::Tab({withKeys, confirmDialog}, &uiDepth)` cuida SÓ
  do roteamento de evento — `TabContainer::OnEvent` só entrega pro filho ATIVO
  (`container.cpp`), então com `uiDepth==1` o `withKeys` (e tudo por baixo — botões, listas, mapa)
  para de receber qualquer evento, o que já BLOQUEIA sozinho a interação com o resto da UI
  enquanto o diálogo está aberto, sem precisar de nenhum flag extra. A composição VISUAL (o
  diálogo sobreposto, não substituindo a tela) é feita à mão no `Renderer` mais externo — `dbox`
  (camadas) + `clear_under` (opaco por cima) + `center` —, exatamente como o exemplo da lib faz:
  `Container::Tab::OnRender()` só desenha o filho ativo, então contar com o `Render()` automático
  do `Tab` mostraria SÓ o diálogo, sem o resto congelado atrás.
  - `l`/`r`/`s`/`q` viraram "pedidos" (só armam `pendingAction` + `uiDepth=1`); as ações de
    verdade (as que antigas faziam) ganharam o sufixo `Confirmed` e só rodam a partir do diálogo
    (`Enter`/clique em "[Enter] Confirmar") ou são descartadas (`Escape`/clique em "[Esc]
    Cancelar", `cancelPendingAction()`).
  - **`Escape` saiu do atalho direto de sair** (antes era sinônimo de `q`) — dentro do diálogo ele
    significa "cancelar", e mantê-lo também como atalho de sair no nível de cima geraria o efeito
    estranho de Escape armar a confirmação de sair e o Escape SEGUINTE cancelar ela na hora. `q`/
    `Q` continuam sendo o único atalho de teclado pra sair.
  - Confirmado rodando sob pty: `q` mostra o diálogo sem sair; `Escape` cancela e volta pra UI
    normal (`t=`/`sim=` seguem avançando, prova que o processo nunca foi encerrado); `r` + `Enter`
    reinicia de verdade (tela pisca, `t=` volta a 0 — o mesmo reexec de sempre, ver `app/
    Respawn.hpp`).

**Quarta passada: aba "Frota" virou "Players" com coluna de thread, Mapa em milhas náuticas
(vista de cima) e — a maior peça — breakpoint na árvore de BT.**

- **`[F1] Frota` → `[F1] Players`** (pedido literal, em inglês mesmo — única string da UI que
  foge da convenção geral de português do projeto, por instrução direta) e nova coluna de thread
  na lista (`EntityState::threadTag`, já existia só no card de detalhe). Nesta poc (a mesma pilha
  nativa da `single-thread`) a coluna sai sempre `-`: quem decide no pool T/C é a `FlightAgentTC`
  da `multi-thread`, não o `SimAgent` do laço de background que o dashboard usa — é a resposta
  CERTA, não um campo quebrado.
- **Mapa, vista de cima, em milhas náuticas** — só a vista de cima (pedido explícito); a de lado
  continua com pés no eixo Y (rodada anterior) e metros no resto. `mixr::base::distance::M2NM`
  (`distance_utils.hpp`) faz a conversão só na CAMADA DE EXIBIÇÃO — o zoom (`metersPerCell`)
  continua em metros por baixo, sem tocar `project()`/`panMap()`/`zoomMap()`.
- **Breakpoint de árvore de BT** — "marcar um estado da bt de um dado elemento e rodar a
  simulação até que aquele nó seja atingido, devolvendo a simulação pausada", com a escolha de
  velocidade ("a que eu decidir" ou "máxima possível") e tratamento de nó nunca atingido. Módulo
  novo nenhum — tudo em `app/DashboardLoop.cpp` (a lógica de arvore ficou em `app/
  BehaviorTreeView`, que perdeu a dependência de FTXUI: agora só expõe dado — `BtTreeLine`,
  `flattenBehaviorTree()` — e o critério de correspondência — `matchesLabel()`, promovido de
  privado pra público porque a checagem de breakpoint precisa do MESMO critério que já destacava
  a folha ativa).
  - **A "caixa da árvore" virou `ftxui::Menu` de verdade** (pedido explícito: clicável) — mas
    precisou de DUAS instâncias (`treeMenuFleet`/`treeMenuMap`), não uma só: `Container::Tab` (o
    `contentTab` que já existia) só entrega evento pro filho ATIVO, então um `Menu` dentro da
    Frota nunca receberia clique nenhum com a aba Mapa em cena. As duas instâncias compartilham o
    MESMO `MenuOption` (copiado, não movido — os ponteiros `entries`/`selected` continuam os
    mesmos) — selecionar numa aba reflete na outra, o mesmo padrão já usado por
    `selectedEntityIndex` entre Frota e Mapa.
  - **O alvo é (id do PLAYER, tag do nó) — "de um dado elemento", não "qualquer um nesse
    estado"**: `Breakpoint::entityId` é capturado no momento de armar (`doArmBreakpoint`), da
    entidade selecionada NAQUELE instante — trocar a seleção depois não muda o alvo já armado.
  - **Estado do breakpoint é compartilhado entre a thread de SIMULAÇÃO (quem checa a condição e
    quem pausa) e a de UI (quem arma/cancela)** — `bpMutex`, mesmo padrão de `stateMutex`/
    `latest` já usado pelo resto do arquivo. A checagem roda a cada amostra nova (10 Hz), rodando
    ou pausada, em QUALQUER escala de tempo — "a velocidade que eu decidir" não é um modo
    especial, é só não estar pausado enquanto o observador roda em paralelo.
  - **"Velocidade máxima possível" é um modo de fato novo**: `fastRunToBreakpoint` (atômico, lido
    a cada iteração do laço de `simThread`) faz o laço PULAR o `msleep()` de pacing — gira o mais
    rápido que a CPU permitir, independente da `timeScale` do `ClockStation` (que continua
    controlando quanto tempo SIMULADO cada passo representa; os dois efeitos se somam). Ao SAIR
    do modo rápido (atingiu, foi cancelado, ou nunca esteve armado), a referência de parede
    (`wallTimeElapsed`/`startTime`) é RESINCRONIZADA na hora — sem isso, o pacing tentaria
    "recuperar" de uma vez todo o tempo que o laço correu sem dormir, travando a tela por um
    tempo proporcional a quanto ele adiantou. Medido rodando: sem a resincronização o sintoma
    seria um `msleep()` de vários segundos logo depois do breakpoint disparar.
  - **"Trate os casos em que um nó objetivo nunca é atingido"**: cancelamento manual sempre
    disponível (`[x]`/botão, roda em qualquer aba) MAIS um limite automático —
    `kBreakpointTimeoutSimSec` (300s) de tempo SIMULADO (não de parede: independe de estar em
    velocidade máxima ou 1×) desde que foi armado. Vencido o prazo, desarma sozinho e avisa
    "NAO atingido... cancelado" em vez de rodar pra sempre.
  - **Testado ponta a ponta com um emulador de terminal de verdade (`pyte`, via um venv
    Python — a captura ingênua de bytes ANSI das rodadas anteriores não bastava pra achar
    coordenada de clique com confiança depois que a tela cresceu de camadas), não só inspeção de
    código**: clicar em `"SupportAlert"` na árvore mudou o status pra "Folha selecionada"; `[G]`
    armou em modo rápido (`t=` disparou pra centenas de "segundos" enquanto `sim=` mal se moveu,
    `dec=` na casa dos milhares — prova de que o laço rodou muitas iterações sem dormir);
    poucos segundos de parede depois, `falcon1` atingiu `SUPPORT` e o cabeçalho mostrou
    `PAUSADO` — a simulação nativa pausou de verdade (`ClockStation::setPaused(true)`), não só a
    UI achou que estava.

## Estado atual / pendências conhecidas

- A renomeação `poc/` → `src/` foi propagada aos caminhos de arquivo (defaults dos `main.cpp`,
  `.epp`/`.epp.in` e alvos do `Makefile`). **Comentários e banners de console ainda dizem
  `poc/<nome>`** — é só prosa, nenhum caminho depende disso.
- Não há pasta `docs/`: a documentação vive no `README.md` da raiz, no de cada subprojeto
  e em `tests/README.md`.
- `build/`, `dist/` e `contexts/src/` não são versionados (`.gitignore`); `build/` já foi
  destrackeado com `git rm -r --cached`.
- Limitação conhecida na poc/09: chaff/flare saem no Tacview como `Misc`/`Grey` em vez de
  `Misc+Decoy+Chaff`/`+Flare` — soma das armadilhas 4, 6 e 7 do xtacview.
