# poc-mixr

Metaprojeto de exploração do framework [MIXR](https://mixr.dev) (pacote **Conan** — o gerenciador
de pacotes/dependências para C++ que este projeto usa — `mixr/1.0.5`) e do
[BehaviorTree.CPP v3](https://github.com/BehaviorTree/BehaviorTree.CPP) (`behaviortree.cpp.asa/3.5.6`).
Não é uma aplicação de simulação em si — é um conjunto de subprojetos que testam **como usar** o
MIXR: o que o framework já resolve pronto, o que sobra para escrever, e qual o preço de cada
escolha de integração (onde a decisão roda, como um player chega à simulação, em que linguagem a
lógica de decisão é escrita...).

> **Novo nos dois?** (E o que é uma "**poc**"? *Prova de conceito* — cada pasta sob `src/poc/`
> isola UMA variável de integração, ver "Como o projeto se organiza". É também o sufixo do nome
> deste repositório.) **MIXR** (*Mixed Reality Simulation*, nome herdado do framework original —
> apesar do nome, não é sobre realidade aumentada/virtual) é um framework C++ para modelagem e
> simulação (M&S) de sistemas — plataformas, sensores, armamento, redes de interoperabilidade
> (**DIS**, *Distributed Interactive Simulation*, protocolo de rede padronizado como IEEE 1278,
> ver "Rodar"); não é um simulador pronto, é um conjunto de bibliotecas para montar um (aqui,
> usado para simular aeronaves — a dinâmica de voo em si vem do **JSBSim**, motor de física de
> voo open-source — mas o framework em si não é exclusivo de aviação). Um **player** é qualquer
> entidade simulada dentro de um cenário (uma aeronave, por exemplo), hospedada por uma
> **Station** — o objeto raiz de uma simulação MIXR, dona da lista de players e do laço de
> execução (ver "O modelo MIXR em uma tela" em `CLAUDE.md` para a hierarquia completa).
> **BehaviorTree.CPP** é
> uma biblioteca de árvores de comportamento — a forma padrão, em robótica/jogos, de compor
> decisão em nós reutilizáveis (sequências, *fallbacks*, condições, ações); é uma das formas que
> o **UBF** (*Unified Behavior Framework*, o mecanismo nativo do MIXR para plugar decisão externa
> num player) aceita — este projeto usa quase sempre BehaviorTree.CPP por trás do UBF. A
> estrutura de cada cenário (que *players*/sensores existem, com que parâmetros) é declarada num
> arquivo **`.edl`** — **EDL** (*English Description Language*, nome oficial do MIXR original; a
> sintaxe não é inglês natural, é só o nome herdado do framework), a linguagem de configuração
> nativa do MIXR: texto simples, abre em qualquer editor (exemplo real:
> [`src/poc/dis/flight/configs/scenario.edl.in`](src/poc/dis/flight/configs/scenario.edl.in)). Cada
> campo configurável de uma classe é um **slot**; uma **fábrica** (*factory*) resolve o nome de
> classe escrito no `.edl` para o construtor C++ correspondente.

O MIXR **nunca é modificado** — entra como dependência binária, resolvida pelo Conan. Os
**modelos** (a lógica de decisão de cada aeronave) são carregados pelo executável em tempo de
execução como plugins (`dlopen`: o `.so` do modelo é aberto em *runtime* pelo **host** — o
executável `./app` deste repositório, detalhado na seção "Build" abaixo — nunca linkado em tempo
de compilação; o host nunca vê o código-fonte do modelo); o fork empacotado é
**headless**, e toda visualização é feita via **Tacview Real-Time Telemetry**
([Tacview](https://www.tacview.net/) — visualizador 3D de voo de terceiros; a simulação roda
normalmente sem ele, ele só recebe telemetria ao vivo por *socket*, ver "Pré-requisitos").

> Documentação, comentários de código e mensagens de console são em português do Brasil.
> Identificadores, nomes de slot e nomes de fábrica ficam em inglês — são os originais do MIXR.

## Pré-requisitos

| ferramenta | versão | por quê |
|---|---|---|
| Conan | ≥ 2.0 | **gerenciador de pacotes/dependências para C++**; resolve MIXR (ver acima), BehaviorTree.CPP, e três libs de papel específico — `ftxui` (biblioteca de interface de texto/TUI, usada só no `./app`), `pybind11` (gera os *bindings* Python↔C++, usado só em `src/rl/bindings`), `onnxruntime` (motor de inferência de redes neurais, usado só em `libs/xinfer` para políticas `.onnx`) — mais `gtest` (framework de testes unitários, usado na suíte de testes) |
| Meson | ≥ 1.0 | sistema de build (como o CMake — gera os arquivos que o Ninja de fato executa; host e modelo são dois projetos Meson separados, ver "Build" abaixo) |
| Ninja | qualquer | *backend* do Meson |
| GCC ≥ 7 | — | o projeto compila em C++17 — único compilador de fato exercitado (INSTALL.md, CI e as receitas de `deps/` só instalam/testam GCC; Clang deve funcionar em teoria por ser C++17 padrão, mas nunca foi verificado por nenhum processo automatizado deste repositório) |
| pkg-config | qualquer | resolve as libs via `dependency(method: 'pkg-config')` |
| Python 3 + `python3-dev` | 3.x | `src/rl/bindings` (parte do host) linka `pybind11`/`Python.h` |
| gzip | qualquer | descomprime os tiles SRTM na 1ª execução (SRTM = dados públicos de elevação de terreno, NASA) |
| Qt5 + ZeroMQ (dev) | Qt5 ≥ 5.5, CMake ≥ 3.2 | builda o Groot 1.0 (`deps/groot/`) — editor/monitor visual das árvores de comportamento; só necessário se for usar o Groot (ver "Leia mais") |
| Tacview (opcional) | Standard/Advanced | visualizador 3D de terceiros, [tacview.net](https://www.tacview.net/) — Standard é gratuito, Advanced é pago; sem ele a simulação roda normalmente, só sem visualização 3D ao vivo |
| Node.js + npm | ≥ 18 | `make docs`, `make open-edl-builder` e `make test-ci` (`gitlab-ci-local`). Os dois primeiros baixam React/ReactDOM via `curl` e o Babel via `npm` na primeira execução — precisa de rede liberada para `cdnjs.cloudflare.com`/`registry.npmjs.org`; depois disso cacheiam e rodam offline. **O pacote da distro pode ser velho demais** (medido: o `apt` do Ubuntu 22.04 traz `nodejs 12.22.9`) — instale a LTS pelo NodeSource e atualize o npm em seguida, ver [`INSTALL.md`](INSTALL.md) §5. Não é dependência de *build* (o C++ compila sem ele), mas é pré-requisito do projeto — o ferramental documentado depende dele |
| Docker (opcional) | qualquer | só para `make test-ci` — roda o pipeline de `.gitlab-ci.yml` (que segue esta seção) num `ubuntu:24.04` limpo. Não instala Docker aqui; ver [docker.com](https://www.docker.com/) |


As cinco dependências que não estão no ConanCenter são construídas do código-fonte para o cache local
do Conan. São elas:

- **`mixr`** — o framework em si (ver acima);
- **`behaviortree.cpp.asa`** — o fork da biblioteca de árvores de comportamento;
- **`jsbsim`** — motor de física de voo;
- **`openrti`** — implementação de **RTI** (*Runtime Infrastructure*, o middleware de rede) para
  **HLA** (*High Level Architecture*, padrão IEEE 1516 de federação de simulações — uma
  alternativa ao DIS); o MIXR a declara mas este fork não a compila, já que a
  interoperabilidade usada aqui é DIS, não HLA;
- **Groot 1.0** — editor/monitor visual das árvores de comportamento; exige Qt5 e ZeroMQ
  instalados no sistema antes de rodar o script (ver a tabela de pré-requisitos e
  [`INSTALL.md`](INSTALL.md) §4).

É o mesmo caminho que o CI usa (ver `.gitlab-ci.yml`). A primeira execução é lenta — a receita
builda dependências transitivas inteiras. Depois disso, `make configure` resolve tudo do cache
local.

Passo a passo (pacotes de sistema, Conan, perfil, etc), o "porquê" de cada um, e uma
instalação Ubuntu 24.04 do zero testada em container → [`INSTALL.md`](INSTALL.md).

## Build

Um **modelo** é a lógica de decisão de um *player* (a entidade simulada dentro do MIXR — uma
aeronave, por exemplo) (`domain`/`bt`/`ubf`/`xnative` de `models/<categoria>/<nome>/` — caminhos
exatos na árvore abaixo), compilada à parte e carregada em *runtime* via `dlopen` — nunca
linkada no host. O **host** é o executável `./app` — mais três binários satélite: `edlcheck
<arquivo>` (valida um `.edl` sem levantar simulação), `plugininfo <arquivo.so>` (introspecciona um
plugin sem `Station` nenhuma) e `node <arquivo.edl>` (runner headless, sem TUI, ver "Leia mais") —
compilado em `app/`+`src/`+`libs/`. **Host e modelo são dois projetos Meson separados**, orquestrados pelo
`Makefile` — o host nunca vê o código-fonte de um modelo, só o `.so` já compilado. Etapas, em
ordem:

```bash
make configure   # 1. conan install + meson setup do host                    -> build/
make sdk         # 2. publica o SDK (Software Development Kit) de plugin em dist/: o
                 #    ABI (a interface binaria que um .so de modelo tem que
                 #    respeitar, libs/xplugin/PluginAbi.hpp) + as .so compartilhadas
                 #    host<->modelo (libs/x<nome>, ex.: xboard,
                 #    xlog -- ver "Como o projeto se organiza" abaixo)       -> dist/
make models      # 3. compila o(s) modelo(s) (nao mexe em dist/)             -> plugins/
make build       # 4. compila o host (nao depende dos modelos)               -> build/
make install     # 5. compila o host se preciso (depende de 'build') + sincroniza
                 #    plugins/ -> dist/ e instala o host                    -> dist/
```

`build`/`install` puxam `sdk` sozinhos, mas **não** puxam `models` — as duas são DECOPLADAS de
propósito (compilar o host nunca precisou saber onde os modelos guardam os artefatos deles, ver
[`CLAUDE.md`](CLAUDE.md) — apesar do nome, é referência de arquitetura para humanos também, não
só config de IA; ver a tabela "Leia mais" no fim — seção "Desacoplando `models` de `dist/`"). No
dia a dia, rode as duas: `make configure
&& make models && make install` (ou `make configure && make build && make models && make
install`, se quiser separar explicitamente o passo 4); as etapas do bloco acima existem para
rodar isoladamente (ex.: mexeu só no modelo, `make models` sozinho não toca no host).

> **Armadilha:** `make install` sem um `make models` anterior sincroniza um `plugins/` vazio, em
> silêncio (com um aviso) — nenhum **cenário** (a instância de `.edl` que descreve *players*,
> sensores e rede a rodar — ver "Rodar" abaixo) carrega nada. **Recuperação:** rodar `make models
> && make install` de novo resolve; não precisa de `make clean`.

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
2. Gera uma fixture **hermética** (sem `networks:` — não abre porta DIS nenhuma, ver "Rodar" abaixo)
   da poc (prova de conceito) `flight` (`tests/scenario/make_fixture.py --poc
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

Determinismo (mesmo resultado com 1, 2 e 4 threads de tempo crítico — o laço de simulação a 50 Hz
dividido em 4 fases; a decisão do UBF roda na fase 3) tem script próprio, fora do `make test`:

```bash
./tests/determinism/check_determinism.sh ./build/app/src/app flight 2000 flight
#                                          binario           rotulo frames poc
```

`<rotulo>` é só um nome livre para a pasta de saída (`build/tests-determinism/<rotulo>`);
`frames` é quantos passos de simulação rodar (2000 é um valor razoável para conferir); `<poc>`
(opcional, default vazio = cenário de produção como está) é a poc de `src/poc/dis/` usada para
gerar uma fixture hermética com intruso. O que cada suíte prova e quanto custa →
[`tests/README.md`](tests/README.md).

## CI (GitLab)

[`.gitlab-ci.yml`](.gitlab-ci.yml) tem dois jobs de verdade, `build` e `test`, seguindo a MESMA
sequência desta seção README + [`INSTALL.md`](INSTALL.md), do zero, **num container Docker sem
nada pré-instalado**. Os dois jobs sobem o ambiente inteiro do roteiro de instalação — pacotes de
sistema (§1) → Conan via `pipx` (§2) → perfil default (§3) → Node.js pelo NodeSource (§5) → a
checagem final dos sete comandos (§6) —; o `build` acrescenta os pacotes do Groot e
`./scripts/deps.sh` (§4) e então `configure` → `sdk` → `models` → `build` → `install`, e o `test`
roda `meson configure -Dtests=true` + `make test-models` + `make test`. O Node não é usado por
alvo nenhum do pipeline: ele está lá porque `INSTALL.md` §5 o declara pré-requisito do projeto, e
sem isso aquele passo (repositório de terceiro, linha LTS, `npm@latest`) não teria cobertura
automatizada nenhuma. As dependências privadas (`mixr/1.0.5`,
`behaviortree.cpp.asa/3.5.6`) vêm por padrão de um remote Conan privado, mas **este pipeline nunca
usa esse remote** — de propósito, sem exceção nem variável de CI/CD para religar isso: o job
`build` sempre compila `mixr`/`behaviortree.cpp.asa`/`jsbsim`/`openrti`/**Groot** do fonte
(`./scripts/deps.sh`, documentado em [`INSTALL.md`](INSTALL.md) §4 — o Groot em particular nunca
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
./build/app/src/app -f <arquivo.edl>                   # cenario apontado direto (assume a frota falcon1..4)
./build/app/src/app -folder <pasta>                     # navega uma pasta de cenarios, tela de selecao (TUI
                                                         # no proprio terminal -- funciona por SSH, sem X11/navegador)
make run-app                                            # atalho para '-folder ./sandbox'
```

`-f` sempre assume a frota `falcon1..4`; um `.edl` sem esses nomes falha ao carregar (erro claro,
não silencioso). Para frota arbitrária, use `-folder`, que descobre os *players* em runtime.

| poc | o que demonstra | comando | porta Tacview | porta DIS (local) |
|---|---|---|---|---|
| `flight` | cadeia completa de decisão (evade → alerta → apoio), trocando DIS de verdade com `bandit` | `-folder src/poc/dis -scenario flight` | 1234 | 3002 |
| `bandit` | o intruso: pilotado por joystick ou por piloto automático de reserva (script fixo, não é o nó "Fallback" de árvore de comportamento citado acima), emitindo estado via DIS | `-folder src/poc/dis -scenario bandit` | 1235 | 3001 |
| `python-flight` | as folhas de ação da árvore de decisão escritas em Python, editáveis sem recompilar | `-folder src/poc -scenario python-flight` | 1237 | 3004 |
| `onnx-policy` | a decisão inteira (não só folhas) feita por uma rede neural treinada, sem árvore de comportamento | `-folder src/poc -scenario onnx-policy` | 1238 | 3005 |
| `built-in_mixr_1` | o player mais completo montável só com componentes nativos do MIXR (53 classes num único avião) | `-folder src/poc -scenario built-in_mixr_1` | 1239 | — (**hermético**: não abre rede nenhuma, sem DIS) |
| `full-systems-nav` | o mesmo player máximo acima, agora navegando de verdade (`Route`/`Steerpoint`) com uma árvore de comportamento de um nó só | `-folder src/poc -scenario full-systems-nav` | 1240 | — (**hermético**) |

`porta Tacview` é a mesma ferramenta de visualização 3D explicada em "Pré-requisitos" acima —
conecte o Tacview nesse número de porta para ver aquele cenário específico ao vivo. As pocs
**não-herméticas** (`flight`/`bandit`/`python-flight`/`onnx-policy` — qualquer uma com um número
na coluna "porta DIS") escutam **DIS** (*Distributed Interactive Simulation*, protocolo de rede
para troca de estado de entidades simuladas entre processos, padronizado como IEEE 1278) em
`3000`; a porta acima é só a de emissão local — `flight`/`bandit` trocam DIS entre si, rode as
duas juntas para ver a cadeia completa. `built-in_mixr_1`/`full-systems-nav` não abrem porta DIS
nenhuma (daí "hermético"). Esta é a lista completa das pocs sob `src/poc/`; para as de `sandbox/` e para
armadilhas já confirmadas de cada uma, abra o `README.md` do subprojeto.

## Como o projeto se organiza

(termos como `.edl`/`dlopen`/MIXR/player citados na árvore abaixo estão definidos no parágrafo
de abertura deste README, no topo do arquivo; `CLAUDE.md`, citado abaixo, está explicado na
tabela "Leia mais" — apesar do nome, não é config de ferramenta de IA, é referência de
arquitetura para humanos também)

```
poc-mixr/
├── app/          painel de controle (TUI) -- o runner interativo principal do repositorio
├── src/
│   ├── poc/      as provas de conceito -- cada pasta isola UMA variavel de integracao
│   ├── rl/       wrapper Gymnasium (biblioteca padrao de ambientes de RL -- Reinforcement
│                 Learning, aprendizado por reforco) pra treinar uma politica de RL contra
│                 a mesma simulacao
│   ├── ui/       editor grafico de cenario .edl (ferramenta de autoria, offline)
│   └── node/     runner HEADLESS de um cenario (sem TUI, so log) -- peer enxuto de ./app,
│                 ver CLAUDE.md secao 'src/node' e src/node/README.md
├── models/       o(s) MODELO(s) -- projetos Meson (sistema de build, ver Pre-requisitos acima)
│                 a parte, carregados como plugin (dlopen);
│                 cada um em camadas models/players/<nome>/{src,include}/{domain,bt,ubf,xnative}/
│                 (.cpp em src/, .hpp em include/ -- convencao C++ comum) -- "o que fazer"
│                 mora em domain/, "como conectar ao MIXR" mora em bt/ (nos da arvore de
│                 comportamento BehaviorTree.CPP) + ubf/ (a classe MIXR que integra a decisao
│                 ao framework nativo, ver UBF acima) + xnative/ (utilitarios especificos
│                 do MIXR, ex.: tag de thread)
│                 (<categoria> em models/<categoria>/<nome>/ e' sempre "players" hoje --
│                 "systems"/"others" existem como convencao para o futuro, ainda vazias)
├── plugins/      deposito flat dos .so compilados (proprios OU de terceiro) -> dist/ via 'make install'
├── libs/         bibliotecas x<nome> (ex.: xboard, xlog -- as que cruzam a fronteira dlopen
│                 host<->modelo; outras, como xtacview, ficam estaticas) -- cada uma com
│                 README.md, ver libs/README.md
├── shared/       dados vendorizados do CENARIO -- terreno SRTM (elevacao publica, NASA) e
│                 aeronaves JSBSim (motor de dinamica de voo open-source), em shared/data/
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

1. **"O que fazer" mora em `domain/`** (`models/<categoria>/<nome>/{src,include}/domain/` —
   `.cpp` em `src/`, `.hpp` em `include/`) **; "como conectar" mora nas factories/adaptadores**
   (`bt/`/`ubf/`/`xnative/`, mesma divisão `src/`+`include/`, ver a árvore acima). `domain/` não
   inclui um header do MIXR — dá para testar a política sem levantar uma simulação.
2. **Um arquivo, uma questão.** Nenhum `main.cpp` de centenas de linhas.
3. **Estrutura vem do EDL, comportamento vem do C++.** Reconfigurar o cenário não recompila nada.

## Leia mais

| documento | quando ler |
|---|---|
| [`TOUR.md`](TOUR.md) | chegou agora? comece por aqui — passeio guiado, em ordem, por todo o repositório |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | escrever um MODELO novo (não mexer no host) |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) / [`models/REGISTRO.md`](models/REGISTRO.md) | como um modelo vira plugin; quem já está trabalhando em qual |
| [`libs/README.md`](libs/README.md) | as 12 bibliotecas de suporte host/modelo (6 compartilhadas via `dlopen`, as demais estáticas), uma por pasta |
| [`tests/README.md`](tests/README.md) | as suítes de teste, o que cada uma prova |
| [`contexts/`](contexts/) | MIXR e BehaviorTree.CPP por dentro (destilado + fonte vendorizado) |
| [`docs/manual/`](docs/manual/) | visualizador do ciclo de execução MIXR e catálogo de classes (`make open-docs` — requer navegador na mesma máquina) |
| [`src/ui/`](src/ui/) | editor gráfico de cenário `.edl` (`make open-edl-builder` — requer navegador na mesma máquina) |
| Groot (`deps/groot/`, ver `INSTALL.md` §4) | editor/monitor ao vivo de árvores de comportamento (`make open-groot` — requer display X11/Wayland na mesma máquina); diferente do `src/ui`, que edita o `.edl` inteiro, não só a árvore |
| [`src/rl/`](src/rl/) | treinar uma política de RL contra a mesma simulação |
| [`app/README.md`](app/README.md) | o painel de controle (TUI) por dentro |
| [`CLAUDE.md`](CLAUDE.md) | **não é documentação de onboarding** — é o diário de arquitetura mantido para dar contexto a sessões de programação agêntica (Claude Code): histórico de decisões e armadilhas, em ordem cronológica de escrita, não pedagógica. Consulte por seção quando precisar confirmar um detalhe específico que os documentos acima não cobrem; não é para leitura sequencial |
