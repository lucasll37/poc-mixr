# poc-mixr

Metaprojeto de exploração do framework [MIXR](https://mixr.dev) (pacote Conan `mixr/1.0.5`) e do
[BehaviorTree.CPP v3](https://github.com/BehaviorTree/BehaviorTree.CPP) (`behaviortree.cpp.asa/3.5.6`).
Não é uma aplicação de simulação em si — é um conjunto de subprojetos que testam **como usar** o
MIXR: o que o framework já resolve pronto, o que sobra para escrever, e qual o preço de cada
escolha de integração (onde a decisão roda, como um player chega à simulação, em que linguagem a
lógica de decisão é escrita...).

O MIXR **nunca é modificado** — entra como dependência binária, resolvida pelo Conan. Os
**modelos** (a lógica de decisão de cada aeronave) são carregados pelo executável em tempo de
execução como plugins (`dlopen`); o fork empacotado é **headless**, e toda visualização é feita
via **Tacview Real-Time Telemetry**.

> Documentação, comentários de código e mensagens de console são em português do Brasil.
> Identificadores, nomes de slot e nomes de fábrica ficam em inglês — são os originais do MIXR.

## Pré-requisitos

| ferramenta | versão | por quê |
|---|---|---|
| Conan | ≥ 2.0 | resolve MIXR, BehaviorTree.CPP, ftxui, pybind11, onnxruntime, gtest |
| Meson | ≥ 1.0 | sistema de build |
| Ninja | qualquer | *backend* do Meson |
| GCC ≥ 7 | — | o projeto compila em C++17 — único compilador de fato exercitado (INSTALL.md, CI e as receitas de `deps/` só instalam/testam GCC; Clang deve funcionar em teoria por ser C++17 padrão, mas nunca foi verificado por nenhum processo automatizado deste repositório) |
| pkg-config | qualquer | resolve as libs via `dependency(method: 'pkg-config')` |
| Python 3 + `python3-dev` | 3.x | `src/rl/bindings` (parte do host) linka `pybind11`/`Python.h` |
| gzip | qualquer | descomprime os tiles SRTM na 1ª execução |
| Qt5 + ZeroMQ (dev) | Qt5 ≥ 5.5, CMake ≥ 3.2 | builda o Groot 1.0 (`deps/groot/`) — editor/monitor visual das árvores de comportamento |
| Tacview (opcional) | Standard/Advanced | recebe a telemetria ao vivo |
| Docker (opcional) | qualquer | só para `make test-ci` — roda o pipeline de `.gitlab-ci.yml` (que segue esta seção) num `ubuntu:24.04` limpo. Não instala Docker aqui; ver [docker.com](https://www.docker.com/) |

Passo a passo (pacotes de sistema, Conan, perfil, remote privado), o "porquê" de cada um, e uma
instalação Ubuntu 24.04 do zero testada em container → [`INSTALL.md`](INSTALL.md).

`mixr`/`behaviortree.cpp.asa`/`jsbsim`/`openrti` vêm prontos de um remote Conan privado por
padrão — **sem acesso a ele, `make configure` falha com "package not found"**; a saída, para
qualquer uma das quatro (não só o Groot), é compilar do fonte via `./scripts/deps.sh` (o mesmo
caminho que o próprio CI usa, de propósito, para nunca depender desse remote — ver
`.gitlab-ci.yml`). O Groot **não tem pacote pronto em remoto nenhum** — pra ele, `deps.sh` não é
alternativa, é a única forma de tê-lo:

```bash
./scripts/deps.sh
```

Pré-requisitos de sistema do Groot (Qt5/ZeroMQ) e a alternativa de buildar só ele →
[`INSTALL.md`](INSTALL.md) §7. Sem acesso ao remote privado, comece por aqui em vez de por
`make configure` — economiza descobrir a falha do jeito difícil.

## Build

O host e o(s) modelo(s) são projetos Meson **separados**, orquestrados pelo `Makefile` — o host
nunca vê o código-fonte de um modelo, só o `.so` já compilado. Etapas, em ordem:

```bash
make configure   # 1. conan install + meson setup do host           -> build/
make sdk         # 2. publica o contrato de plugin + libs de fronteira -> dist/
make models      # 3. compila o(s) modelo(s) (nao mexe em dist/)    -> plugins/
make build       # 4. compila o host (nao depende dos modelos)      -> build/
make install     # 5. sincroniza plugins/ -> dist/ e instala o host -> dist/
```

`build`/`install` puxam `sdk` sozinhos, mas **não** puxam `models` — as duas são DECOPLADAS de
propósito (compilar o host nunca precisou saber onde os modelos guardam os artefatos deles, ver
CLAUDE.md seção "Desacoplando `models` de `dist/`"). Isso significa que `make install` sem um
`make models` anterior sincroniza um `plugins/` vazio, em silêncio (com um aviso) — nenhum
cenário carrega nada. No dia a dia, rode as duas: `make configure && make models && make install`
(ou `make configure && make build && make models && make install`, se quiser separar
explicitamente o passo 4); as etapas do bloco acima existem para rodar isoladamente (ex.: mexeu
só no modelo, `make models` sozinho não toca no host).

```bash
make clean   # remove build/ e dist/ (host e modelos) e o deposito que 'models' gerou
make help    # lista TODOS os alvos do Makefile, com descricao
```

## Testes

```bash
meson configure build -Dtests=true   # roda uma vez, depois do 'make configure' -- a suite fica atras desta opcao
make test-models                     # so a suite do(s) MODELO(s): domain + tree + native
make test                            # so a suite do HOST: scenario/determinism/plugin/memory/guard/...
make test-asan                       # AddressSanitizer/LeakSanitizer (build separado, lento)
```

`make test` builda e sincroniza o(s) modelo(s) antes de rodar (via `install` — o binário do host
precisa do `.so` em `dist/` para os testes que `dlopen()`, ex. `scenario`/`plugin`), mas roda só a
suíte do host; a suíte do próprio modelo (`domain`/`tree`/`native`, sem `Station`) é `make
test-models`, que delega para o `Makefile` autocontido de cada projeto de modelo. As duas juntas
são o que o CI roda (`.gitlab-ci.yml`, job `test`).

### `make test-asan`

Fora de `make test` de propósito — reconfigura e recompila host **e** modelo duas vezes (uma vez
com o sanitizador, outra revertendo), então é lento. Passo a passo:

1. **Recompila os DOIS lados com `-fsanitize=address`**: o modelo (`make models ASAN=true`, que
   hoje instrumenta só `models/players/A-4` — é o único projeto de modelo com uma opção `asan` no
   próprio `meson_options.txt`; `template` não tem essa opção e a ignora) e o
   host (`meson configure build -Dasan=true` + `meson compile`, que instrumenta `./app` e
   `src/rl/bindings`). Os dois são necessários — instrumentar só o host deixaria o `.so` do plugin
   sem *redzone* de pilha e sem símbolos no relatório do LeakSanitizer.
2. Gera uma fixture hermética da poc `flight` (`tests/scenario/make_fixture.py --poc
   flight --mode intruder` — carrega `libflight.so`, o plugin do A-4) e roda 500 frames
   determinísticos com `-threads 1`, sob `LSAN_OPTIONS=suppressions=./tests/memory/asan.supp`.
   `-threads 1` é a mesma cautela já usada pela suíte `memory` (ver
   [`tests/README.md`](tests/README.md)): os contadores de instância do MIXR não são atômicos, e
   mais de uma thread do pool de tempo crítico introduziria ruído não relacionado a vazamento de
   verdade.
3. **Reverte `build/`/`plugins/`/`dist/` para não-ASan automaticamente no final**, tenha o passo 2
   acusado vazamento ou não — para não deixar o repositório instrumentado depois de uma corrida.
   Se a reversão em si falhar (raro), o alvo avisa em `stderr` e pede para rodar `make configure &&
   make build` manualmente antes de confiar no próximo `make test`/`make run-*`.

As supressões de [`tests/memory/asan.supp`](tests/memory/asan.supp) existem porque, sem elas, o
alvo nasce **permanentemente vermelho**: acusa ~896 bytes em 22 alocações que são do próprio
framework MIXR (`JSBSimModel::setSlotRootDir`/`setSlotModel`, `PrintHandler::setFullFilename`,
`DataRecorder::setSlotEventName`), de tamanho fixo e confirmadas — por outro caminho, os
contadores de instância do MIXR — como não crescendo com os frames; não é código deste
repositório. Um vazamento novo, fora dessa lista, ainda derruba o alvo.

**Complementa, não substitui**, os contadores de instância do próprio MIXR (suíte `memory`, já
dentro de `make test`): eles pegam vazamento de *ref-counting* (objeto vivo que ninguém mais
alcança); o LeakSanitizer pega `new`/`malloc` cru sem `delete`/`free` correspondente, que os
contadores não enxergam.

Determinismo (mesmo resultado com 1, 2 e 4 threads de tempo crítico) tem script próprio, fora do
`make test`:

```bash
./tests/determinism/check_determinism.sh ./build/app/src/app <cenario> 2000 <rotulo>
```

O que cada suíte prova e quanto custa → [`tests/README.md`](tests/README.md).

## CI (GitLab)

[`.gitlab-ci.yml`](.gitlab-ci.yml) tem dois jobs de verdade, `build` e `test`, seguindo a MESMA
sequência desta seção README + [`INSTALL.md`](INSTALL.md), do zero, **num container Docker sem
nada pré-instalado** — pacotes de sistema → Conan (via `pipx`) → `configure` → `sdk` → `models` →
`build` → `install` (`build`, que também builda o Groot — ver abaixo) e `meson configure
-Dtests=true` + `make test` (`test`). As dependências privadas (`mixr/1.0.5`,
`behaviortree.cpp.asa/3.5.6`) vêm por padrão de um remote Conan privado, mas **este pipeline nunca
usa esse remote** — de propósito, sem exceção nem variável de CI/CD para religar isso: o job
`build` sempre compila `mixr`/`behaviortree.cpp.asa`/`jsbsim`/`openrti`/**Groot** do fonte
(`./scripts/deps.sh`, documentado em [`INSTALL.md`](INSTALL.md) §7 — o Groot em particular nunca
teve pacote pronto em remoto nenhum). Mais lento na primeira execução (horas — o `cache:` do
arquivo evita repetir o custo enquanto as receitas de `deps/` não mudarem), mas sem depender de
credencial nenhuma.

**`make test-ci`** roda esse pipeline inteiro, do zero, num container Docker — sucessor de um
antigo `make check-docs-ubuntu24` (removido): em vez de uma cópia paralela dos comandos do README
só até `make build`/`make install`, roda o `.gitlab-ci.yml` de verdade, do mesmo jeito que o
runner do GitLab roda, cobrindo também `make test`:

```bash
make test-ci
```

Por baixo é [`gitlab-ci-local`](https://github.com/firecow/gitlab-ci-local): lê
`stages:`/`needs:`/`image:` direto do `.gitlab-ci.yml` e executa cada job em Docker, isolado da
árvore de trabalho (copia só o que o `git` rastreia e não ignora — `build/`/`dist/`/
`contexts/src/` ficam de fora, então não há como o teste "trapacear" reaproveitando cache local).
Exige Docker + Node — chamar `npx gitlab-ci-local` direto (sem o `make test-ci`) dá mais controle:

```bash
npx gitlab-ci-local build       # so o job 'build'
npx gitlab-ci-local --list-all  # mostra TODOS os jobs definidos, mesmo os fora de rules/when
```

## Rodar

Um executável só, `app` — cada prova de conceito é um **cenário**, não um binário próprio.
Rodar sempre a partir da raiz do repositório (caminhos de `configs:`/`data:` são relativos):

```bash
./build/app/src/app -folder <pasta> -scenario <nome>   # uma poc dentro de <pasta>, sem passar pela tela
./build/app/src/app -f <arquivo.edl>                   # cenario apontado direto (assume falcon1..4)
./build/app/src/app -folder <pasta>                     # navega uma pasta de cenarios, tela de selecao
make run-app                                            # atalho para '-folder ./sandbox'
```

Quais cenários existem hoje, o que cada um demonstra e em qual porta o Tacview conecta →
[`CLAUDE.md`](CLAUDE.md) ou o `README.md` de cada subprojeto sob `src/poc/`.

## Como o projeto se organiza

```
poc-mixr/
├── app/          painel de controle (TUI) -- o runner interativo principal do repositorio
├── src/
│   ├── poc/      as provas de conceito -- cada pasta isola UMA variavel de integracao
│   ├── rl/       wrapper Gymnasium para treinar RL contra a mesma simulacao
│   ├── ui/       editor grafico de cenario .edl (ferramenta de autoria, offline)
│   └── node/     runner HEADLESS de um cenario (sem TUI, so log) -- peer enxuto de ./app,
│                 ver CLAUDE.md secao 'src/node' e src/node/README.md
├── models/       o(s) MODELO(s) -- projetos Meson a parte, carregados como plugin (dlopen)
├── plugins/      deposito flat dos .so compilados (proprios OU de terceiro) -> dist/ via 'make install'
├── libs/         bibliotecas x<nome> reaproveitadas entre host e modelos -- cada uma com README.md
├── shared/       dados vendorizados do CENARIO -- terreno SRTM, aeronaves JSBSim (shared/data/)
├── sandbox/      cenarios soltos de experimentacao (ver 'make run-app')
├── tests/        suite do host (a de cada modelo vive dentro do proprio models/players/<nome>/)
├── contexts/     material de consulta sobre MIXR e BehaviorTree.CPP -- destilado + fonte vendorizado
├── docs/         documentacao visual gerada -- manual interativo, slides, livros de referencia
├── deps/         receitas Conan p/ compilar mixr/behaviortree/jsbsim/openrti a partir do fonte
├── scripts/      scripts auxiliares -- new-model (models.sh) e build de deps/ do fonte (deps.sh)
├── tools/        ferramentas standalone -- lint de .edl, extracao da cadeia de execucao p/ docs/
├── build/        gerado por 'make configure'/'make build' -- gitignored, nao existe num clone limpo
├── dist/         gerado por 'make install' -- gitignored, e' o que de fato roda
├── conanfile.py  dependencias binarias
├── meson.build   raiz: resolve as libs por pkg-config, da subdir() em cada subprojeto
└── Makefile      orquestra Conan + Meson
```

Três regras valem para todo subprojeto e todo modelo:

1. **"O que fazer" mora em `domain/`; "como conectar" mora nas factories/adaptadores.** `domain/`
   não inclui um header do MIXR — dá para testar a política sem levantar uma simulação.
2. **Um arquivo, uma questão.** Nenhum `main.cpp` de centenas de linhas.
3. **Estrutura vem do EDL, comportamento vem do C++.** Reconfigurar o cenário não recompila nada.

## Leia mais

| documento | quando ler |
|---|---|
| [`CLAUDE.md`](CLAUDE.md) | referência completa de arquitetura — todo subprojeto, biblioteca compartilhada e armadilha já confirmada rodando |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | escrever um MODELO novo (não mexer no host) |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) / [`models/REGISTRO.md`](models/REGISTRO.md) | como um modelo vira plugin; quem já está trabalhando em qual |
| [`libs/README.md`](libs/README.md) | as 12 bibliotecas compartilhadas host↔modelo, uma por pasta |
| [`tests/README.md`](tests/README.md) | as suítes de teste, o que cada uma prova |
| [`contexts/`](contexts/) | MIXR e BehaviorTree.CPP por dentro (destilado + fonte vendorizado) |
| [`docs/manual/`](docs/manual/) | visualizador do ciclo de execução MIXR e catálogo de classes (`make open-docs`) |
| [`src/ui/`](src/ui/) | editor gráfico de cenário `.edl` (`make open-edl-builder`) |
| [`src/rl/`](src/rl/) | treinar uma política de RL contra a mesma simulação |
| [`app/README.md`](app/README.md) | o painel de controle (TUI) por dentro |
