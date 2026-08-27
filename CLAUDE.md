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

Cada PoC vive em `src/NN-slug/`, é um executável independente, e todas compilam juntas.
A progressão é incremental: cada número reaproveita/contrapõe o anterior (ex.: `12` refaz
`11` com 6-DOF real + UBF; `13` refaz `12` usando a pilha nativa do framework no lugar das
classes próprias; `14` refaz `13` trocando só o agente do UBF — `SimAgent` nativo, que decide em
`updateData()`, por um `AgentTC` próprio, que decide na fase 3 do frame).

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

Binários ficam em `build/src/NN-slug/src/<slug>` (o `<slug>` é o nome da pasta sem o `NN-`), e
cada um tem um alvo `run-<slug>` no Makefile.

**Todos os binários leem `configs/`/`data/` por caminho relativo (`./src/NN-slug/...`) e devem
ser executados a partir da raiz do repositório:**

```bash
./build/src/01-flying-aircraft/src/flying-aircraft
```

Opções de linha de comando das pocs: `-f <arquivo>` (cenário alternativo) em todas;
`-threads <N>` e `-deterministic <N>` nas pocs 11/12/13/14.

**Não há suíte de testes.** A verificação automatizada existente é de **determinismo**
(alvos `check-custom-models`, `check-jsbsim-ubf`, `check-native-stack`, `check-tc-agent`): roda N
frames de passo fixo com 1, 2 e 4 threads T/C e compara os dumps `frame=` — todos devem ser
idênticos. Os mesmos alvos servem de modelo para validar qualquer poc nova que use multithread.
O alvo `compare-13-14` lista o que difere entre as pocs 13 e 14 (deve ser só o agente).

**AddressSanitizer**: `meson configure build -Dasan=true && make build` — liga ASan apenas na
`13-native-stack` (único alvo que consome `asan_cpp_args`/`asan_link_args`).

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
src/NN-slug/
├── meson.build            # só faz subdir('./src')
├── configs/scenario.epp   # cenário EDL (+ .xml das árvores de comportamento)
├── data/                  # dados vendorizados (jsbsim/, terrain/, recordings/)
├── include/
│   ├── app/               # as etapas da aplicação, uma questão por arquivo (ver abaixo)
│   ├── domain/            # regras de negócio puras — sem MIXR, sem BT — testável isolado
│   ├── bt/ | ubf/         # adaptadores da lib externa (nós da árvore, comportamentos UBF)
│   ├── x<nome>/           # classes MIXR próprias (namespace mixr::x<nome>) + factory própria
│   └── mixr_factory.hpp   # factory dos objetos MIXR deste subprojeto
└── src/
    ├── meson.build        # define o executable() — nome = slug
    ├── main.cpp           # FINO: só orquestra (chama os módulos de app/ na ordem)
    └── ... (espelha include/)
```

Regra geral: "o que fazer" mora em `domain/`; "como conectar" mora nas factories/adaptadores;
`main.cpp` não implementa comportamento. `src/03-bt-autopilot/` e `src/13-native-stack/` são as
referências completas do padrão.

**Um arquivo, uma questão.** O que antes era um `main.cpp` de ~450 linhas está quebrado em
`app/`, no namespace `app`, e cada header abre com o "por que" daquele passo:

| módulo | questão |
|---|---|
| `app/Options.*` | `argv` → struct (`-f`, `-threads`, `-deterministic`) |
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
         ( TacviewOutput port: 1234 callsign: "poc-mixr/<slug>"
           fileName: "./src/NN-slug/data/recordings/mission.acmi"
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
no WSL2 o loopback depende de localhost forwarding — se falhar, use `hostname -I`. Porta 1234.

### `shared/xclock` — controle de velocidade do tempo (acelerar / frear / pausar)

Mesmo padrão `shared/x<nome>` do `xtacview` (factory própria, classes em `mixr::xclock`, exposta
como `xclock_dep`). O cenário declara **`( ClockStation )` no lugar de `( Station )`** — é uma
`simulation::Station` com um único override. Usada pelas pocs 13 e 14; trocar de volta para
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
`isFrozen()` (`Agent.cpp:59-62`) — com a simulação pausada, o `SimAgent` da poc/13 continua
avaliando sobre um mundo estático (nada se move; a decisão só não para). O `FlightAgentTC` da
poc/14 decide na fase 3, dentro do frame, então para junto.

`-deterministic` **não é afetado**: chama `station->tcFrame(dt)` direto, sem passar por
`processTimeCriticalTasks()`. `make check-native-stack`/`check-tc-agent` seguem valendo.

Sem TTY (pipe, redirecionamento, CI) o `tcgetattr()` de `ConsoleKeyboard` falha, `isActive()`
fica `false` e a simulação roda normalmente, só sem teclado.

## Ao adicionar um subprojeto novo

1. Criar `src/NN-slug/` (próximo número) seguindo a estrutura acima; sempre com
   `include/mixr_factory.hpp` + `src/mixr_factory.cpp` (a factory **não** fica inline no `main.cpp`).
2. Adicionar `subdir('./NN-slug')` em [src/meson.build](src/meson.build) e o alvo no `summary()`
   de Build Artifacts do [meson.build](meson.build) raiz.
3. Adicionar o alvo `run-<slug>` no [Makefile](Makefile) — apontando para
   `$(BUILD_DIR)/src/NN-slug/src/<slug>`.
4. **Declarar o rpath no `executable()`**: `link_args: rpath_link_args` +
   `install_rpath`/`build_rpath` com `mixr_libdir`, `bt_libdir`, `mixr_bt_libdir` ou
   `mixr_bt_jsbsim_libdir` conforme as dependências usadas (variáveis definidas no `meson.build` raiz).
5. Para exportar ao Tacview: `dataRecorder:` na `Station` com `( TacviewOutput ... )`,
   `dataLogTime:` em **cada** player, `mixr::xtacview::factory` + `mixr::recorder::factory`
   encadeadas no `mixr_factory.cpp`, e `xtacview_dep` no `meson.build`.

### Gotcha: rpath das dependências Conan (`dist/` vs `build/`)

As `.so` do MIXR/BT.CPP/JSBSim vivem no cache do Conan, fora de qualquer caminho do loader.

- **O Meson descarta o `build_rpath` no `meson install`** (por design): um alvo sem
  `install_rpath` roda de `build/` e falha em `dist/bin/<slug>` com
  `error while loading shared libraries`.
- **`-Wl,--disable-new-dtags` (em `rpath_link_args`) não é decorativo**: força `DT_RPATH` em vez
  de `DT_RUNPATH`. RPATH é herdado pelas dependências transitivas, RUNPATH não — sem ele o loader
  resolve as libs nomeadas no executável mas falha entre elas (`libmixr_models.so` → `libmixr_base.so`).

Conferência após `make install`: `ldd dist/bin/<slug> | grep 'not found'` (silêncio = ok) e
`readelf -d dist/bin/<slug> | grep -E 'RPATH|RUNPATH'`.

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
  `poc/NN-slug`** — é só prosa, nenhum caminho depende disso.
- `docs/` está vazio.
- `build/`, `dist/` e `contexts/src/` não são versionados (`.gitignore`); `build/` já foi
  destrackeado com `git rm -r --cached`.
- Limitação conhecida na poc/09: chaff/flare saem no Tacview como `Misc`/`Grey` em vez de
  `Misc+Decoy+Chaff`/`+Flare` — soma das armadilhas 4, 6 e 7 do xtacview.
