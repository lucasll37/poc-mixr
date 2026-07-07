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
│   ├── 05-formation-flight/ # Esquadrilha de 5 aeronaves (1 lead JSBSimModel/F4N pilotado por
│   │                        # teclado + 4 wingmen RacModel autônomos via BT), terreno real
│   │                        # (SRTM), Autopilot.followTheLeadMode nativo p/ formação, Route/
│   │                        # Steerpoint nativos p/ RTB, recorder nativo, Tacview estendido —
│   │                        # ver seção própria mais abaixo (maior subprojeto até agora)
│   ├── 06-radar-detection/  # Radar de busca 100% nativo (Antenna/Gimbal + Tws + AirTrkMgr,
│   │                        # dentro de SensorMgr/OnboardComputer) detectando uma 2ª aeronave
│   │                        # que se aproxima — mesmo padrão de examples/testRadar
│   ├── 07-radar-intercept/  # Integra 04+05+06 num só cenário: hunter 6-DOF (JSBSimModel/F4N)
│   │                        # com o radar nativo da poc/06 detectando 3 targets (RacModel,
│   │                        # RCS diferentes), as 4 aeronaves exportadas ao vivo pro Tacview
│   │                        # (com eventos de detecção do radar no replay)
│   ├── 08-event-relay/      # Mesmas features (6-DOF/radar/Tacview), mas os componentes se
│   │                        # comunicam via evento nativo (Component::event()/send(),
│   │                        # CONTACT_EVENT) que percorre a árvore — local (irmão no mesmo
│   │                        # player) e remoto (outro player, achado via WorldModel, mesma
│   │                        # técnica do Datalink nativo) — não chamada C++ direta
│   ├── 09-chaff-flare/      # Mesmas features (6-DOF/Tacview), lançando Chaff/Flare nativos
│   │                        # (efeitos do MIXR, mesmo mecanismo de release de arma) via
│   │                        # StoresMgr — aparecem/animam/somem no Tacview com os ícones
│   │                        # corretos (Air+FixedWing / Misc+Decoy+Chaff / Misc+Decoy+Flare)
│   └── 10-satellite-constellation/ # 4 satélites LEO (SpaceVehicle "puro", sem dynamicsModel:
│                            # MIXR não tem propagador orbital nativo) com órbita 2-body própria
│                            # aplicada via Player::setGeocPosition(ecef, slaved=true); tempo
│                            # acelerado via Station::fastForwardRate nativo (slot + setter em
│                            # runtime, tecla +/-); Tacview igual às demais pocs
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
make run-radar-detection  # executa build/poc/06-radar-detection/src/radar-detection
make run-radar-intercept  # executa build/poc/07-radar-intercept/src/radar-intercept (Tacview 1234)
make run-event-relay      # executa build/poc/08-event-relay/src/event-relay (Tacview 1234)
make run-chaff-flare      # executa build/poc/09-chaff-flare/src/chaff-flare (Tacview 1234)
make run-satellite-constellation # executa build/poc/10-satellite-constellation/src/satellite-constellation (Tacview 1234)
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

- **`06-radar-detection`**: radar de busca **100% nativo** detectando uma
  segunda aeronave — nenhum código próprio de detecção, ganho de antena,
  RCS ou correlação de pista, tudo delegado ao framework. Réplica
  simplificada de `examples/testRadar/configs/test1.epp` (mesmo padrão de
  `examples/mainCockpit/configs/player01.epp`): `antennas: (Gimbal
  components: { radar: (Antenna ...) })` com `Antenna` (que já é um
  `ScanGimbal`, então `searchVolume`/`numBars` fazem a varredura mecânica
  sem precisar de um componente `ScanGimbal` separado) → `sensors:
  (SensorMgr components: { (Tws trackManagerName: ... antennaName: radar
  powerPeak: ... frequency: ... PRF: ... ranges: [...] initRangeIdx: ...)
  })` → `obc: (OnboardComputer components: { twsTrkMgr: (AirTrkMgr
  maxTracks: ... positionGate: ... rangeGate: ... velocityGate: ...) })`.
  O alvo só precisa de uma `signature: (SigSphere radius: ...)` — é dela
  que `Player::onRfEmissionEvent()`/`Radar::receive()` calculam o RCS e a
  equação do radar (sinal/ruído vs. `threshold`) pra decidir detecção; não
  precisa de nenhum sensor próprio pra ser detectável pelo radar do outro.
  `main.cpp` só orquestra: consulta `Player::getOnboardComputer()->
  getTrackManagerByName("twsTrkMgr")->getTrackList(...)` a cada tick e
  imprime quando aparece um ID de pista novo (`Track::getTrackID/getRange/
  getTrueAzimuthD/getTarget`) — zero lógica de detecção em C++.
  **Também usa `#include`** (`gainPattern.epp`, mesmo padrão de
  `examples/testRadar`), então precisa do mesmo passo de preprocessador C
  documentado no gotcha da poc/05 (ver `preprocessEdl()` no `main.cpp`).
  **Validado rodando de ponta a ponta**: cenário com "hunter" parado
  olhando pro norte e "target" começando 45 NM ao norte fechando a 400kts
  — 0 pistas até o alvo se aproximar, detecção real da pista nº2000 em
  ~57s de simulação (dentro do alcance configurado de 40 NM, a uma
  distância de detecção efetiva de ~24.6 NM — a diferença entre o range
  nominal e o de detecção real é esperada, é a equação do radar de
  verdade rodando, não um gate booleano de distância), pista permanece
  ativa e correlacionada ao player `target` pelo resto da execução.

- **`07-radar-intercept`**: integra 04+05+06 num só cenário, a pedido
  explícito do usuário ("faltou integrar isso num cenário com demais
  aeronaves, tacview, 6dof"). `hunter` = mesma aeronave/dados JSBSimModel/
  F4N da poc/04 (`rootDir` apontando direto pra
  `poc/04-jsbsim-6dof/data/jsbsim/`, sem duplicar dados) carregando o
  mesmo radar 100% nativo da poc/06; três `target1/2/3` (RacModel) com
  `signature:` (RCS) diferentes — 2.0/4.0/1.0 — a bearings/alcances
  diferentes (0°/45NM, +30°/50NM, -20°/35NM). `tacview/
  RealtimeTelemetryServer` (cópia da extensão multi-aeronave da poc/05,
  não a versão single-aircraft da poc/04) exporta as 4 aeronaves ao vivo e
  grava cada detecção de pista nova como `Event=Message` (visível
  correlacionado no replay do Tacview).
  **Mesmo gotcha de trim da poc/04, mais pronunciado aqui**: como o
  `main.cpp` originalmente só sustentava a manete (sem nenhum toque no
  stick), o F4N destrimado perdia altitude continuamente ao longo de uma
  sessão longa (chegou a perder ~4500 ft em 45s rodando sem correção).
  Adicionado um autonivelamento simples (proporcional ao pitch atual, só
  `setControlStick(0, pitchCorrection)`, mesmo princípio da poc/04) que
  reduz bastante a taxa de descida — não é um piloto automático de
  verdade (F4N não tem hold nativo, gotcha já conhecido), só suficiente
  pra manter a geometria do radar razoável numa sessão mais longa.
  **Validado rodando de ponta a ponta**: as 3 detecções acontecem quase
  imediatamente (diferente da poc/06 — aqui `maxRange2PlayersOfInterest`
  ficou em 60NM e os 3 targets já nascem dentro desse alcance, então não
  há uma aproximação gradual pra assistir); depois de ~14s uma das 3
  pistas (`target2`) é perdida e não retorna — comportamento real de
  correlação/scan do `Tws` nativo (idade da pista, geometria mudando), não
  um bug introduzido por nós. Arquivo `.acmi` gravado confirmado válido:
  4 objetos declarados corretamente + 3 linhas `Event=Message`.

- **`08-event-relay`**: mesmas features de 04/06/07 (6-DOF, radar nativo,
  Tacview), mas a pedido explícito do usuário reestruturada em torno do
  **mecanismo nativo de eventos** (`mixr::base::Component::event()`/
  `send()`) em vez de main.cpp chamar objetos diretamente. Investigação
  prévia (antes de escrever qualquer código) revelou como esse mecanismo
  realmente funciona — importante não presumir "pub/sub distribuído
  genérico", porque não é isso:
  - `Component::event(int, Object*)` despacha via a tabela gerada pelas
    macros `BEGIN_EVENT_HANDLER`/`ON_EVENT`/`ON_EVENT_OBJ`/
    `END_EVENT_HANDLER` (`mixr/include/mixr/base/macros.hpp`) — cada
    classe registra que eventos trata escrevendo essas macros; eventos sem
    handler correspondente **não propagam automaticamente** (só eventos de
    tecla, valor ≤ `MAX_KEY_EVENT`=999, sobem pro `container()`; eventos
    não-tecla sem handler simplesmente morrem).
  - `Component::send(nome, evento, valor, SendData&)` **sempre resolve o
    nome nos FILHOS de quem chama `send()`**, não em si mesmo. Isso pegou
    um bug real: `RadarContactRelay::process()` chamava `send(...)` nele
    mesmo (`this`) tentando alcançar `"localAlert"`, um IRMÃO seu (filho
    do `hunter`, não do `RadarContactRelay`) — o lookup falhava
    silenciosamente (`send()` só retorna `false`, não lança nem loga
    nada). Corrigido chamando `getOwnship()->send("localAlert", ...)` —
    ou seja, `send()` no CONTAINER certo, não em `this`. Achado rodando o
    binário real e reparando que só o alerta remoto aparecia, nunca o
    local.
  - Não existe entrega automática "pra árvore inteira" nem entre players
    quaisquer: `mixr::models::system::Datalink::sendMessage()` (o
    mecanismo nativo mais próximo de "distribuído" entre players)
    internamente só itera a lista de players do `WorldModel` e chama
    `player->event(DATALINK_MESSAGE, msg)` direto no ponteiro achado —
    **mesma técnica que `RadarContactRelay` usa** pra alcançar o
    `controller` (`WorldModel::getPlayers()->findByName(...)` seguido de
    `->send(componenteRemoto, evento, msg, SendData&)`). Não há
    roteamento declarado em EDL — toda a "assinatura" de quem recebe o
    quê é código C++ (a tabela `BEGIN_EVENT_HANDLER` decide o que a classe
    aceita; as chamadas `send()`/`event()` decidem quem recebe).
  - Eventos customizados começam em `USER_EVENTS=2000`
    (`mixr/include/mixr/base/eventTokens.hpp`) — usado `CONTACT_EVENT=2001`.
  - `System::process(dt)` (fase 3 do frame TC, a mesma fase em que o
    `Radar` nativo esvazia sua fila de detecções pro `TrackManager`) é o
    hook certo pra lógica de "depois que sensores rodaram, decida algo" —
    `RadarContactRelay` usa exatamente essa fase, não `updateData()`.
  - `Component::updateData(dt)` **cascade nativamente pros filhos**
    (chama `updateData()` de cada componente da lista automaticamente) —
    por isso `main.cpp` não chama nada do `RadarContactRelay`/
    `AlertReceiver` explicitamente: um `station->updateData(dt)` já basta
    pra fase 3 rodar em cada `System` da árvore sozinha.
  - Payload de evento (`RadarContactMessage`) é um `mixr::base::Object`
    de verdade (`DECLARE_SUBCLASS`/`IMPLEMENT_SUBCLASS` completos) — é
    assim que o framework já transporta dados em eventos (emissions de
    RF, mensagens de datalink), não um struct solto.
  - **Validado rodando de ponta a ponta**: pista detectada pelo `Tws`
    nativo aos ~57s (mesma geometria/tempos da poc/06) dispara
    `RadarContactRelay`, que entrega o alerta **local** (`AlertReceiver`
    dentro do próprio `hunter`) E **remoto** (`AlertReceiver` dentro do
    `controller`, outra aeronave) — os dois hops confirmados no log e
    também gravados como `Event=Message` no `.acmi`, cada um associado ao
    ID do objeto correto (hunter vs. controller) no Tacview.

- **`09-chaff-flare`**: mesmas features de 04/07/08 (6-DOF, Tacview), agora
  lançando contramedidas. `mixr::models::Chaff`/`Flare`/`Decoy`
  (`mixr/include/mixr/models/player/effect/`) são subclasses de
  `Effect`→`AbstractWeapon`→`Player` — "podem ser released e virar
  players independentes", exatamente como uma arma de verdade. Não têm
  slots próprios nem RCS/IR signature por padrão (`Effect` só declara
  `dragIndex`; `signature`/`irSignature` são slots do `Player` base, nulos
  por padrão — só importariam se houvesse um radar/seeker de verdade
  tentando ser enganado, fora do escopo desta poc). Armazenados num
  `StoresMgr` comum (mesmo padrão de `stores: { N: (Tipo ...) }` já usado
  com `AamMissile` na poc/03) e liberados via `StoresMgr::
  releaseOneChaff()`/`releaseOneFlare()` — não existe nenhum "dispenser"
  dedicado, é o mesmo mecanismo genérico de release de arma.
  - **Tags ACMI conferidas contra a documentação oficial do Tacview**:
    aeronave `Air+FixedWing`; chaff `Misc+Decoy+Chaff`; flare
    `Misc+Decoy+Flare` — são essas tags que fazem o Tacview desenhar o
    ícone/partícula correto de cada tipo (não há propriedade adicional
    de "animação" a configurar; o Tacview trata isso sozinho a partir do
    `Type=`).
  - **Ciclo de vida nativo**: `Effect::updateTOF()` detona sozinho
    (`DETONATE_NONE`) após `maxTOF` (10s por padrão, nenhum slot nosso
    alterando isso) — o "desaparecimento" no Tacview é só o nosso
    `main.cpp` espelhando esse mesmo prazo com uma linha ACMI `-<id>`
    (adicionado `RealtimeTelemetryServer::removeObject()` pra isso; sem
    remoção explícita, o objeto ficaria "fantasma" parado no replay).
  - **Gotcha real encontrado rodando o binário**: ao liberar (`release()`
    em `AbstractWeapon.cpp`), o clone do efeito passa por `reset()`
    usando os slots `initXPos/initY/initAlt` (não configurados nos
    nossos `Chaff`/`Flare` — ficam em 0/0/0, a origem do `WorldModel`) e
    só herda a posição/velocidade real do lançador no primeiro
    `dynamics()` nativo, que roda na thread T/C separada — por 1-2 frames
    (~0.1-0.2s) o objeto reportava altitude 0 antes de "grudar" na
    altitude real do `hunter`. Corrigido no `main.cpp` (não em `mixr/`):
    um "warm-up" simples que só começa a exportar cada chaff/flare pro
    Tacview quando a altitude relatada já é plausível (`> 100m`), evitando
    mandar esse frame de transição pro replay.
  - **Validado rodando de ponta a ponta**: par chaff+flare liberado a
    cada 15s (primeiro aos 5s), aparecendo já na posição correta do
    `hunter`, caindo/derivando por ~10s (dinâmica nativa do `Effect`, sem
    física escrita por nós) e desaparecendo do `.acmi`/stream exatamente
    no prazo esperado — confirmado tanto no log do console quanto
    inspecionando o `.acmi` gravado (linhas `Name=chaff,Type=Misc+Decoy+
    Chaff`/`Name=flare,Type=Misc+Decoy+Flare` na primeira aparição, linhas
    `-<id>` no desaparecimento).

- **`10-satellite-constellation`**: constelação de 4 satélites LEO (`sat1`-
  `sat4`, altitude 780km, inclinação 53°, mesmo plano orbital, defasados
  90° em argumento de latitude — um "walker train" de 1 plano). Pedido do
  usuário: "constelação de 4 satélites orbitais... velocidade acelerada...
  respeite os 6dof e tacview".
  - **MIXR não tem nenhum propagador orbital nativo.** `SpaceVehicle`/
    `UnmannedSpaceVehicle`/`MannedSpaceVehicle`/`BoosterSpaceVehicle`/
    `SpaceDynamicsModel` (`mixr/src/models/player/space/*.cpp`) existem só
    como stubs de classe (RTTI/slot table), sem nenhuma física real —
    confirmado lendo o fonte antes de assumir que existia algo
    reaproveitável. Cada satélite aqui é um `SpaceVehicle` **sem**
    `dynamicsModel` nenhum (`Player::dynamics()` já tolera
    `getDynamicsModel() == nullptr`, só pula essa parte).
  - **O que É nativo**: `Player::setGeocPosition(const Vec3d& ecef, bool
    slaved=true)` — mesmo mecanismo que o próprio `NetIO` (DIS) do MIXR
    usa pra posicionar entidades remotas por fora do `dynamics()` de cada
    tick. Confirmado lendo `Player::positionUpdate()`
    (`mixr/src/models/player/Player.cpp`): quando `posSlaved`/`altSlaved`
    ficam `true` (setados pelo `slaved=true` do `setGeocPosition`), a
    integração de posição nativa por velocidade vira no-op — ou seja, com
    `slaved=true` a posição do player é **100% ditada** pelo que
    chamarmos a cada tick, sem nenhuma interferência da física própria do
    framework. `mixr::base::nav::convertGeod2Ecef()`/`convertEcef2Geod()`
    (`nav_utils.hpp`) fazem a conversão LLA↔ECEF; `mixr::base::nav::
    ERADM` é o raio equatorial WGS84 reaproveitado no cálculo da órbita
    (`poc/10-satellite-constellation/src/orbit.cpp`) em vez de duplicar a
    constante.
  - **Mecânica orbital em si é toda nossa** (`include/orbit.hpp` +
    `src/orbit.cpp`): 2-body circular + rotação da Terra (equações
    clássicas de ground track: `n=sqrt(mu/a³)`, `u(t)=u0+n·t`,
    `lat=asin(sin(i)·sin(u))`, `lon=RAAN+atan2(cos(i)·sin(u),cos(u)) -
    earthRotRate·t`). `mu`=398600.4418 km³/s² e a taxa de rotação sideral
    (7.2921150e-5 rad/s) são constantes físicas padrão, não existem em
    lugar nenhum do MIXR pra reaproveitar. Com altitude 780km, período
    calculado ≈ 6027s (~100.4 min) — confirmado batendo com o valor
    impresso rodando o binário real.
  - **Aceleração de tempo é o mecanismo NATIVO `Station::fastForwardRate`**
    (slot do `Station`, não da `Simulation`/`WorldModel` — não confundir
    com o gotcha do `numTcThreads` da poc/05, que é o oposto). Rastreado
    até `StationTcPeriodicThread::userFunc()`
    (`mixr/src/simulation/StationTcPeriodicThread.cpp`), que chama
    `Station::processTimeCriticalTasks(dt)`
    (`mixr/src/simulation/Station.cpp`) — essa função faz
    `for (jj=0; jj<getFastForwardRate(); jj++) tcFrame(dt);`, ou seja, a
    cada período real da thread T/C (que já é a mesma arquitetura
    `createTimeCriticalProcess()` usada em todas as pocs anteriores), o
    tempo simulado avança N vezes mais rápido. **Confirmado que funciona
    de fato** com essa arquitetura antes de confiar nele (não só supondo
    pela leitura da doc/slot table). `main.cpp` usa
    `station->getFastForwardRate()` como única fonte de verdade de
    velocidade: o relógio simulado (`simTime`, usado tanto pela órbita
    quanto pelos timestamps do Tacview) avança `dtReal * fastForwardRate`
    a cada iteração do laço principal — órbita e Tacview aceleram sempre
    junto com o mesmo multiplicador nativo, sem uma variável de
    velocidade paralela e desincronizada. `scenario.epp` começa em `60x`
    (senão uma órbita de ~100 min seria impraticável de assistir); tecla
    `+`/`-` chama `station->setFastForwardRate()` diretamente em runtime
    (método público comum — não precisou de um `mixr::linkage::IoDevice`
    dedicado como o teclado da poc/05, já que aqui não há nenhum outro
    canal nomeado/EDL que justificasse esse mecanismo mais pesado; mesmo
    padrão de termios em modo raw + fallback gracioso sem TTY real usado
    lá).
  - **Não existe tag ACMI oficial para satélite/espaçonave.** Verificado
    contra a documentação oficial do Tacview (`Type=` taxonomy: classes
    `Air`/`Ground`/`Sea`/`Weapon`/`Sensor`/`Navaid`/`Misc`, nenhuma
    menciona espaço) antes de inventar uma tag — usado `Misc` sozinho
    como aproximação honesta, documentado aqui em vez de fingir que existe
    uma tag "Space" oficial.
  - **Validado rodando de ponta a ponta**: período orbital impresso bate
    com o calculado (6027s), `fastForwardRate` nativo aplicado (60x),
    ground track de `sat1` evoluindo de forma coerente com inclinação 53°
    (lat 0°→14°→28° nos primeiros minutos simulados), os 4 satélites
    exportados pro Tacview com rumo real (calculado via
    `mixr::base::nav::fll2bd` entre a posição atual e um instante à
    frente, não um valor fixo) e `.acmi` gravado com os 4 objetos e
    altitude constante (780000m, como esperado numa órbita circular).

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
