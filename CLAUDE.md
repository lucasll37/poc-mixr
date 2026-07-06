# poc-mixr

## O que é este projeto

Prova de conceito (PoC) para desenvolvimento de **novos modelos de simulação**
sobre o framework [MIXR](https://mixr.dev) (Mixed Reality Simulation Platform).
O objetivo de longo prazo é evoluir, de forma incremental, modelos próprios
(dynamics models, players, sensores, sistemas, etc.) reutilizando o build
system já funcional.

O framework MIXR em si **não é o objeto de desenvolvimento** — ele é tratado
como dependência/referência.

## Layout do repositório

```
poc-mixr/
├── mixr/          # CÓPIA do framework MIXR (git próprio, ignorado pelo .gitignore).
│                  # Serve só de CONTEXTO/referência de como as classes do framework
│                  # são implementadas (não editar; não é publicado por este repo).
├── examples/      # Exemplos oficiais upstream do MIXR (mainCockpit, demoEfis, etc.),
│                  # vendorizados como referência de uso do framework.
├── BehaviorTree.CPP/  # CÓPIA do repo upstream de BehaviorTree.CPP, só para CONSULTA de
│                      # API/conceitos (não editar). ATENÇÃO: é a v4.9 upstream, mas o
│                      # pacote Conan realmente linkado é behaviortree.cpp.asa/3.5.6, cujos
│                      # headers instalados ficam em <prefix>/include/behaviortree_cpp_v3/
│                      # (namespace/API de v3, não v4 — checar sempre contra os headers
│                      # instalados, não contra este vendor, antes de usar uma API nova).
├── config/        # Referência histórica (não é mais onde a PoC evolui, ver poc/ abaixo):
│   ├── mainSim1/  #   - mais simples: usa src/main.cpp da raiz + configs .epp
│   ├── mainSim2/  #   - + SimStation/SimPlayer/SimIoHandler/InstrumentPanel + display GLUT
│   └── mainSim3/  #   - mais completa: Station/Display/MapPage, ighost (CIGI/POV), DIS, terrain
├── poc/           # SUBPROJETOS INCREMENTAIS que coexistem e buildam em paralelo
│   │              # (é aqui que novos modelos são desenvolvidos, um por pasta numerada):
│   ├── 01-flying-aircraft/  # Aircraft (F-16A) + RacModel comandado a virar/subir/acelerar;
│   │                        # imprime telemetria a cada segundo simulado, roda 30s e encerra
│   ├── 02-behavior-tree/    # BehaviorTree.CPP v3 (lib externa, sem depender do MIXR):
│   │                        # sentinela patrulha/recarrega via Fallback+Sequence+blackboard
│   ├── 03-bt-autopilot/     # MIXR + BehaviorTree.CPP juntos, com domain/ (regras de negócio
│   │                        # puras) separado de bt/ (nós/factory da árvore) e mixr_factory
│   │                        # (factory dos objetos MIXR) — ver estrutura completa abaixo
│   ├── 04-jsbsim-6dof/      # JSBSimModel (aerodinâmica 6-DOF real, aeronave F4N, dados
│   │                        # próprios em data/jsbsim/) + recorder de rede tacview/ que
│   │                        # transmite telemetria ao vivo para o Tacview (Real-Time
│   │                        # Telemetry) em 127.0.0.1:1234
│   └── 05-formation-flight/ # Esquadrilha de 5 aeronaves (1 lead JSBSimModel/F4N pilotado por
│                            # teclado + 4 wingmen RacModel autônomos via BT), terreno real
│                            # (SRTM), Autopilot.followTheLeadMode nativo p/ formação, Route/
│                            # Steerpoint nativos p/ RTB, recorder nativo, Tacview estendido —
│                            # ver seção própria mais abaixo (maior subprojeto até agora)
├── src/           # main.cpp mínimo ("mixr-hello"): cria Station+Simulation vazia,
│                  # roda RESET_EVENT + updateTC()/updateData() manualmente
├── include/       # headers do próprio projeto (vazio por ora, .gitkeep)
├── conanfile.py   # consome o pacote Conan mixr/1.0.5 (binário pré-publicado)
├── meson.build    # raiz: resolve mixr via pkg-config; builda src/main.cpp e cada poc/NN-slug
└── Makefile       # orquestra Conan + Meson
```

`config/mainSimN/` continua no repo só como referência histórica/de leitura
(cada um tem seu próprio `main.cpp` + `configs/*.epp`, mas não tem alvo de
build integrado). **A partir de agora, cada subprojeto novo entra em
`poc/NN-slug/`** com sua própria estrutura:

```
poc/NN-slug/
├── meson.build      # só faz subdir('./src')
├── src/
│   ├── main.cpp
│   └── meson.build  # define o executável (nome = slug, sem o NN-)
└── configs/
    └── scenario.epp
```

Para um subprojeto mais elaborado, que precise separar regras de negócio de
adaptadores/factories (ex.: quando mistura MIXR com outra lib, como
`03-bt-autopilot`), a estrutura interna de `src/`/`include/` pode crescer
assim (mantendo `meson.build` e `configs/` no mesmo lugar):

```
poc/NN-slug/
├── meson.build
├── configs/
│   └── scenario.epp (+ outros arquivos de config específicos da lib externa)
├── include/
│   ├── domain/          # regras de negócio puras (sem MIXR/lib externa) — testável isolado
│   ├── <lib>/            # adaptadores finos para a lib externa (ex.: nós de uma árvore)
│   │   └── <lib>_factory.hpp  # registra os adaptadores na factory da lib
│   └── mixr_factory.hpp  # factory dos objetos MIXR (Station/Aircraft/...)
└── src/
    ├── meson.build       # inclui local_inc_dir = include_directories('../include')
    ├── main.cpp          # só orquestra: constrói Station + integra a lib, roda o loop
    ├── mixr_factory.cpp
    ├── domain/...
    └── <lib>/...
```

Regra geral: `main.cpp` fica fino (orquestração); "o que fazer" mora em
`domain/` (sem depender de MIXR nem da lib externa — testável sozinho); "como
conectar" mora nas factories/adaptadores. Ver `poc/03-bt-autopilot/` como
referência completa desse padrão.

Todos os subprojetos em `poc/` coexistem: o `meson.build` raiz dá `subdir()`
em cada um (variáveis compartilhadas `thread_dep`, `mixr_dep`, `mixr_libdir`,
`behavior_tree_dep`, `inc_dir` já ficam definidas antes desses subdirs), e
todos são compilados juntos por `make build`. Um subprojeto não precisa usar
todas as dependências (ex.: `02-behavior-tree` só usa `thread_dep` +
`behavior_tree_dep`, nem toca no `mixr_dep`). Ao criar `poc/03-.../`,
adicionar mais um `subdir('./poc/03-...')` no `meson.build` raiz (mesmo
padrão dos anteriores) e um alvo `run-<slug>` no `Makefile`.

## Build & Run

Toolchain: **Conan 2.x** (resolve o pacote binário `mixr/1.0.5` do remote
local) → **Meson/Ninja** (compila) → **Makefile** (orquestra tudo).

```bash
make configure             # conan install (profile debug) + meson setup
make build                 # meson compile -C build (builda TODOS os poc/NN-slug juntos)
make run                   # executa build/src/poc-mixr (mixr-hello, raiz)
make run-flying-aircraft   # executa build/poc/01-flying-aircraft/src/flying-aircraft
make run-behavior-tree     # executa build/poc/02-behavior-tree/src/behavior-tree
make run-bt-autopilot      # executa build/poc/03-bt-autopilot/src/bt-autopilot
make run-jsbsim-6dof       # executa build/poc/04-jsbsim-6dof/src/jsbsim-6dof (Tacview em 127.0.0.1:1234)
make run-formation-flight  # executa build/poc/05-formation-flight/src/formation-flight (teclado + Tacview 1234)
make install               # copia artefatos para dist/
make package               # conan create . (gera pacote deste projeto)
make clean                 # remove build/, dist/, subprojects/packagecache
make help                  # lista alvos (via comentários ## no Makefile)
```

Perfis Conan disponíveis no ambiente: `asa-debug`, `asa-release`, `default`.

Subprojetos existentes em `poc/`:

- **`01-flying-aircraft`**: `Aircraft` (F-16A) com `RacModel` como
  `dynamicsModel`, comandado (`cmdHeading`/`cmdAltitude`/`cmdSpeed`) para um
  estado diferente do inicial. Loop roda 30s simulados, imprime telemetria
  (alt/hdg/spd/lat/lon) a cada segundo e encerra sozinho — prova visual de
  que a aeronave manobra de verdade (heading e altitude convergem
  monotonicamente ao alvo comandado). `RacModel` é um dynamics model bem
  simples do próprio framework (não físico-realista); serve de baseline
  antes de modelos mais sofisticados (ex.: `JSBSimModel`).

- **`02-behavior-tree`**: usa `BehaviorTree.CPP` v3 (pacote Conan
  `behaviortree.cpp.asa/3.5.6`, independente do MIXR) para um "sentinela"
  que patrulha e recarrega com base num blackboard compartilhado
  (`Fallback[Sequence[BatteryLow, Recharge], Patrol]`, árvore em
  `configs/tree.xml`). 20 ticks, imprime o estado a cada tick. Serve de
  baseline antes de usar BT.CPP para decidir o comportamento de um player
  MIXR de verdade (ex.: acoplar a árvore ao `01-flying-aircraft`).

- **`03-bt-autopilot`**: junta MIXR + BehaviorTree.CPP. Mesma aeronave dos
  anteriores (F-16A + `RacModel`), mas sem `cmdHeading`/`cmdAltitude`/
  `cmdSpeed` fixos no `.epp` — quem comanda o `DynamicsModel` a cada tick é
  a árvore (`configs/mission.xml`, mesmo `Fallback[Sequence[FuelLow,
  ReturnToBase], Patrol]` do `02-behavior-tree`). Arquitetura em 3 camadas:
  `domain/Mission` (regras puras: patrulha cíclica entre 3 waypoints,
  consumo de combustível, decisão de RTB — sem incluir nada de MIXR/BT,
  testável isolado), `bt/nodes/*` (subclasses de `SyncActionNode`/
  `ConditionNode` que leem `Mission` e chamam `DynamicsModel::
  setCommandedHeadingD/setCommandedAltitude/setCommandedVelocityKts`) +
  `bt/bt_factory` (registra os nós na `BehaviorTreeFactory`), e
  `mixr_factory` (a factory de objetos MIXR, extraída para seu próprio
  arquivo). `Mission`/`DynamicsModel*` são injetados nos nós via blackboard
  do BT (`blackboard->set<T*>(...)` antes de `createTreeFromFile`). Rodado
  por 50s simulados: patrulha wp0→wp1→wp2, combustível cai, aos ~23s entra
  em RTB (vira de volta para o heading base), reabastece após ~6s e retoma
  a patrulha de onde parou — comportamento validado rodando de ponta a
  ponta.
  **Gotcha de unidades**: `DynamicsModel::setCommandedAltitude()` espera
  metros (não pés — converter com `mixr::base::distance::FT2M`);
  `setCommandedHeadingD()` é graus; `setCommandedVelocityKts()` é nós.

- **`04-jsbsim-6dof`**: `Aircraft` com `JSBSimModel` (aerodinâmica 6-DOF de
  verdade via JSBSim 1.1.11, não o `RacModel` simplificado) + um recorder de
  rede (`tacview/RealtimeTelemetryServer`) que implementa o protocolo
  público **Tacview Real-Time Telemetry** e transmite a simulação ao vivo
  para o Tacview em `127.0.0.1:1234`.
  - **Dados JSBSim**: modelo `F4N` (F-4 Phantom, dados públicos e não
    proprietários — o próprio `F4N.xml` declara isso no cabeçalho).
    Vendorizados em `poc/04-jsbsim-6dof/data/jsbsim/{aircraft/F4N,engine,
    systems}` a partir do checkout fonte do pacote Conan `jsbsim/1.1.11`
    (`~/.conan2/p/<hash>/s/aircraft/F4N` etc. — esse checkout é só um
    cache local de build, não uma dependência formal, por isso os dados
    foram copiados para dentro do repo em vez de referenciados por
    caminho absoluto). `rootDir` no `.epp` aponta para essa pasta local
    (caminho relativo, assume execução a partir da raiz do repo, mesma
    convenção dos demais `poc/`).
  - **Sem Autopilot/holds**: `F4N` não tem um sistema JSBSim de autopilot
    próprio (`ap/heading_hold` etc. — ver seus `<system file=.../>` no
    `F4N.xml`), então `DynamicsModel::setCommandedHeadingD/Altitude/
    VelocityKts` (que funcionam no `RacModel`) não teriam efeito aqui.
    `main.cpp` comanda a aeronave diretamente via
    `DynamicsModel::setControlStick(roll, pitch)` +
    `setThrottles(...)` (entradas de controle normalizadas -1..1, não
    "comandos" de alto nível).
  - **Gotcha de trim**: `JSBSimModel::reset()` (no MIXR) chama
    `fdmex->RunIC()` mas **não** roda `FGTrim` — a aeronave começa
    destrimada. Na prática isso causa um transiente energético nos
    primeiros segundos (velocidade lida bem acima da `initVelocity`
    comandada, decaindo gradualmente) antes de convergir para algo mais
    estável. Ganhos de controle pequenos (`0.03`, não `0.3`) evitam que
    esse transiente vire um parafuso/looping; testado rodando o binário
    real (ver `RealtimeTelemetryServer` abaixo para como validar o stream).
  - **Protocolo Tacview**: handshake
    `XtraLib.Stream.0\nTacview.RealTimeTelemetry.0\n<username>\n\0` (host
    envia primeiro; **todas** as linhas terminam em `\n`, inclusive a
    última — o `\0` é um byte extra e separado depois do último `\n`, não
    um substituto dele). A documentação oficial
    (https://www.tacview.net/documentation/realtime/en/) descreve como se
    a última linha não levasse `\n` antes do `\0`; isso **não bateu** com o
    Tacview real (erro "real-time telemetry not compatible with the host
    exporter") — o formato acima foi confirmado contra uma implementação
    de referência que de fato conecta
    (github.com/xutter/tacview-toolset/.../dataserver.py). Depois do
    handshake, o servidor também dá um `recv()` (com timeout de 1s, para
    nunca travar o loop da simulação) para consumir o handshake que o
    Tacview manda de volta, só então envia o cabeçalho ACMI — mesmo padrão
    da referência. Cabeçalho/dados: texto ACMI 2.2 puro (`FileType=`,
    `FileVersion=`, `0,ReferenceTime=...`, blocos `#<segundos>` com linhas
    `<idHex>,T=Lon|Lat|AltM|Roll|Pitch|Yaw,Name=...,Type=...,Color=...` —
    `Name`/`Type`/`Color` só na primeira aparição do objeto). Validado
    byte a byte com `nc 127.0.0.1 1234 | cat -A` enquanto o binário roda
    (isso valida os bytes só até onde o `nc` consegue simular um cliente —
    não substitui testar com o Tacview de verdade). O servidor de escuta é
    não-bloqueante (`accept()` em modo `O_NONBLOCK`): a simulação roda
    normalmente com ou sem o Tacview conectado, e reconecta sozinho se o
    cliente cair. Porta padrão: **1234** (não 1324 — trocado depois que o
    Tacview do usuário indicou esse valor).
  - **Roda indefinidamente** (Ctrl+C/`SIGINT` para encerrar — sem isso o
    binário parava sozinho depois de alguns segundos, tempo insuficiente
    para abrir o Tacview e configurar a conexão).
  - **Gotcha de rede WSL2 ↔ Windows**: ambiente de dev é WSL2 (Linux) com o
    Tacview rodando no Windows. O bind é em `0.0.0.0` (não `127.0.0.1`) —
    `127.0.0.1` sozinho depende do "localhost forwarding" do WSL2
    (Windows→WSL2), que nem sempre está disponível/confiável (rede
    corporativa, versão do WSL, etc.). Com `0.0.0.0`, se `127.0.0.1:1234`
    não conectar no Tacview, use o IP da distro (`hostname -I` dentro do
    WSL2, ex. `172.23.229.154` — muda a cada reinício) e conecte nele em
    vez de `127.0.0.1`. O binário já imprime essa dica ao iniciar.

- **`05-formation-flight`**: o subprojeto mais elaborado até agora —
  esquadrilha de 5 aeronaves (`lead` + `wing1`..`wing4`) sobre terreno real
  (tile SRTM `S23W043`, Serra dos Órgãos/RJ, baixado do mirror público AWS
  `elevation-tiles-prod` e vendorizado comprimido em
  `data/terrain/srtm/*.hgt.gz` — descomprimido no primeiro `run`).
  Estrutura completa e o processo de descoberta (o que era real vs.
  inventado no pedido original) estão em
  `/home/lima/.claude/plans/starry-tickling-cosmos.md`. Pontos que valem a
  pena não redescobrir:
  - **`edl_parser` (flex/bison) NÃO entende `#include`/`#define`
    nativamente.** Os `.epp` de exemplo do MIXR que usam `#include`
    (`saitekEVO.epp`, `route01.epp`, `dataRecorder.epp`) só funcionam
    porque passam por um preprocessador C de verdade ANTES do
    `edl_parser` — daí o `.epp` (preprocessor source) vs. `.edl` (fonte
    final) e o "`make edl`" mencionado em `examples/README.md`. As pocs
    01/03/04 nunca precisaram disso porque seus `.epp` não usavam
    `#include`. A partir da 05, `main.cpp` roda
    `g++ -E -x c -P -undef -nostdinc arquivo.epp -o arquivo.preprocessed.epp`
    antes de chamar `edl_parser` — qualquer subprojeto novo que use
    `#include`/`#define` em `.epp` precisa do mesmo passo.
  - **Um slot com o mesmo nome do arquivo `#include`d duplica a label.**
    Ex.: `dataRecorder: #include "recorder.epp"` só funciona se
    `recorder.epp` começar direto com `( DataRecorder ...)`, **sem**
    repetir `dataRecorder: ( DataRecorder ...)` dentro do arquivo incluído
    (senão vira `dataRecorder: dataRecorder: (...)`, erro de sintaxe).
  - **`numTcThreads` é slot de `simulation::Simulation`/`WorldModel`, não
    da `Station`** (`setSlotNumTcThreads` é privado — sem setter público —
    por isso o valor é injetado via substituição de template no `.epp`
    antes do parse, não um patch no objeto depois; ver
    `configs/scenario.epp.in` + `generateScenario()`/`preprocessEdl()` em
    `main.cpp`).
  - **`WorldModel::getTerrain()` só é público na versão `const`** (a
    não-const é `protected`) — chame a partir de um ponteiro/referência
    `const WorldModel*`.
  - **`BT::Blackboard::create(parent)` não herda entradas do pai
    automaticamente** com `get<T>`/`set<T>` diretos — isso só acontece com
    "port remapping" explícito (subtree ports do XML). Pra compartilhar de
    verdade um ponteiro (ex.: `FormationState*`) entre várias árvores,
    registre a mesma chave em cada blackboard filho explicitamente (ver
    `main.cpp`), não confie na hierarquia pai/filho sozinha.
  - **Formação via `Autopilot::followTheLeadMode` nativo** (não cálculo
    geodésico manual): `leadPlayerName`/`leadFollowingDistanceTrail/
    Right`/`leadFollowingDeltaAltitude` — o BT de cada wingman só decide
    *quais* 3 offsets usar (tabela em `formations.hpp`), quem converge de
    fato é o Autopilot. RTB usa `Route`/`Steerpoint`/`Navigation` +
    `Autopilot::flyCRS`/`navMode` nativos para o `lead`; os wingmen
    "chegam em casa de graça" só por continuarem em `followTheLeadMode`.
  - **F4N não tem hold-modes nativos do `Autopilot`** (mesmo gotcha da
    poc/04: sem `ap/heading_hold` etc. no JSBSim do F4N) — mantido mesmo
    assim por instrução explícita do usuário (reaproveitar a aeronave já
    validada); o operador comanda via setters nativos do `Autopilot`
    normalmente, só a tradução final pro `DynamicsModel` do F4N precisa da
    mesma ponte customizada já documentada.
  - **Teclado**: `KeyboardDevice`/`KeyboardIoHandler` (subclasses de
    `mixr::linkage::IoDevice`/`IoHandler`, termios em modo raw) — testado
    de ponta a ponta neste ambiente **sem** teclado real (sem TTY
    interativo aqui, `tcgetattr` falha graciosamente e o programa segue
    sem input); heading/altitude/velocidade por teclado, troca de
    formação e RTB **precisam de teste manual do usuário num terminal de
    verdade** — não foram validados interativamente.
  - **Afinidade de CPU e tempo por fase não são observáveis de fora**: o
    pool de threads (`numTcThreads`) é nativo e real (round-robin
    confirmado rodando — N threads criadas), mas o MIXR não expõe os
    handles internos nem faz pinning; o log de status reporta o núcleo
    real da nossa própria thread principal via `sched_getcpu()` (sem
    fingir pinning que não existe) e não tenta medir tempo por fase (as 4
    fases rodam dentro de `Simulation::updateTC()`, fora do nosso loop).
  - **Validado rodando de ponta a ponta** (sem teclado): EDL parseado,
    Station construída, pool de 23 threads T/C criado, JSBSim/F4N e
    terreno carregados, as 5 aeronaves telemetram (altitude/posição
    evoluindo de forma coerente), `RealtimeTelemetryServer` grava
    `data/recordings/mission.acmi` válido (5 objetos, replay completo) e o
    `DataRecorder` nativo grava `mission.dat`/`mission.csv` (via
    `RecorderFileWriter`/`TabPrinter`), `Ctrl+C` encerra limpo com os
    arquivos de gravação fechados corretamente.

## Arquitetura do MIXR (para criar novos modelos)

Padrão de classe (ver `mixr/include/mixr/base/macros.hpp` e `Object.hpp`):

- Toda classe herda de `mixr::base::Object` (ref-counting, RTTI própria).
- `DECLARE_SUBCLASS(ClassName, BaseClass)` no `.hpp` + `IMPLEMENT_SUBCLASS(ClassName, "FactoryName")` no `.cpp`.
- **Slot table**: parâmetros configuráveis via EDL/`.epp` são declarados com
  `BEGIN_SLOTTABLE`/`END_SLOTTABLE` + `BEGIN_SLOT_MAP`/`ON_SLOT`/`END_SLOT_MAP`,
  com métodos `setSlotX()` privados. Ver `mixr/src/models/dynamics/RacModel.cpp`
  como exemplo simples e completo.
- **Factory**: cada biblioteca (`base`, `simulation`, `models`, `terrain`,
  `interop/dis`, `graphics`, `instruments`, `ighost/*`, exemplos com
  `xzmq`...) expõe uma função `factory(name)` que faz `new` na classe cujo
  "Factory name" bate com a string usada no `.epp`. O `factory()` de cada
  `main.cpp` de exemplo encadeia essas factories (a ordem importa: a primeira
  que retornar não-nulo vence).
- **Hierarquia de modelos** (`mixr::models`, em `mixr/include/mixr/models/`):
  - `WorldModel` (a "Simulation") contém `players`.
  - `Player` (base de `Aircraft`, `Ship`, `GroundVehicle`, `LifeForm`,
    `Building`, mísseis/armas em `player/weapon`, efeitos em `player/effect`)
    agrega componentes: `DynamicsModel` (física — `RacModel` simples,
    `JSBSimModel` via JSBSim, `LaeroModel`), sensores (`system/`:
    `Antenna`, `RfSensor`, `Gimbal`, `Autopilot`, `Datalink`, etc.) e
    navegação (`navigation/`: `Gps`, `Ins`, `Route`, `Steerpoint`).
  - Um novo "modelo" tipicamente = subclasse de `DynamicsModel` (física nova),
    de `Player`/`Aircraft`/etc. (novo tipo de veículo/entidade), ou de
    `System` (novo subsistema/sensor).
- Configuração de cenário: arquivos `.epp` (dialeto EDL), parseados por
  `mixr::base::edl_parser` usando a `factory()` do programa. Ver
  `poc/01-flying-aircraft/configs/scenario.epp` (ou o histórico
  `config/mainSim1/configs/test0.epp`) como referência mínima
  (Station → WorldModel → players → Aircraft → components.dynamicsModel).

## Convenções para adicionar um novo subprojeto/modelo nesta PoC

1. Criar `poc/NN-slug/` (próximo número sequencial) com `meson.build`,
   `src/main.cpp` + `src/meson.build`, `configs/scenario.epp` — usar
   `poc/01-flying-aircraft/` como template.
2. Se o modelo novo for uma classe própria (não uma do framework, tipo
   `RacModel`), o header/fonte entram em `include/`/`src/` do subprojeto (ou
   no `include/`/`src/` raiz se for compartilhado entre subprojetos),
   **nunca** dentro de `mixr/` (só referência/dependência externa).
3. Registrar a classe nova na função `factory()` do `main.cpp` do
   subprojeto (padrão: tentar a factory local primeiro, cair para as do
   framework — ver `mixr::models::factory` etc. em `poc/01-flying-aircraft/src/main.cpp`).
4. Configurar/instanciar via `configs/scenario.epp` (slots do componente).
5. No `meson.build` raiz, adicionar `subdir('./poc/NN-slug')` (após os já
   existentes) e, se o executável tiver artefato próprio, referenciá-lo no
   `summary()` de Build Artifacts.
6. No `Makefile`, adicionar um alvo `run-<slug>` apontando para
   `$(BUILD_DIR)/poc/NN-slug/src/<slug>`.

## Estado atual / observações

- `build/` e `dist/` (artefatos gerados) estão **versionados no git** (950
  arquivos rastreados), embora já listados no `.gitignore` local (mudança
  ainda não commitada, adicionando `dist` e `mixr` à lista que já tinha
  `build`). Vale decidir e rodar `git rm -r --cached build dist` antes do
  próximo commit para parar de rastreá-los.
- `mixr/` tem `.git` próprio — é um clone independente, não um submodule
  configurado no repo principal.
