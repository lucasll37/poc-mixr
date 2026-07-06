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
│   └── 02-behavior-tree/    # BehaviorTree.CPP v3 (lib externa, sem depender do MIXR):
│                            # sentinela patrulha/recarrega via Fallback+Sequence+blackboard
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
