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

```
poc-mixr/
├── app/          painel de controle (TUI) -- runner interativo principal
├── src/
│   ├── poc/      as provas de conceito -- cada pasta isola UMA variavel de integracao
│   ├── rl/       wrapper Gymnasium -- o AMBIENTE de RL contra a mesma simulacao
│   ├── ui/       editor grafico de cenario .edl (autoria offline)
│   └── node/     runner headless de um cenario (sem TUI, so log)
├── models/       os MODELOS -- projetos Meson a parte, carregados como plugin (dlopen)
├── plugins/      deposito flat dos .so compilados (proprios ou de terceiro) -> dist/
├── libs/         bibliotecas de suporte core/modelo, uma por pasta
├── shared/       dados vendorizados do cenario (terreno SRTM, aeronaves JSBSim)
├── sandbox/      cenarios soltos de experimentacao
├── tests/        suite do core (a de cada modelo vive em models/<categoria>/<nome>/)
├── contexts/     material de consulta sobre MIXR e BehaviorTree.CPP
├── docs/         manual interativo gerado do fonte, slides, estudos de viabilidade
├── deps/         receitas Conan p/ compilar dependencias a partir do fonte
├── scripts/      scaffold de modelo novo, build de deps/ do fonte
├── tools/        extracao de cadeia de execucao/diagrama de classes p/ docs/
├── build/        gerado por 'make configure'/'make build' -- gitignored
├── dist/         gerado por 'make install' -- gitignored, e' o que de fato roda
└── Makefile      orquestra Conan + Meson
```

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
