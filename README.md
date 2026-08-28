# poc-mixr

Prova de conceito para desenvolver **novos modelos de simulação** sobre o framework
[MIXR](https://mixr.dev) (fork empacotado como pacote Conan `mixr/1.0.5`) e sobre o
[BehaviorTree.CPP v3](https://github.com/BehaviorTree/BehaviorTree.CPP)
(`behaviortree.cpp.asa/3.5.6`).

O MIXR **não** é o objeto de desenvolvimento: entra como dependência binária, resolvida pelo
Conan, e nunca é modificado. O que se desenvolve aqui são os **modelos** — players, dinâmicas,
sensores, sistemas, e a camada de decisão — e, principalmente, **a forma de encaixá-los na
estrutura que o framework já dirige**. A pergunta que o repositório responde não é "como fazer
um simulador", é *"o que o MIXR já entrega pronto, o que sobra para escrever, e qual é o preço
de cada escolha"*.

O fork empacotado é **headless**: não publica `mixr_graphics`, `glut`, `instruments` nem
`ighost`. Não existe `GlutDisplay` aqui, e toda a visualização é feita por **Tacview Real-Time
Telemetry**, exportada pela biblioteca compartilhada [shared/xtacview/](shared/xtacview/).

> **Documentação, comentários de código e mensagens de console são em português do Brasil.**
> Identificadores, nomes de slot e nomes de fábrica ficam em inglês — são os originais do MIXR.

---

## Índice

1. [Funcionalidades exploradas](#1-funcionalidades-exploradas)
2. [Pré-requisitos e instalação](#2-pré-requisitos-e-instalação)
3. [Build](#3-build)
4. [Rodar](#4-rodar)
5. [Verificar](#5-verificar)
6. [Como o projeto se organiza](#6-como-o-projeto-se-organiza)
7. [Os subprojetos](#7-os-subprojetos)
8. [Bibliotecas compartilhadas](#8-bibliotecas-compartilhadas)
9. [`contexts/` — onde consultar o framework](#9-contexts--onde-consultar-o-framework)
10. [Onde ler mais](#10-onde-ler-mais)

---

## 1. Funcionalidades exploradas

São oito temas, e cada um foi levado até rodar e ser medido. A coluna "onde" aponta o arquivo
que responde pela funcionalidade; a coluna "detalhe" aponta a seção do README do subprojeto que
disseca o assunto.

| # | funcionalidade | onde vive | detalhe |
|---|---|---|---|
| 1 | **Elevação de terreno** — banco SRTM real consultado pela decisão, virando piso anti-CFIT da manobra de evasão e piso AGL do comportamento de segurança | `configs/scenario.epp.in` (slot `terrain:` do `WorldModel`), [`domain/TerrainFloor`](src/single-thread/include/domain/TerrainFloor.hpp), [`app/TerrainData`](src/single-thread/include/app/TerrainData.hpp) | [§10 do single-thread](src/single-thread/README.md#10-elevação-de-terreno) |
| 2 | **Recorder → Tacview** — visualização por *Real-Time Telemetry*, escrita como um `OutputHandler` de verdade na cadeia nativa do gravador, e não como um stream ACMI paralelo | [`shared/xtacview/`](shared/xtacview/) | [§11 do single-thread](src/single-thread/README.md#11-tacview) |
| 3 | **Dinâmica 6-DOF** — aerodinâmica completa via JSBSim, pelo adaptador nativo `JSBSimModel`, comandada pelo `Autopilot` nativo | `configs/scenario.epp.in` (`dynamicsModel: ( JSBSimModel )`), `data/jsbsim/` | [§6.2 do single-thread](src/single-thread/README.md#62--jsbsimmodel---a-dinâmica-6-dof) |
| 4 | **Interação entre agentes por eventos** — quem detecta o intruso avisa os outros; o transporte é o `Datalink` nativo e o `Component::event()` do framework, sem nenhum ponteiro direto entre players | [`xnative/AlertDatalink`](src/single-thread/include/xnative/AlertDatalink.hpp), [`xnative/TacticalAlert`](src/single-thread/include/xnative/TacticalAlert.hpp) | [§9 do single-thread](src/single-thread/README.md#9-interação-entre-players) |
| 5 | **Paralelismo com determinismo garantido** — os players rodam no pool de threads de tempo crítico do framework, e o estado final é idêntico com 1, 2 e 4 threads | `numTcThreads` no `.epp`, [`app/DeterministicRun`](src/single-thread/include/app/DeterministicRun.hpp), [`app/DeterministicDump`](src/single-thread/include/app/DeterministicDump.hpp) | [§12 do single-thread](src/single-thread/README.md#12-determinismo) · [§7 do multi-thread](src/multi-thread/README.md#7-determinismo--o-ponto-da-poc) |
| 6 | **Comportamento com UBF nativo** — `AbstractState`/`AbstractBehavior`/`AbstractAction` do framework, arbitrados por voto num `UbfArbiter` nativo | [`ubf/`](src/single-thread/include/ubf/) | [§8 do single-thread](src/single-thread/README.md#8-a-cadeia-de-decisão-ubf--behaviortree) |
| 7 | **Árvores de comportamento (BehaviorTree.CPP)** — a política interna de um dos comportamentos do UBF é uma árvore v3 carregada de XML | [`bt/`](src/single-thread/include/bt/), `configs/flight_tree.xml` | [§8 do single-thread](src/single-thread/README.md#8-a-cadeia-de-decisão-ubf--behaviortree) |
| 8 | **Padrões de projeto dos exemplos oficiais** — o *builder* canônico, a factory encadeada por nome, o par estrutura-EDL/comportamento-C++, o gancho de sensor em `transmit()`, o `shared/x<nome>` | [`mixr_factory.cpp`](src/single-thread/src/mixr_factory.cpp), [`app/StationBuilder`](src/single-thread/include/app/StationBuilder.hpp), `shared/x*` | [§3 e §5 do single-thread](src/single-thread/README.md#3-como-o-framework-chama-o-nosso-código) |

E uma nona, que não é funcionalidade e sim consequência: **onde a decisão roda é uma escolha de
integração, não do modelo.** É o que os dois subprojetos existem para demonstrar — são o mesmo
modelo, byte a byte, trocando só o agente do UBF. Ver [§7](#7-os-subprojetos).

---

## 2. Pré-requisitos e instalação

| ferramenta | versão | por quê |
|---|---|---|
| **Conan** | ≥ 2.0 | resolve `mixr/1.0.5` e `behaviortree.cpp.asa/3.5.6` e gera os `.pc` e o *native file* do Meson |
| **Meson** | ≥ 1.0 | sistema de build |
| **Ninja** | qualquer | *backend* do Meson |
| **GCC ≥ 7** ou **Clang ≥ 5** | — | o projeto compila em **C++17** |
| **gzip** | qualquer | o `app/TerrainData` descomprime o tile SRTM na primeira execução |
| **Tacview** (Advanced ou Standard) | opcional | recebe a telemetria ao vivo na porta 1234 |

Os pacotes `mixr/1.0.5` e `behaviortree.cpp.asa/3.5.6` precisam estar disponíveis no *remote*
Conan configurado. **JSBSim, protobuf, OpenRTI, zlib e expat vêm como dependências transitivas
do MIXR** — não é preciso pedi-los. O [conanfile.py](conanfile.py) declara apenas os dois:

```python
def requirements(self):
    self.requires("mixr/1.0.5", transitive_headers=True)
    self.requires("behaviortree.cpp.asa/3.5.6")
```

O pacote `mixr` entrega **nove bibliotecas**: `base`, `simulation`, `models`, `terrain`,
`linkage`, `recorder`, `linearsystem`, `interop_common` e `interop_dis`. O `meson.build` da raiz
resolve tudo por **um** `dependency('mixr', method: 'pkg-config')` — o `mixr.pc` lista as nove
em `Requires:`, então não há uma linha de meson por biblioteca.

---

## 3. Build

Toolchain: **Conan 2.x** → **Meson/Ninja** → **Makefile** (orquestra).

```bash
make configure   # conan install (Debug) + meson setup --reconfigure
make build       # compila TODOS os subprojetos, em paralelo
make install     # instala em dist/
make clean       # remove build/ e dist/
make help        # lista os alvos (comentários ## do Makefile)
```

Cada subprojeto vira um executável independente em `build/src/<nome>/src/<nome>` — hoje
`build/src/single-thread/src/single-thread` e `build/src/multi-thread/src/multi-thread`.

**AddressSanitizer:** `meson configure build -Dasan=true && make build`. É a única opção do
[meson_options.txt](meson_options.txt), e liga ASan apenas na `single-thread` (o único alvo que
consome `asan_cpp_args`/`asan_link_args`). Voltar: `-Dasan=false` e recompilar.

### Gotcha: rpath das dependências Conan

As `.so` do MIXR, do BehaviorTree.CPP e do JSBSim vivem no cache do Conan, fora de qualquer
caminho do *loader*. Duas consequências que já custaram tempo:

- **O Meson descarta o `build_rpath` no `meson install`** (por design). Um alvo sem
  `install_rpath` roda de `build/` e falha em `dist/bin/<nome>` com *error while loading shared
  libraries*. Por isso todo `executable()` declara **os dois**.
- **`-Wl,--disable-new-dtags` não é decorativo.** Força `DT_RPATH` em vez de `DT_RUNPATH`:
  RPATH é herdado pelas dependências transitivas, RUNPATH não. Sem ele o *loader* resolve as
  libs nomeadas no executável mas falha **entre** elas (`libmixr_models.so` →
  `libmixr_base.so`).

Conferência após `make install`: `ldd dist/bin/<nome> | grep 'not found'` (silêncio = ok) e
`readelf -d dist/bin/<nome> | grep -E 'RPATH|RUNPATH'`.

`dist/` **não é auto-contido**: rpath com o hash do cache Conan, e `configs/`/`data/` lidos por
caminho relativo.

---

## 4. Rodar

**Sempre a partir da raiz do repositório** — todos os binários resolvem `configs/` e `data/` por
caminho relativo (`./src/<nome>/...`, `./shared/data/...`).

```bash
make run-single-thread     # decisão no ( SimAgent ) nativo — Tacview Real-Time Telemetry na porta 1234
make run-multi-thread      # o mesmo modelo com o ( FlightAgentTC ) próprio: decisão na fase 3
```

Ou direto pelo binário, também a partir da raiz:

```bash
./build/src/single-thread/src/single-thread
./build/src/multi-thread/src/multi-thread
```

**Opções de linha de comando** (as três valem nos dois binários):

| opção | efeito |
|---|---|
| `-f <arquivo>` | usa outro modelo de cenário (o `.epp.in`, não o `.epp` gerado) |
| `-threads <N>` | força `numTcThreads` do pool nativo de tempo crítico |
| `-deterministic <N>` | roda N frames de **passo fixo** e despeja o estado, em vez de rodar em tempo real |

**Teclado** (só em terminal interativo — ver [`shared/xclock/`](shared/xclock/)):
`+`/`=` acelera o tempo, `-`/`_` freia, `espaço`/`p` pausa, `1` volta ao tempo real, `h` ajuda,
`Ctrl+C` encerra. A linha de status mostra tempo de parede e tempo simulado lado a lado
(`[t=24s sim=8.2s PAUSADO (1x)]`) — é a diferença entre os dois que prova o efeito.

**Tacview:** abra *File > Real-Time Telemetry* e conecte na porta **1234**. Com o binário no
WSL2 e o Tacview no Windows, se `127.0.0.1` não conectar, use o IP que `hostname -I` devolve
dentro do WSL2. Cada execução também grava `src/<nome>/data/recordings/mission.acmi`.

---

## 5. Verificar

**Não há suíte de testes.** A verificação automatizada existente é de **determinismo**: roda N
frames de passo fixo com 1, 2 e 4 threads de tempo crítico e compara os dumps `frame=` — todos
têm de ser idênticos.

```bash
make check-single-thread   # determinismo com a decisão no laço de background
make check-multi-thread    # determinismo com os 4 agentes decidindo em paralelo, na fase 3
make compare-single-multi  # lista o que difere entre os dois subprojetos (deve ser só o agente)
```

`compare-single-multi` é o teste da tese do repositório: se a lista crescer além do agente, do
`.epp` e da observabilidade, alguma coisa vazou de um subprojeto para o outro.

> **Nota:** `make help` ainda lista alvos `run-*` de subprojetos que foram removidos
> (`01-flying-aircraft` … `12-jsbsim-ubf`). São entradas mortas no Makefile; os alvos válidos
> hoje são exatamente os seis listados acima mais `configure`/`build`/`install`/`clean`.

---

## 6. Como o projeto se organiza

```
poc-mixr/
├── src/                    subprojetos: um executável independente por pasta, todos compilados juntos
│   ├── single-thread/      decisão no ( SimAgent ) nativo, em updateData() — thread de background
│   └── multi-thread/       decisão no ( FlightAgentTC ) próprio, na fase 3 do frame de tempo crítico
├── shared/
│   ├── xtacview/           exportação para o Tacview (OutputHandler nativo do recorder)
│   ├── xclock/             controle de velocidade do tempo (acelerar / frear / pausar)
│   └── data/terrain/srtm/  tile SRTM do cenário — compartilhado pelos dois subprojetos
├── contexts/               material de consulta sobre MIXR e BehaviorTree.CPP (ver §9)
├── conanfile.py            dependências binárias
├── meson.build             raiz: resolve as libs por pkg-config e dá subdir() em cada subprojeto
├── meson_options.txt       uma opção: -Dasan
└── Makefile                orquestra Conan + Meson
```

### Anatomia de um subprojeto

```
src/<nome>/
├── meson.build            só faz subdir('./src')
├── configs/
│   ├── scenario.epp.in    cenário EDL — o .in é resolvido em runtime (ver app/ScenarioTemplate)
│   └── flight_tree.xml    a árvore de comportamento
├── data/                  dados vendorizados (jsbsim/, recordings/)
├── include/ e src/        espelhados; um arquivo, uma questão
│   ├── domain/            regras de negócio PURAS — sem MIXR, sem BT — testáveis isoladas
│   ├── xnative/           classes MIXR próprias (namespace mixr::xnative) + factory própria
│   ├── ubf/               percepção / decisão / atuação do UBF
│   ├── bt/                nós da árvore de comportamento e a factory deles
│   ├── app/               as etapas da aplicação, uma questão por arquivo
│   ├── mixr_factory.*     encadeia a factory própria ANTES das do framework
│   └── main.cpp           FINO: chama os módulos de app/ na ordem; não implementa comportamento
```

Três regras estruturam tudo:

1. **"O que fazer" mora em `domain/`; "como conectar" mora nas factories e adaptadores;
   `main.cpp` só orquestra.** `domain/` não inclui um único header do MIXR ou do
   BehaviorTree.CPP — é o que permite testar a política sem levantar uma simulação.
2. **Um arquivo, uma questão.** O que seria um `main.cpp` de ~450 linhas está quebrado em
   `app/`, e cada header abre com o *porquê* daquele passo. Vale o mesmo dentro de `ubf/` (a
   tabela de slots do `BtBehavior` mora num arquivo separado da decisão) e de `xnative/`
   (`Log`, `ThreadTag`, `BehaviorBoard` são um utilitário por questão).
3. **Estrutura vem do EDL, comportamento vem do C++.** Quais players existem, com quais
   subsistemas, em que taxa, com quantas threads — tudo isso é declarado em `configs/*.epp` e
   lido em tempo de carga. **Reconfigurar o cenário não recompila nada.**

### O modelo MIXR em uma tela

- Tudo herda de `mixr::base::Object` (ref-counting + RTTI própria): `DECLARE_SUBCLASS` no
  `.hpp`, `IMPLEMENT_SUBCLASS(Classe, "FactoryName")` no `.cpp`.
- Parâmetros configuráveis por EDL são **slots**: `BEGIN_SLOTTABLE`/`END_SLOTTABLE` +
  `BEGIN_SLOT_MAP`/`ON_SLOT`/`END_SLOT_MAP`, com `setSlotX()` privados.
- **Factory por nome:** cada `mixr_factory.cpp` encadeia as factories na ordem
  `local → xtacview → xclock → simulation → models → terrain → recorder → base`. **A primeira
  que retorna não-nulo vence** — por isso a factory própria vem sempre antes das do framework.
- Hierarquia: `Station` → `WorldModel` → `players` → `Player`, agregando `dynamicsModel`,
  sensores (`Antenna`/`RfSensor`/`Gimbal`/`Autopilot`/`Datalink`) e navegação
  (`Route`/`Steerpoint`).
- **Frame de tempo crítico por fases** — 0 `dynamics()`, 1 `transmit()`, 2 `receive()`,
  3 `process()`. O que roda em qual fase é o que permite o paralelismo determinístico.
- `station->updateData(dt)` no laço principal é o que **drena a fila do gravador** para a cadeia
  de `OutputHandler` — sem isso o Tacview não recebe nada.

---

## 7. Os subprojetos

Os dois são o **mesmo modelo**: quatro caças patrulhando quadrantes distintos sobre a Serra do
Mar, um intruso cruzando a área, e quem detecta avisa os outros pelo datalink — quem recebe o
aviso muda de comportamento e vai apoiar. Mesmo `Aircraft`, mesmo `JSBSimModel`, mesmo
`Autopilot`, mesmo radar, mesmo datalink, mesma árvore de comportamento, mesmos números.

**A única diferença é onde a decisão roda:**

| | [`src/single-thread/`](src/single-thread/) | [`src/multi-thread/`](src/multi-thread/) |
|---|---|---|
| agente do UBF | `( SimAgent )` **nativo** | `( FlightAgentTC )` **próprio** |
| mora em | componente da **`Station`**, ator amarrado por nome | componente do **`Player`**, ator = o container |
| decide em | `updateData()` — thread de **background** | fase 3 do `updateTC()` — thread de **tempo crítico** |
| os 4 agentes | **em sequência**, numa thread só, a 10 Hz | **em paralelo**, um por thread do pool, a 50 Hz |
| determinismo | propriedade do *harness* (o laço serializa `tcFrame` e `updateData`) | propriedade do **modelo** (a decisão está dentro do frame) |

> **Os nomes falam só da decisão, não da simulação.** `single-thread` **não** roda a simulação
> inteira numa thread: os dois subprojetos declaram `numTcThreads`, distribuem os players pelo
> pool de threads de tempo crítico do framework, e **os dois passam no check de determinismo com
> 1, 2 e 4 threads**.

Cada um tem um README que disseca o subprojeto arquivo por arquivo, na ordem em que as
dependências aparecem:

- **[src/single-thread/README.md](src/single-thread/README.md)** — a aula completa. O que vem do
  framework e o que sobra para escrever, como o framework chama o nosso código, a anatomia de um
  frame, a dissecação dos 74 arquivos de código em ordem de dependência, a cadeia de decisão
  UBF+BehaviorTree, os três canais de interação entre players, o terreno, o determinismo, o
  Tacview e as armadilhas medidas rodando.
- **[src/multi-thread/README.md](src/multi-thread/README.md)** — a mesma poc com uma única
  troca. Disseca a classe `FlightAgentTC` linha por linha (cada bloco dela existe por causa de
  uma armadilha do framework), onde a decisão entra no frame, e o que a troca faz com o
  determinismo e com a taxa de decisão — medido nos dois binários.

Comece pelo da `single-thread`: o da `multi-thread` é escrito como *delta* dele.

---

## 8. Bibliotecas compartilhadas

Ambas seguem o padrão `shared/x<nome>` dos exemplos oficiais do MIXR: classes num namespace
`mixr::x<nome>`, factory própria, expostas ao Meson como `x<nome>_dep`.

### `shared/xtacview/` — exportação para o Tacview

**É a única exportação Tacview do repositório; nenhum `main.cpp` monta stream ACMI.**
`TacviewOutput` é um `recorder::OutputHandler` de verdade, declarado na cadeia nativa do slot
`dataRecorder` da `Station` — o framework empurra cada registro do gravador para ele, que
traduz em linhas ACMI, transmite pela porta 1234 e grava o `.acmi`.

As armadilhas confirmadas rodando estão documentadas em
[shared/xtacview/TacviewOutput.cpp](shared/xtacview/TacviewOutput.cpp) e resumidas no
[CLAUDE.md](CLAUDE.md). A primeira delas economiza horas: **`dataLogTime` é slot do `Player`,
nasce zero, e sem `dataLogTime: ( Seconds 0.1 )` o player simplesmente nunca aparece no
Tacview.** Parece bug do handler; não é.

### `shared/xclock/` — controle de velocidade do tempo

O cenário declara **`( ClockStation )` no lugar de `( Station )`** — uma `simulation::Station`
com um único *override*. A divisão entre nativo e próprio é deliberada e vale como lição:

- **Acelerar é 100% nativo.** `Station::processTimeCriticalTasks()` já faz
  `for (jj=0; jj < getFastForwardRate(); jj++) tcFrame(dt)`. Nada foi escrito para isso.
- **Frear não existe no framework** — `fastForwardRate` é `unsigned int` (só multiplica). É a
  **única** coisa acrescentada: abaixo de `1x`, um `tcFrame(dt * fator)` com o `dt` encurtado.
  Passo de integração menor, nunca maior — não degrada o JSBSim.
- **Pausar é nativo, por um caminho não óbvio.** Não existe `Simulation::pause()`; o que existe
  é o *flag* de freeze do `base::Component`, que **não se propaga para os filhos** — a cascata é
  por *consulta*, no sentido inverso (`Player::isFrozen()` testa o próprio flag **ou** o da
  simulação). Por isso `setPaused()` age em `getSimulation()`, e não na `Station`.

### `shared/data/terrain/srtm/`

O tile SRTM1 `S23W043.hgt.gz` (Serra do Mar, RJ), **compartilhado pelos dois subprojetos** —
é a única exceção à regra de `data/` por subprojeto, porque são 12 MB do *cenário*, que é o
mesmo nas duas pocs gêmeas. O `.gz` é versionado; o `.hgt` descomprimido (25 MB) é artefato
gerado na primeira execução e está no `.gitignore`.

---

## 9. `contexts/` — onde consultar o framework

Duas camadas: os `.md` destilados (leitura rápida) e o **código-fonte completo das libs** (a
verdade).

| caminho | conteúdo |
|---|---|
| `contexts/MIXR-CONTEXT.md` | como o MIXR funciona por dentro: classes, macros, ciclo de vida, EDL, recorder, camada `terrain` |
| `contexts/MIXR-PATTERN-CONTEXT.md` | como se escreve uma aplicação MIXR (padrões dos exemplos oficiais); a **§0 lista o que o fork empacotado não tem** — leia antes de qualquer outra coisa |
| `contexts/BTCPP-CONTEXT.md` | BehaviorTree.CPP **3.5.6** — nada ali vale para a v4 |
| `contexts/src/mixr/` | **fonte completo** do fork 1.0.5: `src/` com as 313 implementações, `include/`, `deps/{jsbsim,openrti}`, `doc/`, `src/recorder/proto/DataRecord.proto` |
| `contexts/src/BehaviorTree.CPP/` | fonte da 3.5.6, com `examples/`, `sample_nodes/` e `tests/` |

Os `.md` são destilação; **`contexts/src/` é a verdade**. É ali que se responde "por que esse
token não chega no handler?" ou "o que esse slot faz de verdade" — lendo o `.cpp` do framework,
não adivinhando pelo header. Praticamente toda armadilha documentada neste repositório saiu
desse exercício.

**Atenção:** `contexts/src/` inteiro é **git-ignored** — são cópias locais das árvores de fonte,
não vêm num clone limpo. Sem elas, os headers instalados pelo Conan
(`~/.conan2/p/b/mixr*/p/include/mixr/...`) são o *fallback*. Em caso de divergência entre a
árvore de `contexts/src/` e o pacote Conan, **quem vale é o pacote** — é ele que está linkado.

---

## 10. Onde ler mais

| documento | para quê |
|---|---|
| **[CLAUDE.md](CLAUDE.md)** | guia operacional: comandos, arquitetura em uma tela, e o catálogo consolidado de armadilhas (rpath, unidades, recorder, xtacview, xclock, terreno) |
| **[src/single-thread/README.md](src/single-thread/README.md)** | a aula completa sobre um subprojeto, arquivo por arquivo, em ordem de dependência |
| **[src/multi-thread/README.md](src/multi-thread/README.md)** | onde uma decisão deve rodar, e o que isso custa |
| **[shared/xtacview/TacviewOutput.cpp](shared/xtacview/TacviewOutput.cpp)** | o protocolo *Real-Time Telemetry* e as sete armadilhas do gravador nativo |
