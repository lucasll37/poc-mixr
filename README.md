# POC-MIXR

Metaprojeto de exploração do framework [MIXR](https://mixr.dev) (pacote Conan `mixr/1.0.5`) e do
[BehaviorTree.CPP v3](https://github.com/BehaviorTree/BehaviorTree.CPP)
(`behaviortree.cpp.asa/3.5.6`). Não é uma aplicação de simulação em si — é um conjunto de
subprojetos ("pocs", em `src/poc/`) e ferramentas que testam **como usar** o MIXR: onde a decisão roda, como um
*player* chega à simulação, em que linguagem a lógica de decisão é escrita.

O MIXR nunca é modificado — entra como dependência binária. Os **modelos** (lógica de decisão de
cada novo componente, em `models/`) são compilados à parte e carregados em runtime via `dlopen`; o core
(`./app`) nunca vê o fonte do modelo. O MIXR empacotado é headless — toda visualização é feita via
[Tacview](https://www.tacview.net/) Real-Time Telemetry (recurso da edição Advanced/paga; sem
Tacview a simulação roda normalmente, só sem visualização 3D).

> Primeira vez aqui? Comece por [`TOUR.md`](TOUR.md). Documentação, comentários e mensagens de
> console são em português do Brasil; identificadores e nomes de slot/fábrica ficam em inglês
> (originais do MIXR).

## Pré-requisitos

Conan ≥ 2.0, Meson ≥ 1.0 + Ninja, GCC ≥ 7 (C++17), pkg-config, Python 3 + `python3-dev`, gzip,
Qt5 + ZeroMQ + CMake (para o Groot), Node.js ≥ 18 (para `make open-docs`/`make open-edl`), Docker
(opcional, para `make test-ci`), Tacview Advanced (opcional, visualização).

Cinco dependências (`mixr`, `behaviortree.cpp.asa`, `jsbsim`, `openrti`, Groot) não estão no
ConanCenter — são compiladas do fonte (mesmo caminho que o CI usa). Passo a passo completo, com
pacotes de sistema exatos e uma instalação Ubuntu 24.04 testada em container →
[`INSTALL.md`](INSTALL.md).

**Elevação de terreno (SRTM)**: os cinco tiles do cenário de demonstração já vêm versionados em
`shared/data/terrain/srtm/` — nenhum download é necessário para rodar as pocs padrão. Para
cenários próprios fora dessa área (ou para baixar a cobertura do Brasil inteiro, ~12 GB, não
versionada), use `scripts/fetch_srtm.sh` — passo a passo em [`INSTALL.md`](INSTALL.md), seção
"Elevação de terreno (SRTM)". `make terrain-coverage` desenha num mapa o que de fato está em
disco.

## Build

```bash
make configure   # conan install + meson setup do core           -> build/
make sdk         # publica o SDK de plugin (ABI + libs compartilhadas core<->modelo) -> dist/
make models      # compila o(s) modelo(s)                        -> plugins/
make build       # compila o core (nao depende dos modelos)      -> build/
make install     # sincroniza plugins/ -> dist/ e instala o core -> dist/
```

No dia a dia: `make configure && make models && make install`. `build`/`install` **não** puxam
`models` (decoplados de propósito); sem `make models` antes, `make install`
sincroniza um `plugins/` vazio, só com aviso — rodar `make models && make install` de novo resolve.

```bash
make clean   # remove build/, dist/ e o deposito de plugins/
make help    # lista todos os alvos
```

## Testes

```bash
meson configure build -Dtests=true   # uma vez, depois do 'make configure'
make test-models                     # suite do(s) MODELO(s): domain + tree + native
make test                            # suite do CORE: scenario/determinism/plugin/memory/guard/...
make test-asan                       # AddressSanitizer/LeakSanitizer (build separado, lento)
```

`make test` depende de `install`, não de `models` — rode `make models` antes, ou os testes que
carregam um `.so` falham contra um `plugins/` vazio. Determinismo (mesmo resultado com 1/2/4
threads de tempo crítico) tem script próprio:

```bash
./tests/determinism/check_determinism.sh ./build/app/src/app flight 2000 flight
```

Detalhes de cada suíte → [`tests/README.md`](tests/README.md).

## CI

[`.gitlab-ci.yml`](.gitlab-ci.yml) roda a mesma sequência acima do zero, num container Docker sem
nada pré-instalado, sempre compilando as dependências privadas do fonte (nunca usa o remote Conan
privado). `make test-ci` reproduz o pipeline localmente via
[`gitlab-ci-local`](https://github.com/firecow/gitlab-ci-local):

```bash
make test-ci
```

## Rodar

Um único executável cobre todas as pocs — cada uma é um **cenário**, não um binário próprio.
Rodar sempre a partir da raiz do repositório:

```bash
./build/app/src/app -folder <pasta> -scenario <nome>   # uma poc dentro de <pasta>, direto
./build/app/src/app -file <arquivo.edl>                # cenario apontado direto (frota falcon1..4)
./build/app/src/app -folder <pasta>                     # navega a pasta, tela de selecao (TUI)
make run-app                                            # atalho para '-folder ./sandbox'
```

`-folder` é a opção que sempre funciona — lê o cenário e descobre a frota sozinho, então serve
para qualquer `.edl`, com qualquer player. `-file` é só um atalho mais direto (sem precisar de
uma subpasta com `configs/`), mas assume de antemão a frota fixa `falcon1..4` — use-o quando já
souber que o cenário segue essa convenção, ou para apontar rápido a um arquivo avulso. Na dúvida,
`-folder`.

`-numTcThreads <N>` controla quantas threads do pool de tempo crítico decidem em paralelo (uma
por player); sem a flag, o default é metade dos núcleos da máquina, e o valor pedido é sempre
limitado a `[1, núcleos-1]`.

`-numBgThreads <N>` é o equivalente para o pool nativo de
**background** (`Simulation::updateData()`, fora do frame de tempo crítico — ver
`mixr::simulation::Simulation::setSlotNumBgThreads()`); sem a flag, o default é **2**, com o mesmo clamp `[1, núcleos-1]`.

`-deterministic <N>` roda N frames de passo fixo e sai, sem TUI — é o que `tests/determinism/check_determinism.sh` usa, variando
`-numTcThreads` (1/2/4), para provar que o resultado independe de quantas threads decidem em
paralelo.

| poc | o que demonstra | comando | porta Tacview |
|---|---|---|---|
| `flight` | cadeia completa de decisão (evade → alerta → apoio) via DIS com `bandit` | `-folder src/poc/dis -scenario flight` | 1234 |
| `bandit` | o intruso: joystick ou piloto automático de reserva | `-folder src/poc/dis -scenario bandit` | 1235 |
| `python-flight` | folhas de ação da árvore escritas em Python, editáveis sem recompilar | `-folder src/poc -scenario python-flight` | 1237 |
| `onnx-policy` | decisão inteira feita por rede neural treinada | `-folder src/poc -scenario onnx-policy` | 1238 |

Todas escutam DIS na porta `3000`; nenhuma mostra uma evasão de verdade sozinha — rode junto com
`bandit`. Cada poc tem seu próprio `README.md` com as armadilhas confirmadas.

## Como o projeto se organiza

Quatro binários (`./app`, `src/node`, mais os satélites `edlcheck`/`plugininfo`) e nove projetos
Meson (o core + oito sob `models/`), orquestrados por Conan → Meson/Ninja → Makefile. `build`,
`models`, `plugins/` e `dist/` são deliberadamente **desacoplados**: compilar o core nunca precisa
dos modelos, compilar um modelo nunca precisa saber onde o core guarda artefatos — só `make
install` une as duas coisas, no único momento em que a união importa (alguém vai *rodar* algo).

```
poc-mixr/
├── app/                     painel de controle (TUI, FTXUI) -- runner UNICO das pocs, alem de
│                            dois binarios satelite: edlcheck <arquivo> (valida .edl sem levantar
│                            Station) e plugininfo (introspeccao de um .so, sem Station nenhuma)
├── src/
│   ├── poc/                 as pocs -- cada pasta e' SO DADO: configs/ + data/ + README.md,
│   │   │                    sem .cpp/.hpp proprio (quem executa e' sempre ./app ou src/node)
│   │   ├── dis/             flight + bandit, o UNICO grupo -- trocam DIS nativo do MIXR entre
│   │   │                    processos separados (o intruso mora em bandit, chega em flight so'
│   │   │                    pela rede); rodar uma sozinha e' meia demonstracao
│   │   ├── python-flight/   mesma flight, folhas de acao da arvore em .py (libs/xpyembed)
│   │   ├── onnx-policy/     mesma flight, decisao inteira por rede neural (.onnx, libs/xinfer)
│   │   ├── my-event/        sem BT/UBF -- o modelo Beacon emitindo/tratando um evento proprio
│   │   ├── rl-training/     pipeline de treino (nao produz executavel C++, so' Python)
│   │   └── c130-airdrop/, navstar3-orbit/, paratrooper-drop/   removidas como poc (so' `data/`
│   │                        de execucoes passadas; os modelos que pilotavam seguem em models/,
│   │                        servidos hoje pelos cenarios equivalentes em sandbox/)
│   ├── rl/                  wrapper Gymnasium (extensao pybind11) -- o AMBIENTE de RL contra a
│   │                        MESMA simulacao nativa (nao o pipeline de treino, que e' a poc acima)
│   ├── ui/                  editor grafico de cenario .edl (autoria offline, sem servidor,
│   │                        `.html` autocontido gerado por `make open-edl`)
│   └── node/                runner headless e independente -- roda QUALQUER .edl passado por
│                            argumento, sem TUI, so' log; cadeia de fabrica/ordem de encerramento
│                            proprias, deliberadamente sem reaproveitar nada de ./app/
├── models/                  os MODELOS -- projetos Meson A PARTE (build proprio, nao entra no
│   │                        grafo do core), compilados ANTES (`make models`) e carregados via
│   │                        dlopen; taxonomia espelha o namespace mixr::models do fork:
│   ├── players/
│   │   ├── air/             AirVehicle-derivado -- A-4 (producao, `libA-4.so`), C-130
│   │   ├── effect/          Effect-derivado (familia Chaff/Decoy/Flare) -- paratrooper
│   │   ├── ground/          GroundVehicle-derivado -- AAA (antiaerea, SamVehicle)
│   │   ├── space/           SpaceVehicle-derivado -- Navstar-3 (satelite de navegacao)
│   │   └── weapon/          Weapon-derivado (familia Missile/Bomb/Aam) -- missile
│   ├── systems/trackmanager/, dynamics/, environments/, navegation/, sensors/
│   │                        as demais subpastas da taxonomia -- hoje so' .gitkeep, sem modelo
│   ├── others/Beacon/       nao se encaixa em NENHUMA taxonomia de player -- deriva Player
│   │                        direto (mesmo padrao minimo de mixr::models::Building)
│   ├── events/              contrato de eventos que atravessam fronteira de plugin (payload +
│   │                        token) -- NAO e' um modelo, e' consumido por eles e por app/
│   └── template/            unico ponto de partida COPIAVEL (`make new-model`), com o mirror de
│                            contrato (`mirror.cpp`) que os testes de plugin do core usam
├── libs/                    bibliotecas de suporte core<->modelo, uma por pasta (libs/x<nome>);
│                            so' 6 cruzam a fronteira dlopen como shared_library() (xboard, xlog,
│                            xtrack, xrlbridge, xinfer, xpyembed) -- as demais (xtacview, xclock,
│                            xjoystick, xmsg, xterrain, xplugin) sao estaticas/header-only porque
│                            nenhum modelo as inclui
├── plugins/                 deposito FLAT dos .so (proprios OU de terceiro) + data/<modelo>/ --
│                            decoplado de dist/ de proposito; so' `make install` sincroniza os dois
├── shared/                  dados vendorizados do CENARIO, nao do subprojeto -- terreno SRTM
│                            (`data/terrain/srtm/`) e a aeronave JSBSim comum a flight/bandit
├── sandbox/                 cenarios soltos de experimentacao, fora do catalogo de pocs (achados
│                            por `-folder ./sandbox`, sem precisar registrar em lugar nenhum)
├── tests/                   suite do CORE -- determinism/, domain/, fixtures/, guard/, memory/,
│                            plugin/, scenario/, tools/ (a suite de cada MODELO vive junto dele,
│                            em models/<categoria>/<nome>/tests/, rodada por `make test-models`)
├── contexts/                material de consulta sobre o MIXR/BehaviorTree.CPP: os `.md`
│                            destilados (leitura rapida) + `src/` com o FONTE COMPLETO vendorizado
│                            e versionado (313 .cpp do fork do MIXR, mais o fonte da BT.CPP 3.5.6)
├── docs/                    manual/ (interativo, gerado do fonte por `make open-docs`),
│                            presentation/ (slide deck, `make open-presentation`), books/ (os dois
│                            manuais tecnicos completos em PDF), estudos/ (viabilidade, Markdown)
├── deps/                    receitas Conan p/ compilar mixr/behaviortree/jsbsim/openrti/groot A
│                            PARTIR DO FONTE (`scripts/deps.sh`) -- alternativa ao remote privado
├── scripts/                 deps.sh, fetch_srtm.sh (baixa tiles SRTM reais), models.sh (por tras
│                            de `make new-model`), open_browser.sh, find_groot.sh
├── tools/                   scripts Python que alimentam docs/ e src/ui/ com dado extraido do
│                            fonte real (catalogo EDL, diagrama de classes, cadeia de execucao)
├── build/                   gerado por 'make configure'/'make build' (SO o core) -- gitignored
└── dist/                    gerado por 'make install' -- gitignored, e' o que de fato RODA
    ├── bin/                 app, node, edlcheck, plugininfo
    ├── lib/mixr-plugins/    os .so sincronizados de plugins/
    ├── share/mixr-plugins/  arvores de comportamento + dados de cada modelo (ex.: A-4/jsbsim/)
    └── include/, lib/pkgconfig/   o SDK de plugin publicado por 'make sdk'
```

Alguns invariantes que a árvore acima expressa e valem para qualquer subprojeto novo:

- **Uma poc não tem código.** Toda pasta sob `src/poc/` é só `configs/` (o `.edl`/`.edl.in`) +
  `data/` (gravações/logs, gitignorado) + `README.md`. Quem executa é sempre `./app -folder
  <pasta> -scenario <nome>` (ou `./app -file <arquivo.edl>`) — nunca um binário próprio da poc.
- **O modelo é um plugin, nunca o objeto de desenvolvimento do core.** `models/<categoria>/<nome>/`
  é um projeto Meson independente, com `Makefile`/`tests/`/`docs/`/`README.md`/`CHANGELOG.md`
  próprios; o core só enxerga o `.so` já instalado, nunca o fonte C++ do modelo.
- **`Makefile` orquestra, não implementa.** `make models` descobre todo projeto sob `models/` por
  `find` (qualquer subpasta, qualquer profundidade) — um modelo novo (`make new-model
  NAME=... CATEGORY=players/air`) entra no build sem editar nada na raiz.
- **`plugins/` é o único depósito.** Um `.so` compilado por este repositório e um de terceiro
  entram exatamente pelo mesmo lugar (`plugins/*.so` + `plugins/data/<nome>/`); a partir daí são
  indistinguíveis para `make install`.

## Leia mais

| documento | quando ler |
|---|---|
| [`TOUR.md`](TOUR.md) | chegou agora? passeio guiado por todo o repositório |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | escrever um MODELO novo |
| [`models/REGISTRO.md`](models/REGISTRO.md) | quem já está trabalhando em qual modelo |
| [`libs/README.md`](libs/README.md) | as bibliotecas de suporte core/modelo |
| [`tests/README.md`](tests/README.md) | as suítes de teste, o que cada uma prova |
| [`contexts/`](contexts/) | MIXR e BehaviorTree.CPP por dentro |
| [`docs/manual/`](docs/manual/) | manual interativo (`make open-docs`) |
| [`src/ui/`](src/ui/) | editor gráfico de cenário `.edl` (`make open-edl`) |
| [`src/rl/`](src/rl/) / [`src/poc/rl-training/`](src/poc/rl-training/) | ambiente de RL / pipeline de treino |
| [`app/README.md`](app/README.md) | o painel de controle (TUI) por dentro |
| [`CLAUDE.md`](CLAUDE.md) | diário de arquitetura — histórico de decisões e armadilhas, para consulta pontual, não leitura sequencial |
