---
titulo: "MIXR — Padrões de Uso e Emprego (contexto para RAG)"
projeto: "MIXR (Mixed Reality Simulation) — framework C++ de Modelagem & Simulação"
fonte_primaria: "árvore `examples/` vendorizada neste repositório (release v18.01 — `examples/version.txt`), 48 aplicações + 4 bibliotecas compartilhadas, 826 arquivos"
fonte_secundaria: "cabeçalhos do pacote Conan `mixr/1.0.5` efetivamente linkado por esta PoC"
documento_irmao: "MIXR-CONTEXT.md — referência do FUNCIONAMENTO INTERNO do framework"
escopo: "como se CONSTRÓI uma aplicação com o MIXR: recorrências de código, de configuração, de build e de extensão observadas nos exemplos oficiais"
idioma: "português do Brasil; identificadores, nomes de slot e nomes de fábrica em inglês (originais)"
convencao_de_verdade: "toda afirmação foi conferida contra os arquivos de `examples/` e/ou contra os cabeçalhos instalados do fork; divergências entre o que os exemplos usam e o que o fork empacotado oferece são registradas explicitamente"
---

# COMO USAR ESTE DOCUMENTO

Este é o **terceiro** documento de contexto técnico da base de conhecimento desta PoC:

| Documento | Pergunta que responde |
|---|---|
| `MIXR-CONTEXT.md` | *Como o MIXR funciona por dentro?* (classes, macros, ciclo de vida, EDL, camadas) |
| `BTCPP-CONTEXT.md` | *Como o BehaviorTree.CPP funciona?* |
| **`MIXR-PATTERN-CONTEXT.md`** (este) | ***Como se escreve uma aplicação MIXR?*** (o que se repete em todos os exemplos oficiais, e por quê) |

`MIXR-CONTEXT.md` descreve o **mecanismo**; este documento descreve o **emprego** — o
conjunto de convenções que os exemplos oficiais aplicam de forma tão consistente que
constituem, na prática, o "jeito MIXR" de montar um programa. Quando um padrão aqui
depende de um mecanismo, o texto aponta a seção correspondente do documento irmão
(ex.: *"ver `MIXR-CONTEXT.md` §5.4"*), mas não a repete.

Convenções deste documento (as mesmas do documento irmão):

- **PADRÃO** — recorrência observada em vários exemplos; é o miolo do documento. Cada
  padrão nomeia os arquivos onde aparece, para que um trecho recuperado isoladamente
  continue rastreável.
- **REGRA** — norma que o padrão impõe: se você desviar, não funciona.
- **ARMADILHA** — comportamento real que contraria a expectativa razoável, incluindo
  erros e inconsistências presentes nos próprios exemplos oficiais.
- **POR QUÊ** — motivação da decisão de projeto por trás do padrão.
- **NESTA POC** — como o padrão se relaciona com o que já existe em `poc/NN-slug/`
  deste repositório, incluindo onde a PoC diverge conscientemente.
- Caminhos como `mainSim1/main.cpp` referem-se à árvore `examples/`. Caminhos como
  `poc/05-formation-flight/` referem-se a este repositório. Caminhos como
  `src/base/Component.cpp` referem-se à árvore de fontes do MIXR.
- Blocos de código são transcrição dos arquivos reais (eventualmente condensada, com
  omissões marcadas por `// ...`), nunca reescrita.

---

# 0. APLICABILIDADE: O FORK EMPACOTADO NÃO TEM TUDO QUE OS EXEMPLOS USAM

**Esta é a informação mais importante do documento e deve ser lida antes de qualquer
outra seção.** Boa parte dos exemplos oficiais não é reproduzível como está nesta PoC —
não por erro, mas porque o pacote Conan `mixr/1.0.5` consumido aqui **não publica as
bibliotecas gráficas**.

## 0.1 O que o pacote `mixr/1.0.5` realmente entrega

Conferido diretamente no diretório do pacote (`~/.conan2/p/b/mixr*/p/`):

```
lib/                                 include/mixr/
  libmixr_base.so                      base/
  libmixr_simulation.so                simulation/
  libmixr_models.so                    models/
  libmixr_terrain.so                   terrain/
  libmixr_linkage.so                   linkage/
  libmixr_recorder.so                  recorder/
  libmixr_linearsystem.so              linearsystem/
  libmixr_interop_common.so            interop/{common,dis,rprfom}
  libmixr_interop_dis.so               config.hpp
```

Nove bibliotecas. Oito funções de fábrica disponíveis (`mixr/base/factory.hpp`,
`simulation`, `models`, `terrain`, `linkage`, `recorder`, `interop/dis`,
`interop/rprfom`).

## 0.2 O que os exemplos usam e **não existe** aqui

| Biblioteca usada nos exemplos | Fábrica | Existe no pacote? |
|---|---|---|
| `mixr_graphics` | `mixr::graphics::factory` | **NÃO** |
| `mixr_ui_glut` | `mixr::glut::factory` | **NÃO** |
| `mixr_instruments` | `mixr::instruments::factory` | **NÃO** |
| `mixr_ighost_cigi` | `mixr::cigi::factory` | **NÃO** |
| `mixr_ighost_pov` | `mixr::pov::factory` | **NÃO** |
| `mixr_dafif` | — | **NÃO** |
| `mixr_map_vpf` | `mixr::vpf::factory` | **NÃO** |

**ARMADILHA** — **de 48 aplicações de exemplo, a grande maioria abre uma janela GLUT.**
Dos 38 `main.cpp` que usam o *builder* canônico (§2.2), o tipo-raiz construído é:

- `mixr::glut::GlutDisplay` ou subclasse de display em **20** deles;
- `mixr::simulation::Station` ou subclasse em **11**;
- outra coisa (`Simulation`, `StateMachine`, `Table`, `Rng`, `MyComp`, `Endpoint`,
  `Tester`, `DataRecordTest`, `Base`, `PuzzleBoard`) em **7**.

Ou seja: **os padrões de C++/EDL são todos aproveitáveis; os exemplos que os hospedam,
em geral, não são compiláveis nesta PoC**.

**REGRA** — ao portar um padrão de um exemplo para `poc/NN-slug/`, primeiro classifique
o exemplo:

1. **Diretamente aproveitável** — não toca em `graphics`/`glut`/`instruments`/`ighost`:
   `tutorial01`–`tutorial06`, `mainNonRT1`, `mainSim1`, `testStateMach`, `testSlots`,
   `testTables`, `testTimer`, `testNetHandler`, `testMetaObject`, `testMatrix`,
   `testNavUtils`, `testLinearSys`, `testTemplates`, `testRecorderWrite`,
   `testRecorderRead` (a parte de leitura), `shared/xrecorder`, `shared/xzmq`.
2. **Aproveitável só na parte de cenário/EDL e nas classes de modelo** — o `main.cpp` e
   os displays são descartáveis, mas o `.epp` do cenário e as classes de sensor/estação
   são referência válida: `mainCockpit`, `testRadar`, `testInfrared`, `mainGndMapRdr`,
   `mainUbf1`, `mainSim2`, `mainSim3`, `mainLaero`, `testRecordData`, `mainTerrain`.
3. **Não aproveitável** — é sobre gráficos e nada mais: `demoEfis`,
   `demoFlightDisplays1/2`, `demoInstruments`, `demoSubDisplays`, `testGraphics`,
   `mainGlut`, `tutorial07`, `tutorial08`, `testVmap`, `mainIgViewer`, `mainQt1`,
   `mainPuzzle1/2`, `testLinkage` (o display), `testEvents` (o display).

**POR QUÊ** — o fork foi empacotado para uso *headless*/servidor (simulação
distribuída, DIS, gravação), não para cockpit gráfico. É exatamente por isso que todas
as pocs deste repositório resolvem visualização por **Tacview Real-Time Telemetry**
(`poc/04`, `05`, `07`, `08`, `09`, `10`) em vez de por `GlutDisplay`: não há
`GlutDisplay` disponível.

**NESTA POC** — o mesmo raciocínio explica por que `poc/05-formation-flight` implementou
teclado com `mixr::linkage::IoDevice`/`IoHandler` + termios em modo raw em vez de usar
os *callbacks* de teclado do `GlutDisplay` (que é como **todos** os exemplos oficiais
fazem — ver §8.2): `linkage` existe no pacote, `glut` não.

## 0.3 O que os exemplos revelam mesmo sem compilar

Ainda assim, `examples/` é a melhor fonte disponível para:

- o **formato canônico** de `main.cpp`, `factory.cpp`, `.epp` (§2, §5);
- a **anatomia completa de um cenário** com radar, RWR, jammer, armas, rotas, terreno e
  DIS (§6) — que é o que as pocs 06/07/08/09 replicam;
- os **cinco pontos de extensão** realmente exercitados (§7);
- os **mecanismos de comunicação** entre componentes (§8) — base da `poc/08-event-relay`;
- as **duas arquiteturas de IA** nativas: UBF e `StateMachine` (§10) — comparáveis à
  escolha de BehaviorTree.CPP feita em `poc/02`/`poc/03`.

---

# 1. MAPA DOS EXEMPLOS: O QUE CADA UM ENSINA

Catálogo funcional da árvore `examples/` (release v18.01). A coluna "Ensina" resume o
padrão dominante; a coluna "§" aponta a seção deste documento que o disseca.

## 1.1 Tutoriais — a progressão pedagógica oficial

| Exemplo | Ensina | § |
|---|---|---|
| `tutorial01` | Linkar com uma única lib (`base`); `new` + `unref()` | §4.1 |
| `tutorial02` | Esqueleto mínimo de classe MIXR (`DECLARE_/IMPLEMENT_SUBCLASS`, `EMPTY_SLOTTABLE`) | §4.1 |
| `tutorial03` | Adicionar *slot table* + `edl_parser` + `factory()` + `builder()` | §4.2, §2 |
| `tutorial04` | Classe abstrata + família de subclasses (`IMPLEMENT_ABSTRACT_SUBCLASS`) | §4.3 |
| `tutorial05` | *Slots* de tipos ricos: `PairStream`, `List`, `Identifier`, `String`; `isValid()` | §4.2 |
| `tutorial06` | Primeira classe `Component`: árvore, `tcFrame()`/`updateData()`/`reset()` | §4.4, §3.1 |
| `tutorial07` | Primeira aplicação gráfica: `GlutDisplay` + `Graphic` + laço por `glutTimerFunc` | §3.4 |
| `tutorial08` | Múltiplas páginas (`Page`, `pagingEvent`), texturas, fontes | §11.1 |

**POR QUÊ** — a progressão é deliberada: `Object` → *slots* → herança → `Component` →
gráficos. Um subprojeto novo em `poc/` que crie uma classe própria deve seguir
`tutorial03`+`tutorial06` como gabarito (é o que `poc/03-bt-autopilot` faz).

## 1.2 Simulações — do mínimo ao completo

| Exemplo | Ensina | § |
|---|---|---|
| `mainNonRT1` | Simulação **sem `Station`**: só `WorldModel`, laço o mais rápido possível | §3.2 |
| `mainSim1` | A simulação mínima *full-featured*: `Station` + laço de fundo próprio com `msleep` | §3.3 |
| `mainSim2` | + joystick (`SimIoHandler`), `JSBSimModel`, painel de instrumentos | §9 |
| `mainSim3` | + mapa com todos os *players* (`SymbolLoader`, `MapPage`) | §11.2 |
| `mainCockpit` | Cockpit completo: radar TWS+GMTI, RWR, mísseis, HOTAS, CIGI, DIS | §6 |
| `mainUbf1` | O mesmo cockpit, mas o avião é pilotado por **agentes UBF** | §10.1 |
| `mainLaero` | `LaeroModel` + `Autopilot` + `Route` comandados por botões de tela | §6.5 |
| `mainQt1` | Mesma `Station`, laço de fundo por `QTimer` em vez de GLUT | §3.5 |

## 1.3 Sensores, terreno e ambiente

| Exemplo | Ensina | § |
|---|---|---|
| `testRadar` | O cenário de radar canônico (1 vs 3, N², multi-thread, DIS) | §6.3 |
| `testInfrared` | Cadeia IR completa: `IrAtmosphere`, `IrSignature`, míssil com *seeker* | §6.8 |
| `mainGndMapRdr` | **Estender `Radar`** para radar de mapeamento de solo com terreno | §7.4 |
| `mainTerrain` | `QuadMap`/`SrtmHgtFile`/`DtedFile`; sombreamento, AAC, curvatura | §6.6 |
| `testVmap` | Carga de mapas vetoriais VMAP0 | — |

## 1.4 Infraestrutura

| Exemplo | Ensina | § |
|---|---|---|
| `testEvents` | `send()` com todos os tipos + `ON_EVENT_OBJ` | §8.1, §8.2 |
| `testStateMach` | `StateMachine`: tabela de estados, `call()`/`rtn()`, submáquinas | §10.2 |
| `testLinkage` | Pilha `IoHandler`→`IoData`→`devices`→`adapters`; `MockDevice` | §9 |
| `testTimer` | `UpTimer`/`DownTimer` + `PeriodicThread` própria | §3.6 |
| `testTables` | LFI 1D–4D, `FStorage`, medição de tempo | §6.9 |
| `testSlots` | *Slot* de mesmo nome em base e derivada (sombreamento) | §4.8 |
| `testMetaObject` | `MetaObject`: contagem de instâncias por classe | §4.9 |
| `testNetHandler` | UDP unicast/broadcast/multicast, TCP, ZeroMQ | §13 |
| `testRecordData` | Anexar um `DataRecorder` a uma simulação | §12.1 |
| `testRecorderWrite` | Gerar arquivo de gravação sintético | §12.3 |
| `testRecorderRead` | Ler e imprimir gravação; `PrintHandler` próprio | §12.2 |
| `testDafif` | **Estender `WorldModel`** para carregar bases DAFIF | §7.5 |
| `testNavUtils`, `testMatrix`, `testLinearSys`, `testTemplates`, `testUnits` | Exercícios de biblioteca, sem padrão arquitetural novo | — |

## 1.5 Bibliotecas compartilhadas (`shared/`) — o padrão "biblioteca de exemplo"

| Biblioteca | Conteúdo | § |
|---|---|---|
| `xbehaviors` | Comportamentos UBF de avião (`PlaneState`, `PlaneAction`, 12 *behaviors*, `PriorityArbiter`) | §10.1 |
| `xpanel` | Componentes gráficos de cockpit (`DspRadar`, `DspRwr`, `Pfd`, `Hsi`) + `xpanel.epp` | §11.3 |
| `xrecorder` | **Estender `DataRecorder`** com eventos e mensagens protobuf próprias | §7.7 |
| `xzmq` | **Estender `NetHandler`** com ZeroMQ | §7.8 |

**PADRÃO** — o "x" inicial significa *eXample*. Uma biblioteca `x*` tem sempre a mesma
estrutura: `factory.hpp`/`factory.cpp` no seu próprio *namespace*
(`mixr::xpanel`, `mixr::xzmq`, …), classes que herdam de classes do framework, e um
`Makefile` que produz um `.a` estático em `$(MIXR_EXAMPLES_LIB_PATH)`.

**ARMADILHA** — o `README.md` de `examples/` lista `testRng`, que **não existe** na
árvore; e escreve `mainIGViewer` onde o diretório é `mainIgViewer`. Também menciona
"`mainy1`" nos READMEs de `testRecordData`/`testRecorderRead` — nome antigo do que hoje
é `mainCockpit`. Trate o `README.md` como desatualizado; a lista autoritativa é a
variável `APPLICATIONS` do `examples/Makefile`.

**ARMADILHA** — `examples/meson.build` só constrói `tutorial01`–`tutorial06`. Todo o
resto só tem `Makefile` (GNU make + variáveis de ambiente de `setenv.sh`). Isso é
relevante para esta PoC, que é 100% Meson: **não há build Meson de referência para
nenhuma aplicação completa** — o `meson.build` de `poc/NN-slug/` é invenção própria
desta PoC (ver §14.4).

---

# 2. O PADRÃO "APLICAÇÃO MIXR": ANATOMIA DE UM `main.cpp`

Toda aplicação MIXR — dos 8 tutoriais ao cockpit completo — tem o mesmo esqueleto de
quatro peças:

```
main.cpp
├── factory(name)   → resolve string do arquivo EDL para objeto C++
├── builder(file)   → parseia o EDL e devolve a raiz já com o tipo certo
├── main()          → constrói, reseta, cria thread(s), entra no laço
└── laço/callback   → chama updateTC()/updateData() no ritmo certo
```

## 2.1 PADRÃO: `factory()` encadeada — a ordem importa

**Arquivos**: todos os `main.cpp`/`factory.cpp` (38 ocorrências).

```cpp
// mainSim1/main.cpp — na íntegra
mixr::base::Object* factory(const std::string& name)
{
   // example libraries
   mixr::base::Object* obj{mixr::xzmq::factory(name)};

   // framework libraries
   if (obj == nullptr) obj = mixr::cigi::factory(name);
   if (obj == nullptr) obj = mixr::pov::factory(name);
   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);
   if (obj == nullptr) obj = mixr::terrain::factory(name);
   if (obj == nullptr) obj = mixr::dis::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);
   return obj;
}
```

Quando a aplicação tem classes próprias, elas vêm **antes** de tudo:

```cpp
// mainSim2/factory.cpp — condensado
mixr::base::Object* factory(const std::string& name)
{
    mixr::base::Object* obj {};

    if      ( name == SimStation::getFactoryName()     ) obj = new SimStation();
    else if ( name == SimIoHandler::getFactoryName()   ) obj = new SimIoHandler();
    else if ( name == SimPlayer::getFactoryName()      ) obj = new SimPlayer();
    else if ( name == InstrumentPanel::getFactoryName()) obj = new InstrumentPanel();

    if (obj == nullptr) obj = mixr::xzmq::factory(name);      // libs de exemplo
    if (obj == nullptr) obj = mixr::cigi::factory(name);      // libs do framework
    // ... (11 fábricas)
    if (obj == nullptr) obj = mixr::base::factory(name);      // base SEMPRE por último
    return obj;
}
```

**REGRA** — a ordem canônica é: **(1) classes da aplicação → (2) bibliotecas `x*` de
exemplo → (3) bibliotecas do framework, das mais específicas para as mais genéricas →
(4) `mixr::base::factory` por último.** A primeira fábrica que devolver não-nulo vence.

**POR QUÊ** — `base` é a mais genérica e a que mais nomes registra (≈102, ver
`MIXR-CONTEXT.md` §7.6); se viesse primeiro, sequestraria nomes que uma camada superior
pretendia registrar. Colocar as classes da aplicação primeiro permite **sobrescrever**
um nome de fábrica do framework — é exatamente o que `testDafif` faz ao registrar sua
própria classe com o nome de fábrica `"WorldModel"` (§7.5).

**PADRÃO** — o `factory()` mora no próprio `main.cpp` quando a aplicação tem 0–2 classes
próprias (`mainSim1`, `mainSim3`, `mainNonRT1`, `testEvents`, `testTimer`, tutoriais) e
é extraído para `factory.cpp`/`factory.hpp` quando são mais (`mainCockpit`, `mainSim2`,
`mainUbf1`, `mainGndMapRdr`, `mainIgViewer`, `mainLaero`, `mainPuzzle1/2`, `testRadar`,
`testInfrared`, `testLinkage`, `testRecordData`, `testRecorderRead`, `testStateMach`,
`mainTerrain`). O par `factory.hpp` é sempre este, sem variação:

```cpp
// mainSim2/factory.hpp — na íntegra
#ifndef __factory_H__
#define __factory_H__
#include <string>
namespace mixr { namespace base { class Object; } }
mixr::base::Object* factory(const std::string& name);
#endif
```

**ARMADILHA** — três `factory.cpp` oficiais chamam `mixr::instruments::factory(name)`
**duas vezes** na mesma cadeia (`mainCockpit`, `mainUbf1`, `testRecordData`). É
inofensivo (a segunda chamada só ocorre se a primeira devolveu nulo, o que garante que
devolverá nulo de novo), mas denuncia que essas cadeias são copiadas e coladas entre
exemplos sem revisão. Não replique.

**ARMADILHA** — `testRecorderWrite/main.cpp` encadeia `recorder::factory` **depois** de
`base::factory`, invertendo a regra. Funciona por acaso (nenhum nome colide), mas é o
contraexemplo da convenção.

**NESTA POC** — o `factory()` de cada `poc/NN-slug/src/main.cpp` segue a regra reduzida
ao que existe no pacote: classes locais → `models` → `simulation` → `terrain` →
`linkage` → `recorder` → `dis` → `base`. Em `poc/03-bt-autopilot` a factory de objetos
MIXR foi extraída para `mixr_factory.cpp` — que é exatamente o padrão `factory.cpp` dos
exemplos maiores, com o nome adaptado para não colidir com a `BehaviorTreeFactory` do
BT.CPP.

## 2.2 PADRÃO: `builder()` — cinco passos invariantes

**Arquivos**: **38 arquivos** contêm este bloco praticamente idêntico, incluindo o mesmo
comentário `// do we have a base::Pair, if so, point to object in Pair, not Pair itself`.

```cpp
// mainSim1/main.cpp — o gabarito, na íntegra
mixr::simulation::Station* builder(const std::string& filename)
{
   // (1) lê o arquivo de configuração
   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, factory, &num_errors)};
   if (num_errors > 0) {
      std::cerr << "File: " << filename << ", number of errors: " << num_errors << std::endl;
      std::exit(EXIT_FAILURE);
   }

   // (2) algum objeto foi criado?
   if (obj == nullptr) {
      std::cerr << "Invalid configuration file, no objects defined!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   // (3) se veio um base::Pair, aponte para o objeto DENTRO do Pair, não para o Pair
   const auto pair = dynamic_cast<mixr::base::Pair*>(obj);
   if (pair != nullptr) {
      obj = pair->object();
      obj->ref();
      pair->unref();
   }

   // (4) confira o tipo
   const auto station = dynamic_cast<mixr::simulation::Station*>(obj);
   if (station == nullptr) {
      std::cerr << "Invalid configuration file!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   // (5) devolve já tipado
   return station;
}
```

**REGRA** — o passo (3) não é opcional. Se o arquivo EDL de nível superior começar com
um rótulo (`meuNome: ( Station ... )`), `edl_parser()` devolve um `base::Pair`, não o
`Station`. O bloco de desempacotamento cuida dos dois casos com um único código. O
`obj->ref()` antes do `pair->unref()` é obrigatório: sem ele o `unref()` do `Pair`
destrói o objeto contido.

**POR QUÊ** — `edl_parser()` tem assinatura genérica (`Object*`) porque não sabe o que o
arquivo descreve. O *builder* é o ponto onde a aplicação declara sua expectativa e falha
cedo, com mensagem, se ela não se cumprir. É a única validação de tipo que existe entre
o arquivo de configuração e o programa.

**PADRÃO** — o *builder* é sempre **especializado no tipo-raiz esperado** e leva o nome
da função `builder`, com o tipo de retorno mudando por aplicação. Levantamento completo
dos 38:

| Tipo-raiz devolvido | Exemplos |
|---|---|
| `mixr::glut::GlutDisplay` | `mainGlut`, `demoEfis`, `demoFlightDisplays1/2`, `demoInstruments`, `demoSubDisplays`, `tutorial07`, `tutorial08` |
| subclasse de display própria | `mainTerrain` (`Display`), `testEvents` (`Display`), `testGraphics`/`testVmap` (`TestDisplay`), `testLinkage` (`TestIoDisplay`), `mainPuzzle1/2` (`PuzzleBoard`) |
| `mixr::simulation::Station` | `mainSim1`, `mainCockpit`, `mainUbf1`, `mainIgViewer`, `mainLaero`, `testRecordData` |
| subclasse de `Station` | `mainSim2` (`SimStation`), `mainSim3`/`mainQt1` (`Station`), `testRadar`/`testInfrared`/`mainGndMapRdr` (`TestStation`) |
| `mixr::simulation::Simulation` | `mainNonRT1` |
| outros `Component`/`Object` | `testStateMach` (`StateMachine`), `testTables` (`Table`), `testTimer` (`Tester`), `testNetHandler` (`Endpoint`), `testRecorderRead`/`testRecorderWrite` (`DataRecordTest`), `tutorial03` (`Rng`), `tutorial04` (`AbstractRng`), `tutorial05` (`MyObj`), `tutorial06` (`MyComp`), `testSlots` (`Base`) |

**PADRÃO** — quando o programa é *namespaced* (`mainSim2`, `testRecorderRead`), o
`main()` real vira um *wrapper* fino:

```cpp
// mainSim2/main.cpp — final do arquivo
namespace mixr { namespace example {
   int main(int argc, char* argv[]) { /* ... o main de verdade ... */ }
}}
int main(int argc, char* argv[]) { return mixr::example::main(argc, argv); }
```

**NESTA POC** — todos os `poc/NN-slug/src/main.cpp` reproduzem os cinco passos. Em
`poc/05` e `poc/06` há um passo **(0)** a mais antes do parse — o preprocessador C
(`preprocessEdl()`), necessário porque o `.epp` usa `#include` (ver §5.1).

## 2.3 PADRÃO: seleção do arquivo de configuração por `-f`

```cpp
   // default configuration filename
   std::string configFilename = "test1.edl";
   // parse arguments
   for (int i = 1; i < argc; i++) {
      if ( std::string(argv[i]) == "-f" ) {
         configFilename = argv[++i];
      }
   }
```

Presente em 20+ `main.cpp`. Sempre com um **padrão embutido** no código, de modo que o
programa rode sem argumentos. O nome padrão é sempre um `.edl` do próprio diretório
(`test0.edl`, `test1.edl`, `test.edl`, `puzzle.edl`, `file0.edl`, `test00.edl`).

**ARMADILHA** — o laço não valida `i+1 < argc`: `programa -f` (sem valor) lê além do
fim de `argv`. Erro presente em todos os exemplos; não replique.

**POR QUÊ** — um exemplo como `testRadar` traz 9 cenários alternativos
(`test1.edl` … `test4c.edl`) para o mesmo binário. `-f` é o que torna o exemplo
multi-cenário sem recompilar — a promessa central do EDL (ver `MIXR-CONTEXT.md` §1.1).

## 2.4 PADRÃO: escolha do tipo-raiz — três níveis de "simulação"

Os exemplos mostram três pontos de entrada válidos, em ordem crescente de infraestrutura:

**(a) `Component` qualquer** — nem simulação é. Você mesmo chama `tcFrame()` e
`updateData()`. Usado em `tutorial06`, `testStateMach`, `testTimer`, `testNetHandler`,
`testTables`.

**(b) `simulation::Simulation`/`models::WorldModel`** — há *players*, tempo simulado e
as quatro fases de quadro, mas **não** há `Station`: sem *threads*, sem E/S, sem rede,
sem gravador. Único exemplo: `mainNonRT1`.

```cpp
// mainNonRT1/main.cpp — o laço inteiro
mixr::simulation::Simulation* simulation{builder(configFilename)};
simulation->reset();
const double dt{1.0 / static_cast<double>(frameRate)};   // frameRate = 50
for(; simulation->getExecTimeSec() < 50.0; ) {
   std::cout << simulation->getExecTimeSec() << std::endl;
   simulation->tcFrame( dt );
   simulation->updateData( dt );
}
```

```
// mainNonRT1/configs/test1.epp — o EDL inteiro: nem Station existe
( WorldModel
    latitude:   41.0
    longitude: -112
    players: { p1: ( Aircraft id: 101 type: "F-16A" ... ) }
)
```

**(c) `simulation::Station`** — o caso normal. A `Station` traz *threads* T/C, rede e
fundo, `ioHandler`, `igHosts`, `networks`, `dataRecorder` e o conceito de *ownship*.

**REGRA** — se você não precisa de E/S, rede, IG nem gravação, **(b)** é legítimo e mais
simples. `mainNonRT1` prova que uma simulação MIXR roda sem `Station`.

**NESTA POC** — `poc/01`, `poc/03`, `poc/04` etc. usam **(c)** mesmo quando poderiam usar
**(b)**, porque precisam da *thread* T/C (`createTimeCriticalProcess()`) para que o
tempo simulado avance sozinho. `poc/10-satellite-constellation` depende diretamente de um
slot que **só existe na `Station`** — `fastForwardRate` —, o que torna **(c)**
obrigatório ali.

---

# 3. PADRÕES DE LAÇO PRINCIPAL

O MIXR separa **tempo crítico** (`updateTC()`, física/sensores, taxa fixa alta) de
**tempo não-crítico** (`updateData()`, gráficos/lógica lenta, taxa baixa) — ver
`MIXR-CONTEXT.md` §5.3 e §21.1. Os exemplos exibem **seis** arranjos distintos para
conduzir essas duas cadências. O que muda entre eles é apenas **quem chama o quê e a
partir de qual thread**.

## 3.1 PADRÃO A: totalmente manual, sem thread

**Arquivo**: `tutorial06/main.cpp`.

```cpp
MyComp* myComp{builder(configFilename)};
const double dt{1.0 / static_cast<double>(frameRate)};   // frameRate = 20
myComp->tcFrame(dt);      // tempo crítico
myComp->updateData(dt);   // não-crítico
myComp->reset();          // reseta a árvore
myComp->tcFrame(dt);
myComp->updateData(dt);
myComp->unref();
```

Sem laço nem relógio: só demonstra que `tcFrame()`/`updateData()`/`reset()` percorrem a
árvore de componentes. É o modelo mental mínimo.

**NESTA POC** — é literalmente o que `src/main.cpp` ("mixr-hello") faz.

## 3.2 PADRÃO B: não-real-time, o mais rápido possível

**Arquivo**: `mainNonRT1/main.cpp` (transcrito em §2.4).

Sem `msleep`, sem relógio de parede: o `dt` é **fixo e fictício** (1/50 s) e o critério
de parada é o **tempo simulado** (`getExecTimeSec() < 50.0`). O programa consome 100% de
uma CPU e termina assim que 50 s simulados se passaram.

**POR QUÊ** — análise em lote (Monte Carlo, varredura de parâmetros) não tem por que
esperar o relógio. O MIXR não impõe tempo real; tempo real é uma escolha do laço.

## 3.3 PADRÃO C: *thread* T/C nativa + laço de fundo próprio com `msleep`

**Arquivos**: `mainSim1/main.cpp`, `testNetHandler/main.cpp`.

```cpp
// mainSim1/main.cpp — condensado
mixr::simulation::Station* station{builder(configFilename)};

// reseta e roda UM quadro T/C na thread principal, antes de qualquer thread existir
station->event(mixr::base::Component::RESET_EVENT);
station->tcFrame( 1.0/static_cast<double>(station->getTimeCriticalRate()) );

// cria a thread de tempo crítico do próprio framework
station->createTimeCriticalProcess();
mixr::base::msleep(1000);              // pausa para o SO subir a thread

const double dt{1.0/static_cast<double>(bgRate)};        // bgRate = 10 Hz
double simTime{};
const double startTime{mixr::base::getComputerTime()};

for(;;) {
   station->updateData( dt );                            // fase não-crítica

   simTime += dt;                                        // instante do PRÓXIMO quadro
   const double timeNow{mixr::base::getComputerTime()};
   const double elapsedTime{timeNow - startTime};
   const double nextFrameStart{simTime - elapsedTime};
   const int sleepTime{static_cast<int>(nextFrameStart * 1000.0)};
   if (sleepTime > 0) mixr::base::msleep(sleepTime);
}
```

**REGRA** — o cálculo de espera usa **tempo absoluto acumulado** (`simTime` contra
`startTime`), não `msleep(dt)` fixo. Isso evita *drift*: se um quadro atrasar, o
seguinte dorme menos e a cadência média se mantém. É o mesmo princípio de
`base::PeriodicThread` (ver `MIXR-CONTEXT.md` §12.2).

**ARMADILHA** — `mixr::base::msleep(1000)` depois de `createTimeCriticalProcess()`
existe porque **não há sincronização** entre a criação da *thread* e o primeiro quadro
de fundo. É uma espera arbitrária de 1 s, não um *handshake*.

**NESTA POC** — este é o padrão de laço usado por **todas** as pocs (`poc/01` a
`poc/10`), com duas adaptações: (a) o `msleep(1000)` foi mantido, e (b) o laço tem
condição de parada (30 s em `poc/01`, `SIGINT` em `poc/04`+) em vez de `for(;;)` puro.

## 3.4 PADRÃO D: *thread* T/C nativa + `glutTimerFunc` para o fundo

**Arquivos**: `mainCockpit`, `mainSim2`, `mainSim3`, `mainUbf1`, `testRadar`,
`testInfrared`, `mainGndMapRdr`, `mainLaero`, `testRecordData`, `mainIgViewer` — ou seja,
**todas** as aplicações gráficas com `Station`.

```cpp
// mainCockpit/main.cpp — o callback de fundo
void updateDataCB(int msecs)
{
   glutTimerFunc(msecs, updateDataCB, msecs);   // reagenda a si mesmo, sempre primeiro

   const double time{mixr::base::getComputerTime()};
   static double time0{time};                   // N-1
   const double dt{time - time0};               // dt REAL medido, não nominal
   time0 = time;

   station->updateData(dt);
}

int main(int argc, char* argv[])
{
   glutInit(&argc, argv);
   station = builder(configFilename);

   station->event(mixr::base::Component::RESET_EVENT);

   const double dt{1.0 / static_cast<double>(BG_RATE)};   // BG_RATE = 10
   const int msecs{static_cast<int>(dt * 1000)};

   station->updateData(dt);      // "ensure everything is reset"
   station->updateTC(dt);

   glutTimerFunc(msecs, updateDataCB, msecs);
   station->createTimeCriticalProcess();
   glutMainLoop();               // não retorna
}
```

**REGRA** — `glutTimerFunc()` é *one-shot*: o *callback* **precisa** reagendar a si
mesmo na primeira linha. Todos os exemplos fazem isso antes de qualquer trabalho, para
que uma exceção no corpo não mate a cadência.

**PADRÃO** — o `dt` do fundo é **medido**, não nominal: `getComputerTime()` menos o valor
da chamada anterior guardado em `static double time0`. Contrasta com o padrão C, onde o
`dt` é nominal e a espera é ajustada. Aqui não há como ajustar (quem agenda é o GLUT),
então mede-se.

**ARMADILHA** — nas aplicações gráficas, a `Station` derivada **não** propaga
`updateData()` para o display:

```cpp
// mainSim2/SimStation.cpp
void SimStation::updateData(const double dt)
{
    // ### Don't call updateData for our 'mainDisplay', which is derived from
    // graphics::GlutDisplay, because graphics::GlutDisplay handles calling updateData()
    // for it's own displays.
    // ...
    BaseClass::updateData(dt);
}
```

Mas **propaga** `updateTC()`:

```cpp
void SimStation::updateTC(const double dt)
{
    BaseClass::updateTC(dt);                 // primeiro a simulação
    mixr::base::Timer::updateTimers(dt);
    mixr::graphics::Graphic::flashTimer(dt);
    if (mainDisplay != nullptr) mainDisplay->updateTC(dt);
}
```

Chamar `updateData()` no display a partir da `Station` causaria dupla atualização.

## 3.5 PADRÃO E: *thread* T/C nativa + `QTimer` (Qt)

**Arquivos**: `mainQt1/main.cpp`, `mainQt1/StnTimerObject.cpp`.

```cpp
// mainQt1/main.cpp — o main inteiro, depois do builder
station->event(mixr::base::Component::RESET_EVENT);
station->createTimeCriticalProcess();
station->createWindow(argc, argv);     // entra no QApplication::exec() lá dentro
```

```cpp
// mainQt1/StnTimerObject.cpp — o "laço de fundo"
StnTimerObject::StnTimerObject(Station* station, QObject* parent)
   : QObject(parent), stn(station)
{
   if (stn != nullptr) stn->ref();
   bgTimer = new QTimer(this);
   connect(bgTimer, SIGNAL(timeout()), this, SLOT(updateStation()));
   bgTimer->start(20);                 // 20 ms = 50 Hz
}

void StnTimerObject::updateStation()
{
   if (stn != nullptr) stn->updateData(20.0f/1000.0f);
}
```

**POR QUÊ** — demonstra que o MIXR **não impõe** *toolkit* de interface: a `Station` só
precisa que alguém chame `updateData()` periodicamente. Trocar GLUT por Qt é trocar o
agendador, nada mais.

**ARMADILHA** — aqui o `dt` é **nominal e fixo** (`20/1000`), não medido. Se o `QTimer`
atrasar, o tempo não-crítico da simulação diverge do relógio de parede. Os exemplos GLUT
medem; o Qt não. Prefira medir.

**NESTA POC** — o mesmo raciocínio autoriza o que as pocs fazem: substituir o *toolkit*
gráfico por um exportador Tacview e conduzir `updateData()` do laço próprio.

## 3.6 PADRÃO F: `PeriodicThread` própria para uma tarefa auxiliar

**Arquivo**: `testTimer/main.cpp`.

```cpp
class TimerThread final : public mixr::base::PeriodicThread
{
   public: TimerThread(mixr::base::Component* const parent, const double rate);
   private: unsigned long userFunc(const double dt) override;
};

unsigned long TimerThread::userFunc(const double dt)
{
   mixr::base::Timer::updateTimers(dt);
   return 0;
}

TimerThread* createTheThread(Tester* const tester)
{
   auto thread = new TimerThread(tester, THREAD_RATE);   // 20 Hz
   bool ok{thread->start(THREAD_PRI)};                   // prioridade 0.5 em [0,1]
   if (!ok) { thread = nullptr; std::cerr << "..." ; }
   return thread;
}
```

E o encerramento explícito, na ordem certa:

```cpp
tester->event(mixr::base::Component::SHUTDOWN_EVENT);
tester->unref();  tester = nullptr;
thread->terminate();
thread->unref();  thread = nullptr;
```

**REGRA** — herde `PeriodicThread`, sobrescreva só `userFunc(dt)`, chame `start(pri)`
com prioridade normalizada em [0,1], e ao final `terminate()` + `unref()`. O
`SHUTDOWN_EVENT` vai para o **componente**, não para a *thread*.

**POR QUÊ** — `base::Timer::updateTimers()` é global e estático; alguém precisa
chamá-lo. Numa aplicação com `Station`, isso já acontece dentro de `Station::updateTC()`
(via o slot `enableUpdateTimers`) ou no `updateTC()` da `Station` derivada (§3.4). Sem
`Station`, você provê a *thread*.

## 3.7 PADRÃO TRANSVERSAL: a sequência de *priming*

Antes de entrar no laço, os exemplos com `Station` executam uma sequência de aquecimento
que **varia de exemplo para exemplo** — sinal de que ninguém tem certeza do mínimo
necessário:

| Exemplo | Sequência antes do laço |
|---|---|
| `mainSim1` | `event(RESET)` → `tcFrame(1/tcRate)` → `createTimeCriticalProcess()` → `msleep(1000)` |
| `mainCockpit` | `event(RESET)` → `updateData(dt)` → `updateTC(dt)` → `glutTimerFunc` → `createTimeCriticalProcess()` |
| `mainSim2` | `event(RESET)` → `updateData(dt)` → `updateTC(dt)` → `event(RESET)` → `glutTimerFunc` → `createTimeCriticalProcess()` |
| `mainSim3`, `testRadar` | `event(RESET)` → `updateData(dt)` → `updateTC(dt)` → `event(RESET)` → … |
| `mainUbf1` | `event(RESET)` → `updateData(dt)` → `updateTC(dt)` → `event(RESET)` → **`reset()`** → … |
| `mainQt1` | `event(RESET)` → `createTimeCriticalProcess()` → `createWindow()` |
| `mainNonRT1` | `reset()` apenas |

**PADRÃO** — o denominador comum, presente em todos, é **`event(RESET_EVENT)` antes do
primeiro quadro**. O restante é aquecimento defensivo, comentado nos fontes como
*"ensure everything is reset"*.

**POR QUÊ** — `reset()` não reinicializa nada por conta própria; ele **propaga** e cada
componente reage (ver `MIXR-CONTEXT.md` §5.5). Vários componentes só criam recursos no
`reset()` — a `Station` derivada cria a janela GLUT ali (§7.1), o `Antenna` inicializa a
varredura, o `JSBSimModel` chama `RunIC()`. Sem o `RESET_EVENT` inicial, o primeiro
quadro roda contra estado não inicializado.

**ARMADILHA** — `mainUbf1` chama **`event(RESET_EVENT)` e depois `reset()`** — dois
*resets* seguidos por caminhos diferentes. `event(RESET_EVENT)` despacha pela tabela de
eventos (que, em `Component`, chama `reset()`); `reset()` chama direto. É redundante.

**ARMADILHA** — a `Station` tem o slot nativo **`startupResetTime`** (`base::Time`) que
faz exatamente esse *reset* inicial após N segundos de execução, sem código no `main()`.
Nenhum exemplo o utiliza; todos fazem à mão. A documentação do slot no cabeçalho diz:
*"some simulations may need this -- let it run a few initial frames then reset"*.

**NESTA POC** — as pocs usam a variante do `mainSim1` (`RESET` + um `tcFrame` +
`createTimeCriticalProcess` + `msleep`), que é a mais enxuta entre as testadas.

---

# 4. PADRÕES DE CLASSE: O BOILERPLATE, PASSO A PASSO

Os tutoriais 02–06 existem exatamente para ensinar este boilerplate. O mecanismo das
macros está em `MIXR-CONTEXT.md` §4.3; aqui está **qual combinação usar em cada caso**.

## 4.1 PADRÃO: esqueleto mínimo (classe sem *slots*)

**Arquivos**: `tutorial02/Rng.hpp`, `tutorial02/Rng.cpp`.

```cpp
// Rng.hpp
#include "mixr/base/Object.hpp"
class Rng final: public mixr::base::Object
{
   DECLARE_SUBCLASS(Rng, mixr::base::Object)
public:
   Rng();
   double num();
   void setSeed(const unsigned int);
private:
   std::mt19937 engine;
   std::uniform_real_distribution<> dist;
};
```

```cpp
// Rng.cpp
IMPLEMENT_SUBCLASS(Rng, "Rng")     // "Rng" é o NOME DE FÁBRICA (string do EDL)
EMPTY_SLOTTABLE(Rng)               // esta classe não tem slots

Rng::Rng()
{
   STANDARD_CONSTRUCTOR()          // obrigatório: registra a instância no MetaObject
}

void Rng::copyData(const Rng& org, const bool)
{
   BaseClass::copyData(org);       // SEMPRE primeiro
}

void Rng::deleteData()
{
}
```

**REGRA** — as quatro peças obrigatórias de qualquer classe MIXR: `DECLARE_SUBCLASS` no
`.hpp`; `IMPLEMENT_SUBCLASS(Classe, "NomeDeFabrica")` no `.cpp`;
`STANDARD_CONSTRUCTOR()` como **primeira linha** do construtor;
`copyData()`+`deleteData()` definidos (ainda que vazios, via macro).

**REGRA** — `BaseClass` é o *typedef* que `DECLARE_SUBCLASS` cria para a classe-base;
`copyData()` da derivada **sempre** começa chamando `BaseClass::copyData(org)`.

**ARMADILHA** — o nome de fábrica **não é** o nome da classe. Coincidem em `"Rng"`, mas
divergem deliberadamente em vários exemplos: `mainSim3/Station.cpp` declara
`IMPLEMENT_SUBCLASS(Station, "MapTestStation")`; `mainSim3/MapPage.cpp` declara
`IMPLEMENT_SUBCLASS(MapPage, "MapTestMapPage")`; `testEvents/Display.cpp` declara
`IMPLEMENT_EMPTY_SLOTTABLE_SUBCLASS(Display, "SendDataDisplay")`;
`shared/xrecorder/DataRecorder.cpp` declara
`IMPLEMENT_SUBCLASS(DataRecorder, "XDataRecorder")`. Ver `MIXR-CONTEXT.md` §7.

**POR QUÊ** — a divergência é intencional quando a classe C++ tem nome genérico
(`Station`, `Display`, `MapPage`, `DataRecorder`) que colidiria, no arquivo EDL, com o
nome de fábrica de uma classe do framework. O nome de fábrica é um **espaço de nomes
global e plano**; o nome da classe C++ está protegido por *namespace*.

## 4.2 PADRÃO: adicionar uma *slot table*

**Arquivos**: `tutorial03/Rng.cpp` (1 slot), `tutorial05/MyObj.cpp` (6 slots de tipos
variados).

```cpp
// tutorial05/MyObj.cpp — a forma completa
IMPLEMENT_SUBCLASS(MyObj, "MyObj")

BEGIN_SLOTTABLE(MyObj)
   "colorTable",         // 1: The Color table     <PairStream>
   "textColor",          // 2: Text color          <Identifier>
   "backColor",          // 3: Background color    <Identifier>
   "vector",             // 4: Vector              <List>
   "visible",            // 5: Visibility flag     <Number>
   "message",            // 6: The message         <String>
END_SLOTTABLE(MyObj)

BEGIN_SLOT_MAP(MyObj)
   ON_SLOT(1, setSlotColorTable, mixr::base::PairStream)
   ON_SLOT(2, setSlotTextColor,  mixr::base::Identifier)
   ON_SLOT(3, setSlotBackColor,  mixr::base::Identifier)
   ON_SLOT(4, setSlotVector,     mixr::base::List)
   ON_SLOT(5, setSlotVisible,    mixr::base::Number)
   ON_SLOT(6, setSlotMessage,    mixr::base::String)
END_SLOT_MAP()
```

**REGRA** — o índice de `ON_SLOT` é a **posição na `BEGIN_SLOTTABLE`**, contando de 1.
Manter o comentário `// N: descrição <Tipo>` alinhado é convenção universal nos exemplos
e no framework — é a única documentação dos *slots*.

**PADRÃO** — os métodos `setSlotX` são **privados** e ficam num bloco `private:` separado
no final do `.hpp`, sob o comentário `// slot table helper methods`:

```cpp
// tutorial05/MyObj.hpp — final da classe
private:
   // slot table helper methods
   bool setSlotColorTable(const mixr::base::PairStream* const);
   bool setSlotTextColor(const mixr::base::Identifier* const);
   // ...
```

**PADRÃO** — o `setSlotX` é um **adaptador fino** que valida e delega ao `setX` público:

```cpp
bool MyObj::setSlotVisible(const mixr::base::Number* const x)
{
   bool ok{};
   if (x != nullptr) {
      ok = setVisible(x->getBoolean());
   }
   return ok;
}
```

**REGRA** — devolver `false` de um `setSlotX` faz o *parser* contar um erro
(`num_errors`), que o *builder* transforma em `exit(EXIT_FAILURE)` (§2.2). É assim que
validação de configuração vira falha de carga.

**PADRÃO** — **sobrecarga de `ON_SLOT` para o mesmo índice**, aceitando dois tipos:

```cpp
// tutorial07/Worm.cpp
BEGIN_SLOT_MAP(Worm)
   ON_SLOT(1, setSlotSpeed, mixr::base::Number)
   ON_SLOT(2, setSlotAngle, mixr::base::Angle)     // ( Degrees 30 )
   ON_SLOT(2, setSlotAngle, mixr::base::Number)    // 30   (número puro)
END_SLOT_MAP()
```

**POR QUÊ** — permite que o EDL escreva `startAngle: ( Degrees 30 )` **ou**
`startAngle: 0.5236`. O framework usa muito esse recurso para aceitar grandeza com
unidade ou valor cru. Ver `MIXR-CONTEXT.md` §9.

**PADRÃO** — o `setSlot` que precisa **guardar** o objeto de unidade original, para poder
reconverter num `reset()`:

```cpp
// tutorial07/Worm.cpp
bool Worm::setSlotAngle(const mixr::base::Angle* const saobj)
{
   bool ok{};
   if (saobj != nullptr) {
      mixr::base::Radians radians;
      setStartAngle(static_cast<double>(radians.convert(*saobj)));
      iangle = saobj;      // guarda
      iangle->ref();       // e referencia
      ok = true;
   }
   return ok;
}

void Worm::reset()
{
   BaseClass::reset();
   xPos = 0;  yPos = 0;  nTrails = 0;
   if (iangle != nullptr) {                        // reconverte a partir do original
      mixr::base::Radians radians;
      setStartAngle(static_cast<double>(radians.convert(*iangle)));
   }
}
```

O mesmo padrão aparece em `mainSim2/SimStation.cpp` com `autoResetTimer0`
(guarda o `base::Time` do slot para reinicializar `autoResetTimer` a cada `reset()`).

## 4.3 PADRÃO: classe abstrata + família de subclasses

**Arquivos**: `tutorial04/rngs/AbstractRng.{hpp,cpp}`, `Exponential`, `Lognormal`,
`Uniform`.

```cpp
// AbstractRng.cpp
IMPLEMENT_ABSTRACT_SUBCLASS(AbstractRng, "AbstractRng")
EMPTY_DELETEDATA(AbstractRng)
EMPTY_COPYDATA(AbstractRng)

BEGIN_SLOTTABLE(AbstractRng)
   "seed",
END_SLOTTABLE(AbstractRng)
BEGIN_SLOT_MAP(AbstractRng)
   ON_SLOT(1, setSlotSeed, mixr::base::Number)
END_SLOT_MAP()
```

```cpp
// Exponential.cpp
IMPLEMENT_SUBCLASS(Exponential,"Exponential")
EMPTY_DELETEDATA(Exponential)
BEGIN_SLOTTABLE(Exponential)
   "mean",
END_SLOTTABLE(Exponential)
BEGIN_SLOT_MAP(Exponential)
   ON_SLOT(1, setSlotMean, Number)     // índice 1 LOCAL; o global vira 2
END_SLOT_MAP()
```

**REGRA** — `IMPLEMENT_ABSTRACT_SUBCLASS` para classes com método virtual puro; a
abstrata **é registrada** com nome de fábrica próprio (`"AbstractRng"`) mas o `factory()`
da aplicação nunca a instancia:

```cpp
// tutorial04/main.cpp — só as concretas entram na fábrica
if      ( name == Exponential::getFactoryName() ) obj = new Exponential();
else if ( name == Lognormal::getFactoryName()   ) obj = new Lognormal();
else if ( name == Uniform::getFactoryName()     ) obj = new Uniform();
```

**REGRA** — o índice de `ON_SLOT` na derivada recomeça em 1 e o framework soma o número
de *slots* herdados (indexação cumulativa — ver `MIXR-CONTEXT.md` §4.3.6). No EDL, tanto
`seed:` (da base) quanto `mean:` (da derivada) valem para `( Exponential ... )`:

```
// tutorial04/tutorial04.edl
( Lognormal
    seed: 12
    mean: 10
    stddev: 2
)
```

**PADRÃO** — o *builder* aponta para o **tipo abstrato**, não para o concreto:
`AbstractRng* builder(...)` com `dynamic_cast<AbstractRng*>`. Trocar a distribuição é
editar o `.edl`, sem tocar em C++. Esta é a demonstração mais limpa, em toda a árvore de
exemplos, do valor da configuração declarativa.

**NESTA POC** — o mesmo desenho aparece implicitamente sempre que um `.epp` troca
`dynamicsModel: ( RacModel )` por `( JSBSimModel ... )` ou `( LaeroModel )` — todos são
concretos de `DynamicsModel`.

## 4.4 PADRÃO: primeira classe `Component`

**Arquivos**: `tutorial06/MyComp.{hpp,cpp}`.

```cpp
class MyComp final: public mixr::base::Component
{
   DECLARE_SUBCLASS(MyComp, mixr::base::Component)
public:
   MyComp();
   bool setStr(const mixr::base::String* const);
   const mixr::base::String* getStr() const;

   void reset() final;
   void updateTC(const double dt = 0.0) final;
   void updateData(const double dt = 0.0) final;
private:
   const mixr::base::String* str{};
   bool setSlotStr(const mixr::base::String* const);
};
```

```cpp
void MyComp::reset()
{
   setStr(nullptr);
   std::cout << "I've been reset!\n";
   BaseClass::reset();                 // propaga para os filhos
}

void MyComp::updateTC(const double dt)
{
   // ... trabalho de tempo crítico ...
   BaseClass::updateTC(dt);            // propaga
}

void MyComp::updateData(const double dt)
{
   // ... trabalho de fundo ...
   BaseClass::updateData(dt);          // propaga
}
```

**REGRA** — em `Component`, `reset()`, `updateTC()` e `updateData()` **devem** chamar a
versão da base, senão a subárvore de filhos para de ser percorrida. Ver
`MIXR-CONTEXT.md` §5.3/§5.5.

**PADRÃO** — a assinatura é sempre `(const double dt = 0.0)` com valor-padrão **na
declaração**, nunca na definição.

Um `Component` ganha automaticamente o *slot* `components:`, o que basta para montar
árvores em EDL sem escrever mais nada:

```
// tutorial06/tutorial06b.edl
( MyComp
    str: "top text"
    components: {
      p1: ( MyComp
              str: "p1 text"
              components: { z1: (MyComp str: "z1 text" ) }
          )
      p2: ( MyComp str: "p2 text" )
    }
)
```

E os *slots* de diagnóstico herdados de `Component` (§5.7 do documento irmão) já
funcionam:

```
// tutorial06/tutorial06a.edl
   enableTimingStats: true
   printTimingStats: true
```

## 4.5 PADRÃO: a tabela de macros `EMPTY_*` — qual usar quando

Levantamento das combinações efetivamente usadas nos exemplos:

| Macro | Substitui | Usar quando |
|---|---|---|
| `EMPTY_SLOTTABLE(C)` | `BEGIN/END_SLOTTABLE` + `BEGIN/END_SLOT_MAP` | a classe não acrescenta *slots* |
| `EMPTY_COPYDATA(C)` | `copyData()` | não há membro próprio a copiar |
| `EMPTY_DELETEDATA(C)` | `deleteData()` | não há recurso próprio a liberar |
| `EMPTY_CONSTRUCTOR(C)` | construtor | o construtor só faria `STANDARD_CONSTRUCTOR()` |
| `IMPLEMENT_EMPTY_SLOTTABLE_SUBCLASS(C,"n")` | `IMPLEMENT_SUBCLASS` + `EMPTY_SLOTTABLE` | atalho para os dois |
| `IMPLEMENT_ABSTRACT_SUBCLASS(C,"n")` | `IMPLEMENT_SUBCLASS` | a classe tem virtual puro |

Combinação máxima observada (`PriorityArbiter`, `TestStateMachine01`, `TestIoHandler`):

```cpp
IMPLEMENT_SUBCLASS(PriorityArbiter, "PriorityArbiter")
EMPTY_SLOTTABLE(PriorityArbiter)
EMPTY_CONSTRUCTOR(PriorityArbiter)
EMPTY_COPYDATA(PriorityArbiter)
EMPTY_DELETEDATA(PriorityArbiter)
```

— cinco linhas e a classe só implementa o método que interessa (`genComplexAction`).

## 4.6 PADRÃO: `copyData()`/`deleteData()` com membros que são objetos MIXR

**Arquivo**: `tutorial05/MyObj.cpp` — o gabarito completo.

```cpp
void MyObj::copyData(const MyObj& org, const bool)
{
   BaseClass::copyData(org);

   if (colorTable != nullptr) { colorTable->unref(); colorTable = nullptr; }
   if (org.colorTable != nullptr) colorTable = org.colorTable->clone();

   if (vector != nullptr) { vector->unref(); vector = nullptr; }
   if (org.vector != nullptr) vector = org.vector->clone();

   visible = org.visible;      // escalares: cópia direta
}

void MyObj::deleteData()
{
   setColorTable(nullptr);     // delega ao setter, que já faz o unref
   setVector(nullptr);
   setMessage(nullptr);
}
```

E o *setter* correspondente, com a ordem de `unref`/`ref` que evita autodestruição:

```cpp
bool MyObj::setColorTable(const mixr::base::PairStream* const x)
{
   if (colorTable != nullptr) colorTable->unref();
   colorTable = x;
   if (colorTable != nullptr) colorTable->ref();
   return true;
}
```

**REGRA** — três variantes legítimas para membro-objeto em `copyData()`, escolhidas por
semântica:

1. **`clone()`** — cópia profunda; o clone tem vida independente. Usado para dados de
   configuração (`MyObj`, `MyComp`, `Worm::iangle`, `DataRecordTest`).
2. **`ref()` sobre o mesmo ponteiro** — compartilha a instância. Usado para referências
   a objetos "de fora": `mainSim3/Station.cpp` (`display`), `mainSim3/MapPage.cpp`
   (`player[]`, `loader`, `stn`).
3. **anular** — a cópia recomeça do zero. Usado para ponteiros de conveniência
   descobertos em tempo de execução: `mainCockpit/TestDisplay.cpp` e
   `mainLaero/AdiDisplay.cpp` fazem `myStation = nullptr;` no `copyData()`.

**PADRÃO** — todo membro `SendData` (§8.1) é **esvaziado**, nunca copiado:

```cpp
   rangeSD.empty();
   headingSD.empty();
```

**PADRÃO** — o segundo parâmetro de `copyData()` (`const bool cc`, "copy constructor") só
é nomeado quando usado. Em `mainGndMapRdr/RealBeamRadar.cpp` ele decide se a memória de
imagem precisa ser alocada:

```cpp
void RealBeamRadar::copyData(const RealBeamRadar& org, const bool cc)
{
   BaseClass::copyData(org);
   if (cc) { initImageMemory(IMG_WIDTH, IMG_HEIGHT); }   // só no construtor de cópia
   // ...
}
```

Nos demais exemplos o parâmetro fica sem nome: `const bool)`.

## 4.7 PADRÃO: separar `setX` público de `setSlotX` privado

Observado sem exceção. `setX` recebe tipos C++ (`double`, `bool`, ponteiro MIXR já
validado) e é chamável do programa; `setSlotX` recebe **sempre** um ponteiro a objeto
MIXR (`const base::Number* const`, `const base::Distance* const`, …), converte e delega.

```cpp
// shared/xbehaviors/PlaneBehaviors.cpp — conversão de unidade dentro do setSlot
bool PlaneBehavior::setSlotCriticalAltitude(const base::Distance* const msg)
{
    bool ok{};
    if (msg != nullptr) {
       const double value{base::Meters::convertStatic( *msg )};   // Distance → metros
       criticalAltitude = value;
       ok = true;
    }
    return ok;
}
```

**REGRA** — a **conversão de unidade acontece no `setSlotX`**, uma única vez na carga; o
membro guarda o valor já na unidade interna (SI, quase sempre). Nunca guarde o objeto de
unidade para converter a cada quadro — exceto no caso do §4.2 (reconversão em `reset()`).

**PADRÃO** — validação de faixa também mora no `setSlotX`:

```cpp
// [ 1 .. 65535 ]
bool PlaneBehavior::setSlotVoteOnCriticalAltitude(const base::Number* const num)
{
   bool ok{};
   const int vote{num->getInt()};
   if (vote > 0 && vote <= 65535) { voteOnCriticalAltitude = ...; ok = true; }
   return ok;
}
```

**ARMADILHA** — esse mesmo trecho desreferencia `num` **sem checar `nullptr`**, ao
contrário de todos os outros `setSlot` da árvore. Erro real no código oficial.

## 4.8 ARMADILHA: *slot* de mesmo nome em base e derivada

**Arquivos**: `testSlots/{Base,Derived}.{hpp,cpp}`, `testSlots/file0.edl`. O exemplo
inteiro existe só para expor este comportamento.

```cpp
// Base.cpp
BEGIN_SLOTTABLE(Base)   "seed",   END_SLOTTABLE(Base)
BEGIN_SLOT_MAP(Base)    ON_SLOT(1, setSlotSeed, mixr::base::Number)   END_SLOT_MAP()
bool Base::setSlotSeed(...)    { std::cout << "Base seed being set\n";    return true; }

// Derived.cpp — MESMO nome de slot
BEGIN_SLOTTABLE(Derived)   "seed",   END_SLOTTABLE(Derived)
BEGIN_SLOT_MAP(Derived)    ON_SLOT(1, setSlotSeed, mixr::base::Number)  END_SLOT_MAP()
bool Derived::setSlotSeed(...) { std::cout << "Derived seed being set\n"; return true; }
```

```
// file0.edl
( Derived
    seed: 50
)
```

Como os índices são cumulativos, `Derived` tem **dois** *slots* chamados `"seed"` (índice
global 1, da base; índice global 2, da derivada). O `setSlotSeed` da derivada é quem
atende, e o da base **nunca é chamado** por esse arquivo — o *slot* da base fica
inalcançável pelo nome.

**REGRA** — **não reutilize um nome de *slot* já usado por uma classe-base.** Se
precisar do valor nos dois níveis, chame explicitamente o `setSlotSeed` da base a partir
do da derivada, ou dê nomes distintos.

## 4.9 PADRÃO: `MetaObject` para diagnóstico de instâncias

**Arquivo**: `testMetaObject/main.cpp`.

```cpp
void printMetadata(const mixr::base::MetaObject* metaObject)
{
   std::cout << "class name   : " << metaObject->getClassName()   << std::endl
             << "factory name : " << metaObject->getFactoryName() << std::endl
             << "count        : " << metaObject->count            << std::endl   // vivas
             << "mc           : " << metaObject->mc               << std::endl   // máximo
             << "tc           : " << metaObject->tc               << std::endl;  // total
}

const auto c1 = new mixr::base::Color();
const auto c2 = new mixr::base::Color();
const auto c3 = new mixr::base::Color();
c2->unref();
printMetadata(mixr::base::Color::getMetaObject());   // count=2, mc=3, tc=3
```

**PADRÃO** — `Classe::getMetaObject()` é estático e existe em toda classe MIXR. Os três
contadores (`count` vivas, `mc` máximo concorrente, `tc` total criadas) são a ferramenta
nativa para **caçar vazamento de referência**: se `count` cresce quadro a quadro, algum
`ref()` não tem `unref()`.

**NESTA POC** — não usado ainda; é o instrumento indicado caso alguma poc passe a criar
objetos por quadro (ex.: mensagens de evento em `poc/08-event-relay`, ou clones de
`Chaff`/`Flare` em `poc/09`).

---

# 5. PADRÕES DE ARQUIVO DE CONFIGURAÇÃO (`.epp` → `.edl`)

## 5.1 PADRÃO: a *pipeline* de duas etapas

**REGRA** — o `edl_parser` **não** entende `#include`, `#define` nem `#if`. Um arquivo
`.epp` (*EDL PreProcessor source*) passa por um **preprocessador C de verdade** e vira um
`.edl`, que é o que o *parser* lê.

```makefile
# mainCockpit/Makefile
edl:
	cpp configs/test1.epp >test1.edl $(EPPFLAGS)
```

```makefile
# mainSim1/Makefile — vários cenários de uma vez
edl:
	cpp configs/test0.epp > test0.edl $(EPPFLAGS)
	cpp configs/test1.epp > test1.edl $(EPPFLAGS)
	cpp configs/test2.epp > test2.edl $(EPPFLAGS)
	cpp configs/test3.epp > test3.edl $(EPPFLAGS)
	cpp configs/test4.epp > test4.edl $(EPPFLAGS)
```

No Windows, o equivalente é `make-edl.cmd` por diretório, orquestrado pelo
`examples/make-edl.cmd` da raiz; no Linux, `make edl` a partir de `examples/Makefile`.

**PADRÃO** — o `.edl` gerado é **artefato de build**, listado em `clean:`
(`-rm -f *.edl`), nunca versionado — exceto nos exemplos que **não** usam preprocessador
e escrevem `.edl` à mão: `testStateMach/test1..4.edl`, `testTables/test*.edl`,
`testSlots/file0.edl`, `testTimer/test01.edl`, `tutorial03..06/*.edl`,
`tutorial07/file0.edl`, `testNetHandler/configs/*.edl`.

**REGRA** — se o arquivo de configuração não usa nenhuma diretiva de preprocessador,
escreva `.edl` direto e pule a etapa. É o que fazem 7 exemplos.

**NESTA POC** — `poc/01`, `03`, `04` usam `.epp` sem diretivas (poderiam se chamar
`.edl`). `poc/05` e `poc/06` usam `#include` de verdade e por isso rodam
`g++ -E -x c -P -undef -nostdinc arquivo.epp -o arquivo.preprocessed.epp` dentro do
próprio `main.cpp` antes de chamar `edl_parser` — a mesma etapa dos exemplos, só que
executada em tempo de execução em vez de em tempo de build. Os *flags* `-P -undef
-nostdinc` cumprem o papel dos `EPPFLAGS` (a seguir).

## 5.2 PADRÃO: `EPPFLAGS` — como o preprocessador injeta caminhos de dados

```makefile
# examples/makedefs — na íntegra da parte de EPP
EPPFLAGS = \
	-I$(MIXR_DATA_ROOT) \
	-I$(MIXR_ROOT)/../mixr-examples/shared \
	-I$(MIXR_ROOT)/include \
	-DMIXR_DATA_PATH=\"$(MIXR_DATA_ROOT)/\" \
	-DMIXR_DATA_FONT_PATH=\"$(MIXR_DATA_ROOT)/fonts/\" \
	-DMIXR_DATA_FONT_11_19_PATH=\"$(MIXR_DATA_ROOT)/fonts/11x19/normal/\" \
	-DMIXR_DATA_TEXTURES_PATH=\"$(MIXR_DATA_ROOT)/textures/\" \
	-DMIXR_DATA_JSBSIM_PATH=\"$(MIXR_DATA_ROOT)/JSBSim/\" \
	-DMIXR_DATA_VMAP0_PATH=\"$(MIXR_DATA_ROOT)/vmap0/v0noa_5/vmaplv0/\" \
	-DMIXR_DATA_VISUALDB_PATH=\"$(MIXR_DATA_ROOT)/visualdb/portland/\" \
	-DMIXR_DATA_RECORDER_FILE=\"$(MIXR_DATA_ROOT)/recorder/zFileWriter.ebd\"
```

E o uso, dentro do `.epp`:

```
dynamicsModel: ( JSBSimModel
   rootDir: MIXR_DATA_JSBSIM_PATH
   model: "f16"
)
```

**PADRÃO** — **nenhum caminho absoluto aparece nos `.epp`**. Todos vêm de macros
definidas no `EPPFLAGS`, derivadas de `MIXR_DATA_ROOT` (definida em `setenv.sh`). O
arquivo de cenário fica portável entre máquinas.

**REGRA** — as macros expandem para *string literals* já com aspas (`\"...\"`), então no
`.epp` elas aparecem **sem** aspas. Escrever `rootDir: "MIXR_DATA_JSBSIM_PATH"` produz a
string literal `"MIXR_DATA_JSBSIM_PATH"`, não o caminho.

**PADRÃO** — `-I` inclui `$(MIXR_ROOT)/include`, o que permite `#include` de cabeçalhos
**do próprio framework** dentro do `.epp`:

```
// testRecordData/configs/dataRecorder.epp — primeira linha
#include "mixr/simulation/dataRecorderTokens.hpp"
```

— e assim o `.epp` passa a poder escrever `messageToken: REID_PLAYER_DATA` usando a mesma
constante que o C++ usa. **Este é o mecanismo mais elegante da configuração MIXR**: um
único cabeçalho é a fonte de verdade para C++ e para EDL.

Cabeçalhos escritos para esse uso duplo são explicitamente marcados:

```cpp
// mainSim2/configs/linkage/channel_map.hpp
//    2) This file defines channels for both the C++ code and the MIXR
//       input files, so use only C pre-processor directives only.
```

```cpp
// shared/xrecorder/dataRecorderTokens.hpp
//    1) This file is used by both C++ and mixr input files, so only use
//       C pre-processor directives in this configuration file.
```

**NESTA POC** — `poc/05` usa a mesma ideia por outro caminho: `configs/scenario.epp.in`
com substituição de *template* feita em C++ (`generateScenario()`), porque o valor a
injetar (`numTcThreads`) é calculado em tempo de execução, não em tempo de build.

## 5.3 PADRÃO: modularização por `#include` — o esqueleto que emerge

O cenário de uma aplicação completa **nunca** é um arquivo único. A decomposição
recorrente:

```
configs/
├── test1.epp              ← ponto de entrada: só a Station e os #include
├── scenario.epp           ← ownship: + simulation: ( WorldModel players: {...} )
├── player01.epp           ← um player por arquivo
├── player02.epp
├── player03.epp
├── gainPattern.epp        ← peça reaproveitada por vários players
├── dataRecorder.epp       ← subsistema opcional
├── interop/dis.epp        ← rede
├── ighosts/cigi.epp       ← image generator
└── linkage/
    ├── channel_map.hpp    ← constantes compartilhadas C++/EDL
    ├── saitekST290.epp    ← um arquivo por modelo de joystick
    ├── saitekEVO.epp
    ├── thrustmaster.epp
    └── warthog.epp
```

```
// mainCockpit/configs/test1.epp — o ponto de entrada INTEIRO
#define REF_LATITUDE      37.0
#define REF_LONGITUDE   -116.0
#define IG_LATITUDE     REF_LATITUDE
#define IG_LONGITUDE    REF_LONGITUDE

( SimStation
   tcPriority: 1

   ioHandler: ( TestIoHandler
      inputData: ( IoData numAI: 10   numDI: 40 )
      devices: {
         #include "linkage/saitekST290.epp"
         //#include "linkage/saitekEVO.epp"
         //#include "linkage/thrustmaster.epp"
         //#include "linkage/warthog.epp"
      }
   )

   igHosts:  { #include "ighosts/cigi.epp" }
   networks: { #include "interop/dis.epp"  }

   #include "scenario.epp"
   #include "xpanel/xpanel.epp"
)
```

**PADRÃO** — **alternativas ficam no arquivo, comentadas.** Trocar de joystick, de IG ou
de terreno é descomentar uma linha. Aparece em `mainCockpit` (4 joysticks),
`mainSim1`/`mainSim2` (`pov.epp` vs `cigi.epp`), `mainGndMapRdr` (`dted.epp` vs
`srtm.epp`), `mainLaero` (`LaeroModel` vs `RacModel` vs `JSBSimModel` via `#if`).

**PADRÃO** — o ponto de entrada define as macros de posição de referência
(`REF_LATITUDE`/`REF_LONGITUDE`) que os arquivos incluídos consomem. Isso mantém a
origem do "gaming area" num só lugar:

```
// mainCockpit/configs/scenario.epp
simulation: ( WorldModel
   latitude:  REF_LATITUDE
   longitude: REF_LONGITUDE
   players: { #include "player01.epp"  #include "player02.epp"  #include "player03.epp" }
)
```

## 5.4 REGRA: quem coloca o rótulo, o arquivo incluído ou o local do `#include`?

Esta é a fonte de erro de sintaxe nº 1 ao modularizar. Há **duas convenções válidas** e
elas são mutuamente exclusivas:

**Convenção 1 — o arquivo incluído carrega o rótulo; o `#include` é solto.**

```
// mainSim2/configs/instrumentPanel.epp — começa COM rótulo
display:
( InstrumentPanel
    idleSleepTime: 20
    ...
```
```
// uso:
( SimStation
   ...
   #include "instrumentPanel.epp"      // sem rótulo aqui
)
```

Mesma convenção em: `mainGndMapRdr/configs/displays.epp` (`display:`),
`testRadar/configs/gainPattern.epp` (`gainPattern:`),
`testRecordData/configs/dataRecorder.epp` (`dataRecorder:`),
`mainGndMapRdr/configs/srtm.epp` e `dted.epp` (`terrain:`),
`testInfrared/configs/irAtmosphere.epp` (`atmosphere:`),
`testInfrared/configs/irSignature.epp` (`irSignature:`),
`shared/xpanel/xpanel.epp` (`display:`),
`mainSim1/configs/test2/route01.epp` (`route:`),
`mainSim1/configs/test2/player01.epp` (`p01:`).

Caso especial: **um arquivo incluído pode entregar mais de um *slot* de uma vez.**

```
// mainCockpit/configs/scenario.epp — entrega DOIS slots da Station
ownship: p01
simulation: ( WorldModel ... )
```

**Convenção 2 — o `#include` carrega o rótulo; o arquivo começa direto no `(`.**

```
// mainLaero/configs/test.epp
( TestStation
   glutDisplay:
   #include "adidisplay.epp"           // rótulo aqui
   mapDisplay:
   #include "mapdisplay.epp"
   ...
```
```
// mainLaero/configs/adidisplay.epp — começa SEM rótulo
( AdiDisplay
      name: "Cockpit Display"
      ...
```

Mesma convenção em: `testRadar`/`testInfrared` (`glutDisplay: #include "displays.epp"`,
e `displays.epp` começa em `( TestDisplay`), `mainSim1`/`mainCockpit`
(`igHosts: { #include "ighosts/cigi.epp" }`, e `cigi.epp` começa em `( CigiHost`).

**ARMADILHA** — combinar as duas produz `dataRecorder: dataRecorder: ( ... )`, que é
**erro de sintaxe**. Foi exatamente o erro encontrado ao construir `poc/05-formation-flight`
(registrado no `CLAUDE.md` deste repositório).

**REGRA** — dentro de uma lista `{ }` que é um `PairStream` **nomeado** (`players:`,
`stores:`, `pages:`, `fonts:`, `colorTable:`), cada elemento incluído leva rótulo
(`p01: ( Aircraft ... )`). Dentro de uma lista `{ }` cujos elementos são **anônimos**
(`igHosts:`, `networks:`, `devices:`, `adapters:`, `generators:`, `components:` de
sensor), o elemento incluído começa direto no `(`.

## 5.5 PADRÃO: macro de função para peça repetida

**Arquivos**: `mainUbf1/configs/players/parts/*.epp`.

```
// parts/dynamics.epp
#define JSBSIM_DYNAMICS(MODEL)      \
( JSBSimModel                       \
   rootDir: MIXR_DATA_JSBSIM_PATH   \
   model: MODEL                     \
)

#define LAERO_DYNAMICS()         \
( LaeroModel )
```

```
// parts/missiles.epp
#define AAM_MISSILE(ID)                               \
( AamMissile                                          \
   id: ID                                             \
   type: "AIM-120C"                                   \
   signature: ( SigDihedralCR a: 0.1 )                \
   mode: inactive                                     \
   minSpeed: 0     maxSpeed: 800     speedMaxG: 800   \
   maxg: 9         maxAccel: 200     maxBurstRng: 50  \
   maxTOF: 30      eobt: 90          tsg: 0           \
)
```

```
// players/player01.epp — o uso
#include "parts/dynamics.epp"
#include "parts/agents.epp"
#include "parts/patterns.epp"
#include "parts/missiles.epp"

p01: ( Aircraft
   ...
   components: {
      dynamicsModel: JSBSIM_DYNAMICS("f16")
      //dynamicsModel: LAERO_DYNAMICS()
      ubf1: UBF_AGENT1()
      ...
      sms: ( StoresMgr
         numStations: 4
         stores: {
            1: AAM_MISSILE(201)
            2: AAM_MISSILE(202)
            3: AAM_MISSILE(203)
            4: AAM_MISSILE(204)
         }
      )
   }
)
```

**POR QUÊ** — compare com `mainCockpit/configs/player01.epp` e
`testRadar/configs/test1.epp`, que escrevem os **mesmos** quatro mísseis por extenso, 14
linhas cada, 56 linhas de repetição literal. `mainUbf1` reduz a quatro linhas. É o
padrão mais maduro de configuração em toda a árvore de exemplos.

**REGRA** — a macro de função só serve para peças **sem rótulo** (o rótulo fica no local
de uso: `1: AAM_MISSILE(201)`). Toda linha da macro precisa da barra invertida final; a
última, não.

**PADRÃO** — as tabelas de ganho de antena também viram macro, por serem grandes e
reaproveitadas:

```
// parts/patterns.epp
#define ANTENNA_PATTERN()                                                   \
( Func1                                                                     \
   table: ( Table1                                                          \
      x:    [ 0.0  0.01745 0.02618 0.04363 0.05236 0.061087 0.06981 0.07854 ] \
      data: [ 0.0 -3.0    -10.0   -30.0   -20.0   -14.0    -25.0   -80.0   ] \
   )                                                                        \
)
```
```
   radar: ( Antenna
      gain: ( dB 42 )
      gainPattern: ANTENNA_PATTERN()
      ...
```

Contraste com a Convenção 1 usada por `testRadar`/`mainCockpit`, onde o mesmo conteúdo
é um arquivo `gainPattern.epp` já rotulado, incluído sem rótulo. **As duas formas
coexistem na árvore oficial**; a macro é preferível quando há mais de um padrão de ganho
no mesmo cenário (`ANTENNA_PATTERN` + `JAMMER_PATTERN`).

## 5.6 PADRÃO: variantes por `#define`/`#if`

```
// mainSim2/configs/screenSetup.epp — arquivo inteiro é só configuração de macro
//#define D1920x1200    /* 16x10 1920x1200 */
//#define D1920x1080    /* 16x9  1920x1080 */
//#define D1024x768H    /* 16x9  1024x768  */
#define FULL_SCREEN_FLAG    0
```

```
// testRadar/configs/test1.epp
#define PFRZ false
...
   initVelocity: 250
   positionFreeze: PFRZ        // um #define governa todos os players
```

```
// mainLaero/configs/test.epp — escolha de modelo de dinâmica
   #if 1
      dynamicsModel: ( LaeroModel )
   #else
      dynamicsModel: ( JSBSimModel )
   #endif
```

```
// testRecordData/configs/dataRecorder.epp — bloco desligado
#if 0
   ( RecorderNetOutput netHandler: ( UdpUnicastHandler ... ) )
#endif
```

**PADRÃO** — `#if 0` / `#if 1` é o mecanismo padrão para manter blocos alternativos
inteiros no arquivo. Mais legível que comentar linha a linha quando o bloco é grande.

## 5.7 PADRÃO: quando dar rótulo a um objeto no EDL

Regra empírica, derivada do uso:

| Dê rótulo quando… | Exemplo |
|---|---|
| alguém vai procurar o objeto pelo nome em C++ (`findByName`) | `boolean:`, `integer:` em `testEvents/configs/test.epp` (alvos de `send()`) |
| outro *slot* do EDL referencia o nome | `radar: ( Antenna ...)` ← `antennaName: radar`; `twsTrkMgr: ( AirTrkMgr ...)` ← `trackManagerName: twsTrkMgr` |
| o objeto é um *player* (o `ownship:` referencia pelo nome) | `p01: ( Aircraft ... )` ← `ownship: p01` |
| a posição na lista tem significado | `1: ( AamMissile ...)` em `stores:` (estação de armamento); `1: ( Polygon ...)` em `templates:` de `mainPuzzle1` (índice = tipo de bloco) |
| **não** dê rótulo | elementos de `components:` que ninguém procura; `( Rwr ... )` em `sensors:`; `( Steerpoint ... )` em `route:` |

```
// testRadar/configs/test1.epp — a cadeia de nomes, condensada
antennas: ( Gimbal components: {
      radar: ( Antenna ... )          // ← rotulado porque é referenciado
      rwr:   ( Antenna ... )
} )
sensors: ( SensorMgr components: {
      ( Tws  trackManagerName: twsTrkMgr   antennaName: radar  ... )   // ← anônimo
      ( Rwr  antennaName: rwr  ... )                                    // ← anônimo
} )
obc: ( OnboardComputer components: {
      twsTrkMgr: ( AirTrkMgr ... )    // ← rotulado porque é referenciado
} )
```

**ARMADILHA** — `mainPuzzle1/configs/puzzle.epp` documenta no comentário que a posição em
`templates:` **precisa** bater com o ID do tipo de bloco:
`// (positions in list (slot numbers) MUST match block type IDs)`. Rótulos numéricos são
*slots* posicionais e carregam semântica — ver `MIXR-CONTEXT.md` §6.3.

---

# 6. PADRÕES DE COMPOSIÇÃO DE CENÁRIO

## 6.1 PADRÃO: o esqueleto da `Station`

Todos os cenários com `Station` têm a mesma ossatura, com os *slots* sempre na mesma
ordem:

```
( Station                            // ou subclasse: SimStation, TestStation, ...

   tcPriority: 1                     // [0,1]; alguns usam 0.5 ou 0.0
   tcRate: 100                       // opcional (default 50 Hz) — só testInfrared usa
   netPriority: 1.0                  // só quando há networks
   netRate: 5.0
   bgRate: 10                        // só testRadar

   ioHandler: ( ... )                // E/S com hardware       — §9
   igHosts:  { ... }                 // image generators       — §6.7
   networks: { ... }                 // DIS/HLA                — §6.7
   dataRecorder: ( ... )             // gravação               — §12

   ownship: <nome do player>
   simulation: ( WorldModel ... )    // o cenário propriamente dito
)
```

**REGRA** — `ownship:` recebe o **nome (rótulo) de um player** declarado em
`simulation.players`. Sem ele, `Station::getOwnship()` devolve `nullptr` e todo o código
de controles & displays vira *no-op*.

**PADRÃO** — a decomposição típica é: o `.epp` de entrada contém a `Station` e os
`#include`; `scenario.epp` contém `ownship:` + `simulation:`; um arquivo por *player*.
Ver §5.3.

**ARMADILHA** — `tcPriority` aparece com valores díspares nos exemplos oficiais: `0.0`
(`mainSim2`), `0.5` (`mainSim1/test0`, `testInfrared`), `1` (`mainCockpit`, `testRadar`,
`mainUbf1`, `mainSim1/test2..4`). Não há convenção; é ajuste empírico por máquina.

## 6.2 PADRÃO: `WorldModel` — a área de jogo

```
simulation: ( WorldModel

   freeze: false                     // condição inicial de congelamento

   // posição de referência: TODAS as posições relativas dos players partem daqui
   latitude:  REF_LATITUDE
   longitude: REF_LONGITUDE

   //gamingAreaRange: ( KiloMeters 200.0 )
   //simulationTime: ( Seconds ( + ( * 13 3600 ) ( * 29 60 ) 0 ) )
   //day: 13   month: 11   year: 1973

   //enableTimingStats: true
   //printTimingStats: true

   terrain: ( QuadMap ... )          // opcional — §6.6
   atmosphere: ( IrAtmosphere ... )  // opcional — §6.8

   players: { ... }
)
```

**PADRÃO** — `latitude`/`longitude` do `WorldModel` são a **origem do referencial local
NED**. Um *player* pode então ser posicionado de três formas, todas presentes nos
exemplos:

```
// (a) offset cartesiano em relação à referência (o mais comum em testes)
initXPos: ( NauticalMiles 0 )     initYPos: ( NauticalMiles 0 )     initAlt: ( Feet 20000 )

// (b) vetor NED de uma vez, em metros
initPosition: [ 10000 15000 -10000 ]        // note o Z NEGATIVO = para cima

// (c) coordenadas geodésicas absolutas
initLatitude:  ( LatLon direction: "n" degrees: 36  minutes: 50.0 )
initLongitude: ( LatLon direction: "w" degrees: 116 minutes:  0.0 )
initAlt: ( Feet 15000 )
```

**ARMADILHA** — em `initPosition: [ x y z ]` o eixo Z é **NED** (positivo para baixo):
`-10000` significa 10 000 m **de altitude**. É a convenção de `testRadar`,
`testInfrared`, `mainSim1/test3`. Já `initAlt:` é altitude positiva para cima. Misturar
as duas no mesmo cenário confunde.

**PADRÃO** — o comentário `// Reference position (player positions are relative to this
location)` aparece literalmente em 6 arquivos de cenário. Mantenha-o.

**PADRÃO** — `mainSim1/configs/test3/` é o "banco de estresse": ~72 *players* declarados
por extenso num único `players.epp`, todos `Aircraft` com `RacModel` implícito, para
medir escalabilidade. `testRadar` tem a mesma ideia com `test2a/2b/2c` (48/72/96
*players*, cada um com radar **e** RWR — teste N²).

## 6.3 PADRÃO: a árvore de `components:` de um *player*

Esta é a estrutura mais importante do documento para quem vai montar cenários. Ordem
canônica dos subsistemas dentro de `components:` de um `Aircraft`:

```
p01: ( Aircraft
    id: 1                                   // ID de entidade (DIS)
    side: blue                              // blue | red | yellow | cyan | gray | white
    type: "F-16"                            // string livre; usada pelo mapeamento IG/DIS
    signature: ( SigSphere radius: 2.0 )    // RCS — sem isto o player é invisível ao radar
    camouflageType: 1                       // opcional; usado pelo IG

    initLatitude: ...  initLongitude: ...  initAlt: ...
    initHeading: ( Degrees 0.0 )
    initRoll: ( Degrees 0 )   initPitch: ( Degrees 0 )
    initVelocity: 300

    positionFreeze: false
    killRemoval: true
    fuelFreeze: true
    dataLogTime: ( Seconds 0.1 )            // taxa de gravação deste player — §12.1

    components: {

      dynamicsModel: ( JSBSimModel rootDir: ... model: "F4N" )   // ou RacModel/LaeroModel

      pilot: ( Autopilot navMode: false altitudeHoldMode: false
                         headingHoldMode: false velocityHoldMode: false )

      nav: ( Navigation utc: ( Seconds 0 )  #include "route01.epp" )

      antennas: ( Gimbal components: {
            radar:  ( Antenna ... )
            rwr:    ( Antenna scanMode: manual ... )
            jammer: ( Antenna ... )
      } )

      sensors: ( SensorMgr components: {
            ( Tws     trackManagerName: twsTrkMgr  antennaName: radar  ... )
            ( Rwr     trackManagerName: rwrTrkMgr  antennaName: rwr    ... )
            ( Jammer  antennaName: jammer ... )
      } )

      obc: ( OnboardComputer components: {
            twsTrkMgr: ( AirTrkMgr  ... )
            rwrTrkMgr: ( RwrTrkMgr  ... )
      } )

      sms: ( StoresMgr numStations: 4 stores: { 1: (AamMissile ...) ... } )
    }
)
```

**REGRA — a cadeia de nomes.** Três *slots* fazem a ligação por **string**, e é onde os
cenários mais quebram silenciosamente:

| *Slot* | Aponta para | Onde o alvo é declarado |
|---|---|---|
| `antennaName:` (no sensor) | rótulo de uma `Antenna` | dentro de `antennas: ( Gimbal components: {...} )` |
| `trackManagerName:` (no sensor) | rótulo de um `TrackManager` | dentro de `obc: ( OnboardComputer components: {...} )` |
| `leadPlayerName:` (no `Autopilot`) | rótulo de um *player* | na lista `players:` do `WorldModel` |

**ARMADILHA** — se o nome não resolver, **não há erro de parse**: o sensor simplesmente
nunca transmite e a pista nunca aparece. Foi a mesma classe de falha silenciosa
encontrada em `poc/08-event-relay` com `Component::send()` (§8.1).

**PADRÃO** — `Antenna` **já é** um `ScanGimbal`. Não é preciso um `ScanGimbal` separado:
os *slots* `searchVolume:`, `numBars:`, `scanMode:`, `commandRateAzimuth:` etc. ficam
direto na `Antenna`.

```
radar: ( Antenna
      polarization: horizontal
      gain: ( dB 42 )
      #include "gainPattern.epp"
      initPosition: [ 0 0 ]                       // az, el iniciais (rad)
      maxRates: [ 0.8727 0.8727 ]                 // rad/s
      commandRates: [ 0.43633 0.6981 ]
      reference: [ 0 0 ]                          // centro do volume de busca
      searchVolume: [ 1.0472 0.05 ]               // 60° az, ~3° el (rad)
      numBars: 2

      maxPlayersOfInterest: 20
      playerOfInterestTypes: { air ground weapon ship building lifeform space }
      maxRange2PlayersOfInterest: ( KiloMeters 100.0 )
      maxAngle2PlayersOfInterest: ( Degrees 90.0 )
      localPlayersOfInterestOnly: false
      useWorldCoordinates: false
)
```

**PADRÃO** — o bloco `*PlayersOfInterest*` é o **filtro de custo**: limita quantos
*players* entram na equação do radar por quadro. `localPlayersOfInterestOnly: true`
descarta entidades vindas da rede. Em `mainSim1/configs/test3/networkSetup.epp` esse
*flag* aparece em **todos** os `EmissionPduHandler`, justamente no cenário de estresse.

**PADRÃO** — o alvo só precisa de `signature:` para ser detectável. Não precisa de
sensor:

```
// testRadar/configs/test1.epp — um alvo típico
p00: ( Aircraft
   side: red   type: "F-16"   id: 1000
   signature: ( SigSphere radius: 2.0 )
   initPosition: [ 10000 15000 -10000 ]
   initHeading: ( Degrees 220 )   initVelocity: 80
   killRemoval: true
   components: {
      rwr: ( Antenna scanMode: manual polarization: horizontal gain: ( dB 12 ) )
      ( Rwr antennaName: rwr threshold: ( dB 0.0 )
            frequency: ( GigaHertz 3.0 ) bandwidth: ( GigaHertz 2.0 ) )
   }
)
```

Aqui o RWR existe só para o alvo **saber** que está sendo iluminado.

**NESTA POC** — `poc/06-radar-detection` e `poc/07-radar-intercept` reproduzem exatamente
este esqueleto (`antennas`/`sensors`/`obc`), com o `Tws` no lugar do `Radar` genérico.
`poc/09-chaff-flare` reaproveita o `sms: ( StoresMgr stores: {...} )` trocando
`AamMissile` por `Chaff`/`Flare`.

**PADRÃO** — `GroundStation` para radares fixos (`mainSim1/configs/test4/spinner.epp`):
`initVelocity: 0`, `initAlt: ( Feet 0 )`, antena com `scanMode: circular` e
`commandRateAzimuth: ( Degrees 36 )` — 10 s por volta.

## 6.4 PADRÃO: dinâmica de voo — três opções

```
dynamicsModel: ( RacModel )                                    // cinemático simples
dynamicsModel: ( LaeroModel )                                  // aero linearizado
dynamicsModel: ( JSBSimModel rootDir: MIXR_DATA_JSBSIM_PATH model: "f16" debugLevel: 0 )
```

**PADRÃO** — quando **não há** `dynamicsModel:`, o *player* usa a integração de posição
nativa do `Player` a partir de `initVelocity`/`initHeading` (movimento retilíneo
uniforme). É o caso dos ~72 alvos de `mainSim1/test3` e dos 3 alvos de `testRadar/test1`.

**PADRÃO** — `mainSim1/configs/test1.epp` mostra o modo de teste puro, sem dinâmica
nenhuma, forçando taxas:

```
xper: ( Helicopter
        //testRollRate: ( Degrees 0.0 )   // Test roll rate (units per second)
        //testPitchRate: ( Degrees 0.0 )
        testYawRate: ( Degrees 5.0 )      // Test heading rate
        //testBodyAxis: false             // taxas no corpo, senão Euler (default: false)
      )
```

Útil para exercitar sensores contra um alvo de trajetória perfeitamente previsível.

**ARMADILHA** — nenhum exemplo da árvore oficial **estende** `DynamicsModel`. Os três
modelos existentes são do framework. Se você for criar física própria (o objetivo
declarado desta PoC), **não há gabarito oficial**; use `mainGndMapRdr/RealBeamRadar` como
modelo de "estender um `System`" (§7.4) e `RacModel.cpp` do framework como modelo de
`DynamicsModel`.

## 6.5 PADRÃO: navegação por rota

```
// mainSim1/configs/test2/player01.epp
      pilot: ( Autopilot
         navMode: true
         altitudeHoldMode: true
         headingHoldMode: true
         velocityHoldMode: false
      )
      nav:( Navigation
         utc: ( Seconds 0 )
         #include "route01.epp"
      )
```

```
// mainSim1/configs/test2/route01.epp
route:
( Route
   autoSequence: true          // avança sozinho ao atingir o steerpoint
   wrap: true                  // volta ao primeiro depois do último
   components: {
      ( Steerpoint
         latitude:  ( LatLon direction: "n" degrees: 37  minutes: 20.0 )
         longitude: ( LatLon direction: "w" degrees: 116 minutes: 20.0 )
         altitude: ( Feet 12000 )
         airspeed: 525
      )
      // ... mais 3 steerpoints
   }
)
```

Variante em `mainLaero/configs/test.epp`, com `to: 1` (steerpoint inicial) e o `Route`
aninhado direto no `Navigation` sem `#include`:

```
navigation: ( Navigation
   route: ( Route
      to: 1
      components: {
         ( Steerpoint latitude: 0.1  longitude: 0.1  altitude: ( Feet 12000 ) airspeed: 300 )
         ...
      }
   )
)
```

**REGRA** — `Autopilot.navMode: true` é o que faz o piloto **seguir** a rota. Sem ele o
`Route` existe mas ninguém o consome.

**PADRÃO** — o *slot* do `Autopilot` no *player* é `pilot:` em `mainSim1`, `mainCockpit`,
`mainUbf1`, `mainGndMapRdr` e `mainLaero`; o do `Navigation` é `nav:` em `mainSim1` e
`navigation:` em `mainLaero`. **O rótulo é livre** — quem identifica o subsistema é o
tipo, não o nome. `Player::getPilotByType(typeid(Autopilot))` acha pelo tipo (§8.4).

**NESTA POC** — `poc/05-formation-flight` usa exatamente esse par
(`Route`/`Steerpoint` + `Autopilot::flyCRS`/`navMode`) para o RTB do líder, e
`followTheLeadMode` para os *wingmen* — modo que aparece comentado em
`mainLaero/configs/test.epp` (`//followTheLeadMode: 0`).

## 6.6 PADRÃO: terreno como árvore de `QuadMap`

```
// mainGndMapRdr/configs/srtm.epp
terrain:
( QuadMap
    path: MIXR_DATA_PATH
    components: {
        ( QuadMap
            components: {
                ( SrtmHgtFile file: "srtm3/N39W115.hgt" )
                ( SrtmHgtFile file: "srtm3/N39W114.hgt" )
                ( SrtmHgtFile file: "srtm3/N38W115.hgt" )
                ( SrtmHgtFile file: "srtm3/N38W114.hgt" )
            }
        )
    }
)
```

```
// mainGndMapRdr/configs/dted.epp — DTED e DED misturados, dois níveis de QuadMap
terrain: ( QuadMap  path: MIXR_DATA_PATH  components: {
    ( QuadMap components: {
        ( QuadMap components: {
            ( DtedFile file: "dted/W119/N37.DT1" )  ( DtedFile file: "dted/W120/N37.DT1" )
            ( DtedFile file: "dted/W119/N36.DT1" )  ( DtedFile file: "dted/W120/N36.DT1" )
        } )
        ( QuadMap components: {
            ( DedFile file: "ded/W117_N37.ded" )    ( DedFile file: "ded/W118_N37.ded" )
            ( DedFile file: "ded/W117_N36.ded" )    ( DedFile file: "ded/W118_N36.ded" )
        } )
    } )
} )
```

**REGRA** — `QuadMap` agrega **quatro** filhos em grade 2×2 e pode aninhar outros
`QuadMap`, formando um quadtree. `path:` é herdado pelos filhos, então cada arquivo
declara só o caminho relativo. Ver `MIXR-CONTEXT.md` §22.4.

**PADRÃO** — `terrain:` é *slot* do **`WorldModel`** quando o terreno serve à simulação
(`mainGndMapRdr`), mas em `mainTerrain` ele é *slot* do **display**
(`( TerrainDisplay terrain: ( QuadMap ... ) )`), porque ali não há simulação nenhuma —
só um visualizador de elevação.

**NESTA POC** — `poc/05-formation-flight` usa um único tile SRTM (`S23W043`), portanto um
`QuadMap` com um filho. O acesso em C++ passa por
`WorldModel::getTerrain()` — **que só é público na versão `const`** (registrado no
`CLAUDE.md`). Em `mainGndMapRdr/RealBeamRadar.cpp` o acesso é feito assim:

```cpp
const models::WorldModel* sim{own->getWorldModel()};
if (sim != nullptr) {
   setTerrain( dynamic_cast<const mixr::terrain::Terrain*>(sim->getTerrain()) );  // ddh
}
```

— note que o próprio exemplo oficial usa ponteiro `const` e deixou a marca `// ddh` de
quem contornou o problema.

## 6.7 PADRÃO: rede (DIS) e *image generator*

```
// mainSim1/configs/interop/dis.epp
( DisNetIO
   siteID: 1     applicationID: 1     exerciseID: 1
   enableInput: 1   enableOutput: 1   enableRelay: 0

   netInput:  ( UdpBroadcastHandler localIpAddress: localhost networkMask: "255.0.0.0"
                port: 3000  ignoreSourcePort: 3001  shared: true )
   netOutput: ( UdpBroadcastHandler localIpAddress: localhost networkMask: "255.0.0.0"
                port: 3000  localPort: 3001         shared: true )

   emissionPduHandlers: {
      ( EmissionPduHandler
         emitterName: 999   emitterFunction: 1
         sensor: ( Radar )
         antenna: ( Antenna localPlayersOfInterestOnly: true )
         defaultIn: true   defaultOut: true
      )
   }

   #include "DisIncomingEntityTypes.epp"
   #include "DisOutgoingEntityTypes.epp"
)
```

**REGRA** — `netInput` e `netOutput` são handlers **separados**, na mesma porta, com
`localPort` do lado de saída igual a `ignoreSourcePort` do lado de entrada. É assim que a
aplicação **não escuta o próprio tráfego** de broadcast. `shared: true` permite reuso do
endereço.

**PADRÃO** — o `EmissionPduHandler` com `emitterName: 0` e
`defaultIn/defaultOut: true` é o **catch-all**: qualquer emissão que não bata com um
*handler* específico usa esse. Em `mainSim1/configs/test3/networkSetup.epp` há três
*handlers*: dois específicos (`991` EW, `992` TWS, ambos com `defaultIn/Out: false`) e o
genérico.

**PADRÃO** — o dimensionamento de *buffer* aparece só no cenário de estresse:
`sendBuffSizeKb: 256  recvBuffSizeKb: 256`.

```
// mainSim1/configs/ighosts/pov.epp — o IG mais simples: só posição/atitude por UDP
( PovHost
    netOutput: ( UdpUnicastHandler localIpAddress: localhost ipAddress: localhost
                 port: 4950 localPort: 4951 shared: true)
)
```

```
// mainSim1/configs/ighosts/cigi.epp
( CigiHost
   maxRange: ( NauticalMiles 20.0 )   maxModels: 50   maxElevations: 20   async: false
   session: ( CigiHostSession
      netInput:  ( UdpUnicastHandler localIpAddress: "127.0.0.1" ipAddress: "127.0.0.1" port: 8008 )
      netOutput: ( UdpUnicastHandler localIpAddress: "127.0.0.1" ipAddress: "127.0.0.1" port: 8108 localPort: 8208 )
   )
   #include "CigiTypeMap.epp"      // mapeamento player → entity ID do IG
)
```

**ARMADILHA** — `CigiTypeMap.epp`, `DisIncomingEntityTypes.epp` e
`DisOutgoingEntityTypes.epp` são `#include`d mas **não existem** na árvore
`examples/` vendorizada — moram em `mixr-data`, fora deste repositório. Os `.epp` que os
incluem não preprocessam sem eles. Por isso todos os pontos de entrada trazem essas
linhas **comentadas** por padrão (`//#include "interop/dis.epp"`).

**NESTA POC** — nenhuma poc usa DIS nem CIGI/POV: os *ighosts* não existem no pacote
(§0.2) e o papel de "visualização externa" é cumprido pelo exportador Tacview próprio.
`interop_dis` **existe** no pacote e está disponível caso uma poc futura precise.

## 6.8 PADRÃO: cadeia infravermelha (`testInfrared`)

Estrutura análoga à de RF, mas com peças próprias:

```
// no WorldModel
atmosphere: ( IrAtmosphere
   waveBands:            ( Table1 x: [3.200 3.775 4.325 4.750] data: [0.4 0.75 0.35 0.5] )
   transmissivityTable1: ( Table1 x: [3.200 3.775 4.325 4.750] data: [0.011 0.004 0.28 0.017] )
   skyRadiance: 11.2     earthRadiance: 1.0
)
```

```
// no player-alvo (em vez de signature: ( SigSphere ... ))
irSignature: ( IrSignature
   irShapeSignature: ( IrBox x: (Meters 1.0) y: (Meters 1.0) z: (Meters 1.0) )
   baseHeatSignature: 320.0
   emissivity: 0.75
   effectiveArea: (SquareMeters 1.0)
)
```

```
// no míssil — comentário do próprio exemplo:
// if not configured with obc, sensor and seeker, will behave as simple aam
// to configure as an IR missile, need all of OBC, Sensor and Seeker
1: ( AamMissile
      power: 0   maxTOF: 150.0   maxBurstRng: 50   lethalRange: 30.0
      type: "AIM-9"
      enableMessageType: INFO
      components: { #include "IrAAMcomponents.epp" }
   )
```

**REGRA** — um `AamMissile` sem `obc`/`sensor`/`seeker` degrada para "míssil burro" com
guiamento simplificado. As três peças são necessárias e suficientes para guiamento IR
real.

**PADRÃO** — `enableMessageType: INFO` (ou `DEBUG`) liga a instrumentação de mensagens do
`Object` naquele objeto específico. É o *logging* nativo do framework.

## 6.9 PADRÃO: tabelas LFI como dados de configuração

```
// testRadar/configs/gainPattern.epp
gainPattern:
( Func1
   table: ( Table1
           //  0.0   1.0      1.5       2.5      3.0      3.5       4.0      4.5   graus
       x:    [ 0.0   0.01745  0.02618   0.04363  0.05236  0.061087  0.06981  0.07854 ] // rad
       data: [ 0.0  -3.0    -10.0     -30.0    -20.0    -14.0     -25.0    -80.0 ]     // dB
   )
)
```

**PADRÃO** — a convenção universal nos exemplos é escrever o **eixo em unidade legível
(graus) no comentário** e os **valores na unidade interna (radianos) no dado**. Os `x:`
precisam estar monotônicos.

**PADRÃO** — `Func1` embrulha a `Table1` para que o *slot* aceite "uma função de uma
variável". `testTables/main.cpp` mostra o uso direto (`tbl->lfi(x, storage)`) e a
otimização por `FStorage`:

```cpp
mixr::base::FStorage* s{};
if (sflg) s = tbl->storageFactory();     // cache do índice do último breakpoint
double value{tbl->lfi(x1, s)};
if (sflg && s != nullptr) { s->unref(); s = nullptr; }
```

**REGRA** — o `FStorage` acelera consultas sucessivas próximas (caso típico de um sensor
varrendo). É **por consumidor**, não por tabela: crie um por objeto que consulta.

---

# 7. PADRÕES DE EXTENSÃO: OS SEIS PONTOS QUE OS EXEMPLOS EXERCITAM

Levantamento do que a árvore oficial realmente estende — e do que **não** estende.

| Ponto de extensão | Exemplos | Frequência |
|---|---|---|
| `simulation::Station` | `mainSim2`, `mainSim3`, `mainQt1`, `testRadar`, `testInfrared`, `mainGndMapRdr`, `mainLaero`, `mainCockpit`, `mainUbf1`, `mainIgViewer`, `testRecordData` | **11** — o mais comum |
| `graphics::Display`/`Page`/`Graphic` | quase todos os gráficos | dezenas |
| `models::System` (sensor) | `mainGndMapRdr` (`RealBeamRadar`) | **1** |
| `models::WorldModel` | `testDafif` | **1** |
| `models::Player` | `mainSim2` (`SimPlayer`) | **1** (e trivial) |
| `linkage::IoHandler` | `mainSim2`, `mainCockpit`, `testRecordData`, `testLinkage` | **4** |
| `recorder::DataRecorder` / `PrintHandler` | `shared/xrecorder`, `testRecorderRead` | **2** |
| `base::NetHandler` | `shared/xzmq` | **1** |
| `base::StateMachine` | `testStateMach` | **4 classes** |
| `base::ubf::*` | `shared/xbehaviors` | **15 classes** |
| **`models::DynamicsModel`** | — | **NENHUM** |
| **`models::AbstractWeapon`** | — | **NENHUM** |

## 7.1 PADRÃO: estender `Station` — o padrão dominante

Todas as 11 subclasses de `Station` fazem essencialmente a mesma coisa. O gabarito:

```cpp
// mainSim3/Station.hpp
class Station final: public mixr::simulation::Station
{
    DECLARE_SUBCLASS(Station, mixr::simulation::Station)
public:
    Station();
    void reset() final;
private:
    mixr::glut::GlutDisplay* display{};   // o que a Station passa a conhecer
    bool displayInit{};                   // guarda de "já inicializei?"
    bool setSlotDisplay(mixr::glut::GlutDisplay*);
};
```

```cpp
// mainSim3/Station.cpp
IMPLEMENT_SUBCLASS(Station, "MapTestStation")
BEGIN_SLOTTABLE(Station)  "display",  END_SLOTTABLE(Station)
BEGIN_SLOT_MAP(Station)   ON_SLOT(1, setSlotDisplay, mixr::glut::GlutDisplay)  END_SLOT_MAP()

void Station::reset()
{
    if (display != nullptr && !displayInit) {
        display->createWindow();       // <<< o recurso pesado nasce AQUI, não no construtor
        display->focus(display);
        displayInit = true;
    }
    BaseClass::reset();
}

bool Station::setSlotDisplay(mixr::glut::GlutDisplay* dis)
{
    bool ok{};
    if (display != nullptr) { display->unref(); display = nullptr; }
    if (dis != nullptr) {
        display = dis;
        display->ref();
        display->container(this);      // <<< liga o objeto à árvore de componentes
        ok = true;
    }
    return ok;
}
```

**REGRA — três invariantes** que aparecem em **todas** as 11 subclasses:

1. **`createWindow()` (ou qualquer recurso pesado) vai no `reset()`, guardado por um
   *flag***, nunca no construtor nem no `setSlot`. Motivo: o `setSlot` roda durante o
   *parse*, quando a árvore ainda não está completa.
2. **`objeto->container(this)` no `setSlotX`.** Sem isso o objeto não faz parte da árvore
   de contenção e `findContainerByType()` a partir dele não acha a `Station` — quebrando
   o padrão de navegação do §8.4.
3. **`updateTC()` propaga para o display; `updateData()` não** (§3.4).

**PADRÃO** — três responsabilidades adicionais recorrem:

**(a) `stepOwnshipPlayer()`** — trocar o *ownship* para o próximo *player* aéreo local
(`mainSim2/SimStation.cpp`, `mainIgViewer`, `testRadar`, `mainCockpit`):

```cpp
void SimStation::stepOwnshipPlayer()
{
    mixr::base::PairStream* pl{getSimulation()->getPlayers()};
    if (pl != nullptr) {
       mixr::models::Player* f{};   // primeiro encontrado
       mixr::models::Player* n{};   // o "próximo" após o ownship atual
       bool found{};
       mixr::base::List::Item* item{pl->getFirstItem()};
       while (item != nullptr) {
           const auto pair = static_cast<mixr::base::Pair*>(item->getValue());
           if (pair != nullptr) {
               const auto ip = static_cast<mixr::models::Player*>(pair->object());
               if ( ip->isMode(mixr::models::Player::ACTIVE) &&
                    ip->isLocalPlayer() &&
                    ip->isClassType(typeid(mixr::models::AirVehicle)) ) {
                   if (f == nullptr) { f = ip; }
                   if (found) { n = ip; break; }
                   if (ip == getOwnship()) found = true;
               }
           }
           item = item->getNext();
       }
       if (found && n == nullptr) n = f;      // circular
       if (n != nullptr) setOwnshipPlayer(n);
       pl->unref();                            // <<< getPlayers() devolve com ref()
    }
}
```

**REGRA** — `Simulation::getPlayers()` devolve um `PairStream*` **já referenciado**;
quem chama **deve** dar `unref()`. Vale para toda a família `get*` que devolve ponteiro
de contêiner.

**(b) auto-RESET por temporizador** (`mainSim2/SimStation.cpp`, `mainIgViewer`):

```cpp
void SimStation::updateData(const double dt)
{
    if ( autoResetTimer > 0 && getSimulation()->isNotFrozen() ) {
       autoResetTimer -= dt;
       if (autoResetTimer <= 0) {
         mixr::base::Boolean newFrz(true);
         getSimulation()->event(FREEZE_EVENT, &newFrz);
         this->event(RESET_EVENT);
       }
    }
    BaseClass::updateData(dt);
}
```

**ARMADILHA** — isso duplica o *slot* nativo `startupResetTime` da `Station` (§3.7).

**(c) dois displays em vez de um** (`mainLaero/TestStation.cpp`: `glutDisplay:` +
`mapDisplay:`) — mesma mecânica, dois pares de ponteiro+flag.

## 7.2 PADRÃO: estender `Player`

Único exemplo, e é deliberadamente vazio:

```cpp
// mainSim2/SimPlayer.cpp — na íntegra
IMPLEMENT_SUBCLASS(SimPlayer, "SimPlayer")
EMPTY_DELETEDATA(SimPlayer)
EMPTY_COPYDATA(SimPlayer)
EMPTY_SLOTTABLE(SimPlayer)

SimPlayer::SimPlayer()
{
    STANDARD_CONSTRUCTOR()
    static mixr::base::String generic("Sim");
    setType(&generic);                    // define o "type" padrão
}

void SimPlayer::reset()
{
    BaseClass::reset();
}
```

**POR QUÊ** — a lição é justamente essa: **estender `Player` raramente é necessário.** Um
*player* é composto por `components:`; comportamento novo entra como um `System` filho,
não como uma subclasse de `Player`. `SimPlayer` só existe para dar um nome de fábrica
próprio ao cenário de `mainSim2`.

**REGRA** — antes de herdar de `Aircraft`, pergunte se o comportamento não cabe em um
`System` filho (§7.4) ou num `DynamicsModel` (§6.4).

## 7.3 PADRÃO: estender um `System`/sensor — `RealBeamRadar`

**Arquivos**: `mainGndMapRdr/RealBeamRadar.{hpp,cpp}`. É o **único** exemplo de modelo de
sensor novo em toda a árvore, e portanto o gabarito de referência para esta PoC.

```cpp
class RealBeamRadar final: public mixr::models::Radar
{
    DECLARE_SUBCLASS(RealBeamRadar, mixr::models::Radar)
public:
    RealBeamRadar();
    virtual bool setTerrain(const mixr::terrain::Terrain* const msg);
    double getBeamWidth() const final { return beamWidth; }   // sobrescreve a base
    const unsigned char* getImage() const { return image; }
    // utilitários estáticos de geometria
    static bool computeGroundRanges(double* const, const unsigned int, const double);
    static bool computeSlantRanges2(double* const, const unsigned int, const double* const, const double);
    static bool computeRangeLoss(double* const, const unsigned int, const double* const);
    static bool computeEarthCurvature(double* const, const unsigned int, const double, const double);
protected:
    void transmit(const double dt) final;      // <<< O PONTO DE ENGATE
private:
    bool setSlotInterpolate(const mixr::base::Number* const);
};
```

**REGRA** — o ponto de engate de um sensor RF é **`transmit(dt)`**, e ele **deve** chamar
`BaseClass::transmit(dt)` primeiro:

```cpp
void RealBeamRadar::transmit(const double dt)
{
   BaseClass::transmit(dt);
   // ... a partir daqui, o modelo próprio
}
```

**PADRÃO** — o corpo de `transmit()` segue quatro passos que valem para qualquer sensor:

```cpp
   // (1) obter o ownship e seus parâmetros
   const models::Player* own{getOwnship()};
   if (own != nullptr) {
      altitude  = static_cast<double>(own->getAltitude());
      latitude  = own->getLatitude();
      longitude = own->getLongitude();

      // (2) localizar recursos do mundo, uma única vez (lazy)
      if (terrain == nullptr) {
         const models::WorldModel* sim{own->getWorldModel()};
         if (sim != nullptr) setTerrain( dynamic_cast<const terrain::Terrain*>(sim->getTerrain()) );
      }
   }

   // (3) só trabalhar se todas as pré-condições estiverem satisfeitas
   const models::Antenna* ant{getAntenna()};
   if (isTransmitting() && ant != nullptr && image != nullptr &&
       terrain != nullptr && terrain->isDataLoaded()) {

      // (4) usar o estado da antena como entrada geométrica
      antAzAngle = static_cast<double>(ant->getAzimuthD());
      antElAngle = static_cast<double>(ant->getElevationD());
      double maxRngNM{getRange()};
      // ... modelo próprio ...
   }
```

**PADRÃO** — o modelo próprio consome utilitários **do framework** em vez de reimplementar:

```cpp
   terrain->getElevations(elevations, validFlgs, IMG_HEIGHT,
                          latitude, longitude, direction, groundRange[IMG_HEIGHT-1], interpolate);
   terrain::Terrain::vbwShadowChecker(maskFlgs, elevations, validFlgs, IMG_HEIGHT,
                                      groundRange[IMG_HEIGHT-1], altitude, antElAngle, beamWidth);
   terrain::Terrain::aac(aacData, elevations, maskFlgs, IMG_HEIGHT,
                         groundRange[IMG_HEIGHT-1], altitude);
   computeEarthCurvature(curvature, IMG_HEIGHT, maxRngNM, static_cast<double>(base::nav::ERAD60));
```

**PADRÃO** — alocação em bloco no construtor, com `initImageMemory`/`copyImageMemory`/
`freeImageMemory` chamadas de construtor/`copyData`/`deleteData` respectivamente. É o
padrão para membros que são *buffers* crus (não objetos MIXR).

**PADRÃO** — o EDL correspondente troca `( Radar ... )` por `( RealBeamRadar ... )` e
acrescenta o *slot* novo, sem mais nada:

```
sensors: ( RealBeamRadar
    antennaName: radar
    powerPeak:  ( KiloWatts 500.0 )   frequency: ( GigaHertz 3.0 )
    PRF: ( Hertz 500.0 )              pulseWidth: ( MilliSeconds 0.01 )
    threshold: ( dB 2.0 )
    ranges: [ 60 120 ]                initRangeIdx: 1
    interpolate: true       // <<< o único slot novo
)
```

**NESTA POC** — este é o gabarito a seguir quando alguma poc precisar de um sensor
próprio. `poc/06`–`poc/08` usam sensores nativos sem estender nada; a extensão feita em
`poc/08-event-relay` (`RadarContactRelay`) herda de `System` e engata em `process(dt)`,
que é a fase 3 — o análogo do `transmit()` para um sistema que **consome** o que os
sensores produziram.

## 7.4 PADRÃO: estender `WorldModel` — e sobrescrever um nome de fábrica

**Arquivos**: `testDafif/models/WorldModel.{hpp,cpp}`.

```cpp
class WorldModel : public mixr::models::WorldModel
{
   DECLARE_SUBCLASS(WorldModel, mixr::models::WorldModel)
public:
   mixr::dafif::AirportLoader* getAirports();
   mixr::dafif::NavaidLoader* getNavaids();
   mixr::dafif::WaypointLoader* getWaypoints();
   void updateData(const double dt = 0.0) override;
private:
   bool setSlotAirports(mixr::dafif::AirportLoader* const);
   // ...
};
```

```cpp
IMPLEMENT_SUBCLASS(WorldModel, "WorldModel")     // <<< MESMO nome de fábrica da base!
```

**PADRÃO** — registrar a subclasse com o **mesmo nome de fábrica** da classe do framework
e colocá-la **antes** na cadeia (§2.1) faz todo `( WorldModel ... )` do EDL instanciar a
versão estendida, **sem alterar um único arquivo de cenário**. É a técnica de
substituição transparente.

**PADRÃO** — carga incremental de dados pesados, um item por quadro, dentro de
`updateData()`:

```cpp
void WorldModel::updateData(const double dt)
{
   // Load DAFIF files (one pre frame)
   if      (airports  != nullptr && airports->numberOfRecords()  == 0) airports->load();
   else if (navaids   != nullptr && navaids->numberOfRecords()   == 0) navaids->load();
   else if (waypoints != nullptr && waypoints->numberOfRecords() == 0) waypoints->load();

   BaseClass::updateData(dt);
}
```

**POR QUÊ** — carregar tudo no `reset()` bloquearia a simulação por segundos. O `else if`
garante **um** carregamento por quadro de fundo.

## 7.5 PADRÃO: estender `IoHandler`

**Arquivos**: `testLinkage/TestIoHandler.cpp` (mínimo), `mainSim2/SimIoHandler.cpp` e
`mainCockpit/TestIoHandler.cpp` (completo). Detalhado em §9.

O mínimo é literalmente isto:

```cpp
class TestIoHandler final: public mixr::linkage::IoHandler
{
   DECLARE_SUBCLASS(TestIoHandler, mixr::linkage::IoHandler)
public:
   TestIoHandler();
private:
   void inputDevicesImpl(const double dt) override   { readDeviceInputs(dt);   }
   void outputDevicesImpl(const double dt) override  { writeDeviceOutputs(dt); }
};
```

**REGRA** — `inputDevicesImpl`/`outputDevicesImpl` são os dois únicos métodos a
sobrescrever. `readDeviceInputs(dt)`/`writeDeviceOutputs(dt)` são helpers protegidos da
base que percorrem `devices:` e seus `adapters:`.

## 7.6 PADRÃO: estender o gravador de dados

**Arquivos**: `shared/xrecorder/`, `testRecorderRead/PrintMyData.cpp`. Detalhado em §12.

Três peças, sempre juntas:

1. **`dataRecorderTokens.hpp`** — IDs de evento próprios, a partir do primeiro livre:
   ```cpp
   #include "mixr/simulation/dataRecorderTokens.hpp"
   #define REID_MY_DATA_EVENT       REID_FIRST_USER_EVENT
   ```
2. **`DataRecord.proto`** — mensagem protobuf nova, registrada como *extension* da
   mensagem `DataRecord` do framework.
3. **A subclasse de `recorder::DataRecorder`** com sua tabela de despacho:
   ```cpp
   IMPLEMENT_SUBCLASS(DataRecorder, "XDataRecorder")
   BEGIN_RECORDER_HANDLER_TABLE(DataRecorder)
      ON_RECORDER_EVENT_ID( REID_MY_DATA_EVENT, recordMyData )
   END_RECORDER_HANDLER_TABLE()
   ```

**PADRÃO** — a mesma subclasse pode **sobrescrever** um *handler* da base para enriquecer
o registro nativo (aqui, `recordMarker` ganha um campo `foo` via *extension* protobuf).

## 7.7 PADRÃO: estender `NetHandler`

**Arquivos**: `shared/xzmq/ZeroMQHandler.{hpp,cpp}`, `ZeroMQContext.{hpp,cpp}`.

```cpp
class ZeroMQHandler final: public base::NetHandler
{
   DECLARE_SUBCLASS(ZeroMQHandler, base::NetHandler)
public:
   bool initNetwork(const bool noWaitFlag) final;
   bool isConnected() const final;
   bool closeConnection() final;
   bool sendData(const char* const packet, const int size) final;
   unsigned int recvData(char* const packet, const int maxSize) final;
   bool setBlocked() final;
   bool setNoWait() final;
};
```

**REGRA** — sete métodos virtuais puros compõem o contrato de `NetHandler`. Implementados
todos, o novo transporte é utilizável **em qualquer lugar** que aceite um `NetHandler`:
`DisNetIO.netInput`, `CigiHostSession.netOutput`, `RecorderNetOutput.netHandler`,
`Endpoint.netHandler`. O `.epp` só troca o nome de fábrica.

O próprio cabeçalho registra a honestidade sobre a herança:

```cpp
// The derivation from NetHandler is to allow this handler to be used in place
// of other NetHandler objects but 0MQ is so dissimilar that only the
// signature type is used - little sharing of methods.
```

**PADRÃO** — o `ZeroMQContext` é um objeto MIXR que embrulha um recurso global de
biblioteca externa, com um `static ZeroMQContext* masterContext` como padrão. É o padrão
para "recurso de processo" que precisa aparecer no EDL.

**NESTA POC** — `poc/04`+ implementam `tacview::RealtimeTelemetryServer` com socket
POSIX cru, **não** como `NetHandler`. Torná-lo um `NetHandler` seria o caminho canônico
se ele precisasse ser plugado em `RecorderNetOutput` ou similar; como ele é o consumidor
final e fala um protocolo próprio, a simplificação é defensável.

## 7.8 PADRÃO: a biblioteca `x*` — como empacotar extensões reutilizáveis

Estrutura invariável (`xbehaviors`, `xpanel`, `xrecorder`, `xzmq`):

```
shared/xNOME/
├── factory.hpp          // namespace mixr::xNOME { base::Object* factory(const std::string&); }
├── factory.cpp          // cadeia de if/else if sobre getFactoryName()
├── <classes>.{hpp,cpp}  // no namespace mixr::xNOME
├── Makefile             // produz $(MIXR_EXAMPLES_LIB_PATH)/libxNOME.a
└── [<config>.epp]       // fragmento de EDL que configura a biblioteca (xpanel)
```

```cpp
// shared/xzmq/factory.cpp — o gabarito completo
namespace mixr { namespace xzmq {
base::Object* factory(const std::string& name)
{
    base::Object* obj{};
    if      ( name == ZeroMQContext::getFactoryName() ) obj = new ZeroMQContext;
    else if ( name == ZeroMQHandler::getFactoryName() ) obj = new ZeroMQHandler;
    return obj;
}
}}
```

**REGRA** — a `factory()` de uma biblioteca `x*` **não** encadeia para outras fábricas;
devolve `nullptr` e deixa o `factory()` da aplicação decidir o próximo elo. Só o
`factory()` da aplicação encadeia.

**REGRA** — a biblioteca vive em `namespace mixr::xNOME`, dentro do *namespace* do
framework. Já as classes das **aplicações** (não das bibliotecas) ficam no *namespace*
global: `SimStation`, `TestDisplay`, `Worm`, `MyComp` — sem *namespace* nenhum.

**NESTA POC** — o análogo é o diretório `include/`/`src/` da **raiz** do repositório,
reservado para código compartilhado entre subprojetos (hoje vazio). Quando o
`tacview::RealtimeTelemetryServer` (hoje duplicado em `poc/04`, `05`, `07`, `08`, `09`,
`10`) for consolidado, este é o padrão a seguir: um *namespace* próprio, uma `factory()`
que não encadeia, e um alvo de biblioteca no Meson.

---

# 8. PADRÕES DE COMUNICAÇÃO ENTRE COMPONENTES

O MIXR oferece três formas de um componente falar com outro, e os exemplos usam as três
com propósitos bem distintos.

## 8.1 PADRÃO: `send()` + `SendData` — empurrar um valor para um filho nomeado

**Arquivos**: `testEvents/`, `mainLaero/AdiDisplay.cpp`, `mainSim2/InstrumentPanel.cpp`,
`demoInstruments/test_pages/*.cpp`, `mainCockpit/TestDisplay.cpp` — dezenas de usos.

```cpp
// mainLaero/AdiDisplay.hpp — o par membro/SendData
   double psiRO{};          // valor
   SendData psiRO_SD;       // estado do último envio
```

```cpp
// mainLaero/AdiDisplay.cpp — updateData()
   mixr::models::Aircraft* pA{getOwnship()};
   if (pA != nullptr) {
      psiRO = pA->getHeadingD();
      thtRO = pA->getPitchD();
      // ...
   }
   send("psiRO", UPDATE_VALUE, psiRO, psiRO_SD);
   send("thtRO", UPDATE_VALUE, thtRO, thtRO_SD);
   send("altRO", UPDATE_VALUE, altRO, altRO_SD);
   send("mfdADI", UPDATE_INSTRUMENTS, pitchADI, pitchADI_SD);
   send("mfdADI", UPDATE_VALUE,       bankADI,  bankADI_SD);
```

**REGRA** — `send(nome, evento, valor, sendData)` **resolve o nome entre os FILHOS de
quem chama `send()`**, não em si mesmo nem na árvore inteira. Este é o erro nº 1 do
mecanismo — o mesmo diagnosticado em `poc/08-event-relay`, onde `send()` era chamado em
`this` tentando alcançar um **irmão**. A correção é chamar `send()` no **contêiner**
correto: `getOwnship()->send("localAlert", ...)`.

**REGRA** — o `SendData` é **por par (alvo, evento)**, e memoriza o último valor enviado
para que o `send()` não repita eventos redundantes. Daí o padrão de dois `SendData`
distintos para o **mesmo alvo** com **eventos diferentes** (`mfdADI` acima recebe
`UPDATE_INSTRUMENTS` e `UPDATE_VALUE`, cada um com seu `SendData`).

**REGRA** — todo `SendData` é **esvaziado** (`.empty()`) em `copyData()`, nunca copiado
(§4.6).

**PADRÃO** — sobrecargas de `send()` para todos os tipos escalares e para `const char*`:

```cpp
// testEvents/ObjectHandler.cpp — a demonstração exaustiva
   bool boolVal{obj->getBoolean()};      send("objboolean", UPDATE_VALUE, boolVal,  boolSD);
   int intVal{obj->getInteger()};        send("objinteger", UPDATE_VALUE, intVal,   intSD);
   float floatVal{obj->getFloat()};      send("objfloat",   UPDATE_VALUE, floatVal, floatSD);
   double doubleVal{obj->getDouble()};   send("objdouble",  UPDATE_VALUE, doubleVal,doubleSD);
   const std::string& myChar{obj->getChar()};
   send("objascii", UPDATE_VALUE, myChar.c_str(), charSD);
```

E os alvos, no EDL, são simplesmente componentes rotulados:

```
// testEvents/configs/test.epp
    objtest: ( ObjectHandler
        components: {
            objboolean: ( NumericReadout format: "#"        position: [ 12 10 ] )
            objinteger: ( NumericReadout format: "###"      position: [ 13  9 ] )
            objfloat:   ( NumericReadout format: "00#.####" position: [ 14 10 ] )
        }
    )
```

**POR QUÊ** — `send()` é o mecanismo que mantém **lógica e apresentação desacopladas**:
quem calcula não sabe se o destino é um mostrador numérico, um ponteiro analógico ou um
componente que ainda não existe. A ligação é feita por **nome no arquivo de configuração**.

**PADRÃO** — os dois eventos mais usados são `UPDATE_VALUE` (valor principal) e
`UPDATE_INSTRUMENTS` (valor para instrumento gráfico). Um mesmo alvo aceita ambos, com
significados diferentes — é assim que um ADI recebe *pitch* por um e *roll* pelo outro.

## 8.2 PADRÃO: tabela de eventos (`BEGIN_EVENT_HANDLER`)

```cpp
// mainCockpit/TestDisplay.cpp
BEGIN_EVENT_HANDLER(TestDisplay)
   ON_EVENT('r',onResetKey)
   ON_EVENT('R',onResetKey)
   ON_EVENT('f',onFreezeKey)
   ON_EVENT('F',onFreezeKey)
   ON_EVENT('l',onWpnRelKey)
   ON_EVENT('L',onWpnRelKey)
   ON_EVENT('s',onTgtStepKey)
   ON_EVENT('2',onRtn2SearchKey)
   ON_EVENT('a',onAir2AirKey)
   ON_EVENT('g',onAir2GndKey)
   ON_EVENT('i',onIncRngKey)
   ON_EVENT('d',onDecRngKey)
   ON_EVENT('+',onStepOwnshipKey)
END_EVENT_HANDLER()
```

**PADRÃO** — teclas são eventos cujo código é o **próprio caractere**; maiúscula e
minúscula são registradas separadamente apontando para o mesmo *handler*. O `.hpp`
documenta o mapa em comentário, e o `README.md` do exemplo o repete para o usuário.

**PADRÃO** — cada *handler* é um método privado `bool onXxxKey()` que devolve `true`
(consumido). O corpo é sempre curto e delega:

```cpp
bool TestDisplay::onFreezeKey()
{
   if ( getSimulation() != nullptr ) {
      mixr::base::Boolean newFrz( !getSimulation()->isFrozen() );
      getSimulation()->event(FREEZE_EVENT, &newFrz);
   }
   return true;
}

bool TestDisplay::onWpnRelKey()
{
   if (getOwnship() != nullptr) getOwnship()->event(WPN_REL_EVENT);
   return true;
}
```

**PADRÃO — `ON_EVENT_OBJ`** para evento que carrega um objeto tipado:

```cpp
// testEvents/ObjectHandler.cpp
BEGIN_EVENT_HANDLER(ObjectHandler)
    ON_EVENT_OBJ(UPDATE_VALUE, onUpdateObject, TestObject)
END_EVENT_HANDLER()

bool ObjectHandler::onUpdateObject(const TestObject* const x) { /* ... */ return true; }
```

O framework faz o `dynamic_cast` para `TestObject` antes de chamar; se o objeto for de
outro tipo, o *handler* não é invocado.

**PADRÃO** — **tabela de eventos vazia** é legítima e significa "não trato nenhum
evento, mas quero a tabela para que a cadeia funcione":

```cpp
// tutorial07/Worm.cpp
BEGIN_EVENT_HANDLER(Worm)
END_EVENT_HANDLER()
```

**PADRÃO — eventos de usuário** a partir de `Component::USER_EVENTS`:

```cpp
// mainIgViewer/events.hpp — na íntegra
#include "mixr/base/Component.hpp"
// synonym for convenience
const int USER_EVENT_ON_ENTRY = ::mixr::base::Component::USER_EVENTS + 1;
```
```cpp
// mainIgViewer/SimpleIGen.cpp
BEGIN_EVENT_HANDLER(SimpleIGen)
   ON_EVENT(USER_EVENT_ON_ENTRY, onEntry)
END_EVENT_HANDLER()
```

**REGRA** — eventos com código ≤ 999 (`MAX_KEY_EVENT`) sobem para o `container()` quando
não tratados; eventos não-tecla sem *handler* **morrem silenciosamente**. Ver
`MIXR-CONTEXT.md` §5.4.

**PADRÃO** — `shutdownNotification()` sobrescrito para propagar para cima antes de
delegar à base:

```cpp
// mainCockpit/TestDisplay.cpp
bool TestDisplay::shutdownNotification()
{
   mixr::base::Component* parent{container()};
   if (parent != nullptr) parent->event(SHUTDOWN_EVENT);
   return BaseClass::shutdownNotification();
}
```

**NESTA POC** — `poc/08-event-relay` foi construída inteiramente em torno deste
mecanismo, com `CONTACT_EVENT = USER_EVENTS + 1 = 2001` e payload
`RadarContactMessage` como `base::Object` completo — exatamente o padrão de
`testEvents/TestObject` + `ON_EVENT_OBJ`. A poc também descobriu, por leitura do fonte,
que `Datalink::sendMessage()` usa a mesma técnica manual (`findByName` no `WorldModel` +
`event()` direto), confirmando que **não existe roteamento declarativo de eventos** —
toda "assinatura" é código C++.

## 8.3 PADRÃO: navegar a árvore e **cachear** o ponteiro

Os exemplos nunca guardam ponteiros resolvidos em tempo de configuração; eles os
descobrem no primeiro `updateData()` e memorizam.

```cpp
// mainSim2/InstrumentPanel.cpp — o gabarito, replicado em ~8 arquivos
mixr::simulation::Station* InstrumentPanel::getStation()
{
   if (myStation == nullptr) {
      const auto s = dynamic_cast<mixr::simulation::Station*>(
                        findContainerByType(typeid(mixr::simulation::Station)) );
      if (s != nullptr) myStation = s;
   }
   return myStation;
}

mixr::models::Player* InstrumentPanel::getOwnship()
{
   mixr::models::Player* p = nullptr;
   mixr::simulation::Station* sta = getStation();
   if (sta != nullptr) p = dynamic_cast<mixr::models::Player*>(sta->getOwnship());
   return p;
}
```

**PADRÃO** — a **cadeia canônica de acesso** a partir de qualquer componente:

```
componente
  └─ findContainerByType(typeid(Station))   → Station    (sobe a árvore)
       ├─ getSimulation()                   → Simulation/WorldModel
       │    └─ getPlayers()                 → PairStream de players  (unref() depois!)
       └─ getOwnship()                      → Player
            ├─ getSensorByType(typeid(Tws)) → Pair com o sensor
            ├─ getPilotByType(typeid(Autopilot))
            ├─ getStoresManagement()        → StoresMgr
            ├─ getNavigation()              → Navigation → getPriRoute() → Route
            └─ getOnboardComputer()         → OnboardComputer → getTrackManagerByName(...)
```

**PADRÃO** — descoberta de **filho por tipo**, com o mesmo *lazy caching*:

```cpp
// mainSim3/MapPage.cpp
   if (loader == nullptr) {
      mixr::base::Pair* pair{findByType(typeid(mixr::graphics::SymbolLoader))};
      if (pair != nullptr) {
         loader = dynamic_cast<mixr::graphics::SymbolLoader*>(pair->object());
         if (loader != nullptr) loader->ref();
      }
   }
```

**REGRA** — `findByType()`/`findByName()` devolvem `base::Pair*`; o objeto está em
`pair->object()`. Guardar o ponteiro exige `ref()`; liberar exige `unref()` em
`deleteData()`.

**PADRÃO** — busca por **tipo** quando o subsistema é único (`Autopilot`, `SymbolLoader`,
`Station`), e por **nome** quando há vários do mesmo tipo (`getTrackManagerByName("twsTrkMgr")`).

```cpp
// mainCockpit/TestDisplay.cpp — sensor por tipo, escolhendo entre dois
   mixr::models::Radar* rdr{};
   {
      mixr::base::Pair* pair{getOwnship()->getSensorByType(typeid(mixr::models::Tws))};
      if (pair != nullptr) rdr = static_cast<mixr::models::Radar*>(pair->object());
   }
   mixr::models::StoresMgr* sms{getOwnship()->getStoresManagement()};
   if (sms != nullptr && sms->isWeaponDeliveryMode(mixr::models::StoresMgr::A2G)) {
      mixr::base::Pair* pair{getOwnship()->getSensorByType(typeid(mixr::models::Gmti))};
      if (pair != nullptr) rdr = static_cast<mixr::models::Radar*>(pair->object());
   }
```

**ARMADILHA** — este mesmo bloco de ~12 linhas aparece **quatro vezes** em
`mainCockpit/TestDisplay.cpp` (nos *handlers* de tecla e no `updateData()`), copiado e
colado. Extraia para um método.

**NESTA POC** — o `main.cpp` de `poc/06`/`poc/07` usa a mesma cadeia
(`getOnboardComputer()->getTrackManagerByName("twsTrkMgr")->getTrackList(...)`), e
`poc/08` usa `WorldModel::getPlayers()->findByName(...)` para alcançar outro *player* —
ambas confirmadas contra os exemplos.

## 8.4 PADRÃO: percorrer um `PairStream`

O laço canônico, idêntico em ~15 arquivos:

```cpp
mixr::base::PairStream* pl{getSimulation()->getPlayers()};
if (pl != nullptr) {
   mixr::base::List::Item* item{pl->getFirstItem()};
   while (item != nullptr) {
       const auto pair = static_cast<mixr::base::Pair*>(item->getValue());
       if (pair != nullptr) {
           const auto p = static_cast<mixr::models::Player*>(pair->object());
           // ... usa p ...
       }
       item = item->getNext();
   }
   pl->unref();                  // <<< obrigatório
}
```

Variante `const`, com `for` (`shared/xbehaviors/PlaneState.cpp`):

```cpp
const base::PairStream* players{sim->getPlayers()};
bool finished{};
for (const base::List::Item* item = players->getFirstItem();
     item != nullptr && !finished; item = item->getNext()) {
   const auto pair = static_cast<const base::Pair*>(item->getValue());
   const auto player = static_cast<const models::Player*>(pair->object());
   // ...
}
```

**PADRÃO** — o rótulo (nome) do par se obtém por `pair->slot()->getString()`:

```cpp
// testTimer/Tester.cpp
std::printf("  timer(%s)", pair->slot()->getString());
```

**REGRA** — use `static_cast` quando o tipo é garantido pela estrutura (todo item de
`players:` é `Player`), e `dynamic_cast` quando é hipótese a verificar (todo item de
`components:` pode ser qualquer coisa). Os exemplos seguem essa distinção com
consistência.

---

# 9. PADRÕES DE ENTRADA/SAÍDA (`linkage`)

## 9.1 PADRÃO: a pilha de quatro camadas

```
ioHandler: ( TestIoHandler                    ← 1. orquestrador (subclasse sua)
   inputData: ( IoData numAI: 10 numDI: 40 )  ← 2. buffer de canais
   devices: {                                 ← 3. dispositivos físicos
      ( UsbJoystick
         deviceIndex: 0
         adapters: {                          ← 4. mapeamento canal↔canal
            ( AnalogInput   ai: ROLL_AI     channel: 0  offset: 0.0  gain: 1.0 )
            ( DiscreteInput di: TRIGGER_SW1 port: 0     channel: 0 )
         }
      )
   }
)
```

**REGRA — a numeração difere por camada e é a armadilha central**, documentada no próprio
exemplo:

```
// testLinkage/configs/test1.epp
//   2) using standard iolinkage::IoData, so its AI & DI channels are one(1) based.
//   3) UsbJoystick channels are zero(0) based.
```

`ai:`/`di:` (canal lógico no `IoData`) contam de **1**; `channel:` (canal físico no
dispositivo) conta de **0**.

**PADRÃO** — `numAI`/`numDI` dimensionam o *buffer*; os exemplos usam
`numAI: 10 numDI: 40` (cockpit) ou `numAI: 20 numDI: 40` (banco de teste).

## 9.2 PADRÃO: `channel_map.hpp` — nomes simbólicos compartilhados C++/EDL

```cpp
// mainSim2/configs/linkage/channel_map.hpp
#define ROLL_AI            1
#define PITCH_AI           2
#define THROTTLE_AI        3
#define RUDDER_AI          4
// ...
#define TRIGGER_SW2         2
#define PICKLE_SW           3
#define PADDLE_SW           5
#define TMS_UP_SW           7
// ...
#define RESET_SW           31  /* (IFF-OUT switch) */
#define FREEZE_SW          32  /* (RADAR uncage switch) */
#define RELOAD_SW          33  /* (IFF-IN switch) */
#define CTL_ENABLE_SW      34  /* Enables reset, freeze, reload (PINKY switch) */
```

Incluído nos dois lados:

```cpp
// mainSim2/SimIoHandler.cpp
#include "configs/linkage/channel_map.hpp"
// ...
inData->getAnalogInput(ROLL_AI, &ai);
```
```
// mainSim2/configs/linkage/saitekEVO.epp
#include "channel_map.hpp"
( UsbJoystick deviceIndex: 0 adapters: {
      ( AnalogInput ai: ROLL_AI  channel: 0  offset: 0.0  gain: 1.0 )
```

**POR QUÊ** — é o mesmo truque de `dataRecorderTokens.hpp` (§5.2): **um cabeçalho, duas
linguagens**, impedindo que o C++ e o EDL divirjam sobre o significado do canal 3.

**PADRÃO** — um arquivo `.epp` por modelo de hardware (`saitekEVO`, `saitekST290`,
`thrustmaster`, `warthog`), todos definindo os **mesmos** canais lógicos a partir de
canais físicos diferentes. O `main` do cenário escolhe descomentando um `#include`.

## 9.3 PADRÃO: `MockDevice` — desenvolver sem hardware

```
// testLinkage/configs/linkage/MockDevice.epp
( MockDevice
   generators: {
      ( AnalogInputFixed ai: 11     value: -1.0 )
      ( AnalogInputFixed ai: 13     value:  0.0 )
      ( AnalogSignalGen  ai: 16     signal: "SINE"    frequency: (Hertz 0.1) )
      ( AnalogSignalGen  ai: 17     signal: "COSINE"  frequency: (Hertz 0.1) )
      ( AnalogSignalGen  ai: 18     signal: "SQUARE"  frequency: (Hertz 1.0) )
      ( AnalogSignalGen  ai: 19     signal: "SAW"     frequency: (Hertz 0.2) )
      ( DiscreteInputFixed di: 31   signal: "ON" )
      ( DiscreteInputFixed di: 32   signal: "OFF" )
   }
)
```

**PADRÃO** — `MockDevice` usa o *slot* `generators:` (não `adapters:`) e **coexiste** com
um dispositivo real na mesma lista `devices:`, preenchendo os canais que o hardware não
tem:

```
// mainSim2/configs/linkage/saitekEVO.epp — as duas coisas no mesmo arquivo
( UsbJoystick deviceIndex: 0 adapters: { ... } )

( MockDevice generators: {
      ( DiscreteInputFixed di: TMS_PUSH_SW     signal: OFF )
      ( DiscreteInputFixed di: DMS_PUSH_SW     signal: OFF )
      ( DiscreteInputFixed di: CMS_PUSH_SW     signal: OFF )
} )
```

**POR QUÊ** — o Saitek EVO não tem os botões de *push* do HOTAS completo; sem o
`MockDevice`, esses canais ficariam com lixo. Este é o padrão para **degradar
graciosamente** entre configurações de hardware.

**NESTA POC** — é o análogo direto da estratégia de `poc/05-formation-flight`, em que
`KeyboardDevice` falha graciosamente (`tcgetattr` falha, o programa segue sem entrada)
quando não há TTY interativo.

## 9.4 PADRÃO: detecção de borda de chave

```cpp
// mainSim2/SimIoHandler.hpp — um flag "N-1" por chave
private:
   bool rstSw1{};
   bool frzSw1{};
   bool wpnReloadSw1{};
   bool wpnRelSw1{};
   bool trgSw1{};
   bool tgtStepSw1{};
   // ...
```

```cpp
// mainSim2/SimIoHandler.cpp — borda de subida (dispara uma vez)
{  bool sw{};
   inData->getDiscreteInput(RESET_SW, &sw);
   const bool rstSw{sw && enabled};
   if (rstSw && !rstSw1) {          // <<< subiu agora
      sta->event(RESET_EVENT);
   }
   rstSw1 = rstSw;
}
```

```cpp
// mudança de estado (dispara nos dois sentidos, passando o novo estado)
{  bool sw{};
   inData->getDiscreteInput(PICKLE_SW, &sw);
   if (sw != wpnRelSw1) {
      mixr::base::Boolean sw(sw);
      av->event(WPN_REL_EVENT, &sw);
   }
   wpnRelSw1 = sw;
}
```

**REGRA** — o sufixo `1` no nome do membro significa "valor do quadro N-1". Convenção
consistente em `SimIoHandler`, `TestIoHandler` e `TestStation` de `testRadar`.

**PADRÃO** — cada bloco de chave fica num escopo `{ }` próprio, para que a variável
local `sw` possa se repetir sem colisão. Torna o método longo, mas cada bloco é lido
isoladamente.

**PADRÃO** — **chave de habilitação** (`CTL_ENABLE_SW`, o *pinky*) protege comandos
destrutivos:

```cpp
bool enabled{};
inData->getDiscreteInput(CTL_ENABLE_SW, &enabled);
// ... e todo comando crítico faz  sw && enabled
```

## 9.5 PADRÃO: `inputDevicesImpl()` — do buffer para o modelo

O corpo completo de `SimIoHandler::inputDevicesImpl()` é o gabarito de "traduzir entrada
crua em comandos de simulação". Sequência:

```cpp
void SimIoHandler::inputDevicesImpl(const double dt)
{
   readDeviceInputs(dt);                                   // (1) hardware → IoData

   const mixr::base::AbstractIoData* const inData{getInputData()};

   // (2) localizar Station / Simulation / ownship
   const auto sta = static_cast<SimStation*>( findContainerByType(typeid(SimStation)) );
   mixr::simulation::Simulation* sim{};
   mixr::models::AirVehicle* av{};
   if (sta != nullptr) {
      sim = sta->getSimulation();
      av  = dynamic_cast<mixr::models::AirVehicle*>(sta->getOwnship());
   }

   if (av != nullptr && sim != nullptr && inData != nullptr) {      // (3) guarda única

      // (4) localizar o autopilot OPCIONAL
      mixr::models::Autopilot* ap{};
      {  mixr::base::Pair* p{av->getPilotByType( typeid( mixr::models::Autopilot) )};
         if (p != nullptr) ap = static_cast<mixr::models::Autopilot*>(p->object());
      }

      // (5) controles de simulação (freeze/reset/reload) — borda de subida
      // (6) controles de voo — valor contínuo
      {  double ai{};
         inData->getAnalogInput(ROLL_AI, &ai);
         const double aiLim{mixr::base::alim(ai, 1.0)};       // satura em ±1
         if (ap != nullptr) ap->setControlStickRollInput(aiLim);
         else               av->setControlStickRollInput(aiLim);
      }
      // (7) eventos discretos — WPN_REL_EVENT, TGT_STEP_EVENT, SENSOR_RTS...
      // (8) navegação — incrementar/decrementar steerpoint
   }
}
```

**REGRA — o comando vai para o `Autopilot` se ele existir, senão direto para o
`AirVehicle`.** Esse `if (ap != nullptr) … else …` aparece para *stick* de rolagem,
*stick* de arfagem e manete. Motivo: o `Autopilot` precisa saber que o piloto assumiu.

**PADRÃO** — `mixr::base::alim(valor, limite)` satura entradas analógicas. A manete tem
tratamento próprio (faixa 0..2, para incluir pós-combustão):

```cpp
   inData->getAnalogInput(THROTTLE_AI, &value);
   if (value < 0.0f)       value = 0.0f;
   else if (value > 2.0f)  value = 2.0f;
   if (ap != nullptr) ap->setThrottles(&value,1);
   else               av->setThrottles(&value,1);
```

**PADRÃO** — desengate do piloto automático (tecla *paddle*) desliga **todos** os modos:

```cpp
   const auto ap = dynamic_cast<mixr::models::Autopilot*>(av->getPilot());
   if (ap != nullptr) {
      ap->setHeadingHoldMode(false);
      ap->setAltitudeHoldMode(false);
      ap->setVelocityHoldMode(false);
      ap->setLoiterMode(false);
      ap->setNavMode(false);
   }
```

**PADRÃO** — navegar até a rota para trocar de *steerpoint*, com `ref()`/`unref()` a cada
salto:

```cpp
   mixr::models::Navigation* myNav{av->getNavigation()};
   if (myNav != nullptr) {
      myNav->ref();
      mixr::models::Route* myRoute{myNav->getPriRoute()};
      if (myRoute != nullptr) {
         myRoute->ref();
         myRoute->incStpt();
         myRoute->unref();
      }
   }
```

**ARMADILHA** — o `myNav->ref()` deste trecho oficial **não tem `unref()` correspondente**:
vazamento de referência a cada acionamento da chave. Erro real em
`mainSim2/SimIoHandler.cpp` e `mainCockpit/TestIoHandler.cpp` (o mesmo código copiado).
Não replique.

**ARMADILHA** — no mesmo arquivo, o bloco de *pickle* declara
`mixr::base::Boolean sw(sw);` — uma variável que **sombreia** o `bool sw` externo e se
inicializa consigo mesma. Compila, mas é comportamento indefinido. Também presente nos
dois arquivos.

---

# 10. PADRÕES DE IA E COMPORTAMENTO

O framework oferece **duas** arquiteturas nativas de decisão. Os exemplos exercitam ambas,
e as duas são alternativas ao BehaviorTree.CPP adotado nesta PoC.

## 10.1 PADRÃO: UBF (*Unified Behavior Framework*)

**Arquivos**: `shared/xbehaviors/` (15 classes), `mainUbf1/configs/players/parts/agents.epp`.

Quatro abstrações, todas em `mixr::base::ubf` (ver `MIXR-CONTEXT.md` §14):

| Abstração | Papel | Exemplo concreto |
|---|---|---|
| `AbstractState` | fotografia do mundo, atualizada por quadro | `PlaneState` |
| `AbstractBehavior` | gera uma ação candidata **com um voto** | `PlaneFlyStraight`, `PlaneFire`, … (12) |
| `Arbiter` | escolhe/combina as ações candidatas | `PriorityArbiter` |
| `AbstractAction` | a ação, que sabe se **executar** no ator | `PlaneAction` |

### O `State`: traduzir o *player* em fatos

```cpp
// shared/xbehaviors/PlaneState.cpp
void PlaneState::updateState(const base::Component* const actor)
{
   const auto airVehicle = dynamic_cast<const models::AirVehicle*>(actor);
   setAlive(false);
   if (airVehicle != nullptr && airVehicle->isActive()) {
      setAltitude(airVehicle->getAltitude());
      setAlive(airVehicle->getMode() == models::Player::ACTIVE);
      setHeading(airVehicle->getHeading());
      setPitch(airVehicle->getPitch());
      setRoll(airVehicle->getRoll());
      base::Vec3d angularVels = airVehicle->getAngularVelocities();
      setRollRate(angularVels.x());  setPitchRate(angularVels.y());  setYawRate(angularVels.z());
      setSpeed(airVehicle->getCalibratedAirspeed());
      setNumEngines(airVehicle->getNumberOfEngines());
      // ... e as pistas do radar:
      const base::Pair* sensorPair{airVehicleX->getSensorByType(typeid(models::Radar))};
      if (sensorPair != nullptr) {
         const auto radar = static_cast<const models::Radar*>(sensorPair->object());
         const models::TrackManager* trackManager{radar->getTrackManager()};
         base::safe_ptr<models::Track> trackList[50];
         int nTracks{trackManager->getTrackList(trackList, 50)};
         for (int i = nTracks-1; i >= 0; i--) {
            setHeadingToTracked(i,  trackList[i]->getRelAzimuth());
            setPitchToTracked(i,    trackList[i]->getElevation());
            setDistanceToTracked(i, trackList[i]->getRange());
            if (getTargetTrack()==MAX_TRACKS && trackList[i]->getShootListIndex() == 1
                && trackList[i]->getTarget()->isActive()) {
               setTargetTrack(i);
            }
            setTracking(true);
            setNumTracks(nTracks);
         }
      }
   }
}
```

**REGRA** — o `State` é a **única** camada que toca no `Player`. Comportamentos leem
apenas o `State`; nunca o modelo. É o que os torna testáveis e reutilizáveis.

**ARMADILHA** — `setTargetTrack(MAX_TRACKS)` é o sentinela de "sem alvo", porque `0` é
índice válido. Comentado no próprio código: `// 0 is a valid target track, use MAX_TRACKS
to signal "no tgt track"`.

**ARMADILHA** — `PlaneState.cpp` faz `const_cast<models::AirVehicle*>(airVehicle)` com o
comentário `// DH - DOES NOT COMPILE WITH CONST -- ????`, porque `getSensorByType()` não
tem sobrecarga `const`. Limitação real da API.

### O `Behavior`: uma ação candidata, com voto

```cpp
// shared/xbehaviors/PlaneBehaviors.cpp
base::ubf::AbstractAction* PlaneFire::genAction(const base::ubf::AbstractState* const state,
                                                const double dt)
{
   PlaneAction* action{};
   const auto pState = dynamic_cast<const PlaneState*>(state->getUbfStateByType(typeid(PlaneState)));

   if (pState != nullptr && pState->isAlive() && pState->isTracking()
       && pState->getTargetTrack() < PlaneState::MAX_TRACKS) {
      if (!pState->isMissileFired()
          && pState->getDistanceToTracked(pState->getTargetTrack()) < maxDistance) {
         action = new PlaneAction();
         action->setFireMissile(true);
         action->setVote(getVote());
      }
   }
   return action;                      // nullptr = "não tenho opinião agora"
}
```

**REGRA** — `genAction()` devolve `nullptr` quando o comportamento não se aplica. Devolver
uma ação com voto 0 **não** é o padrão.

**PADRÃO** — voto **contextual**: o mesmo comportamento vota mais alto em emergência.

```cpp
// PlaneFlyStraight::genAction() — final
   if (voteOnCriticalAltitude != 0 && pState->getAltitude() < criticalAltitude)
      action->setVote(voteOnCriticalAltitude);   // ex.: 100
   else
      action->setVote(getVote());                // ex.: 10
```

```
// mainUbf1/configs/players/parts/agents.epp
( PlaneFlyStraight
   vote: 10
   voteOnCriticalAltitude: 100
   criticalAltitude: ( Meters 3500 )
)
```

**PADRÃO** — hierarquia `PlaneBehavior` (abstrata, com os *slots* comuns
`criticalAltitude`/`voteOnCriticalAltitude`/`voteOnIncomingMissile`) → 12 concretos. A
abstrata usa `IMPLEMENT_ABSTRACT_SUBCLASS(PlaneBehavior, "BasePlaneBehavior")`.

### O `Arbiter`: fundir votos por campo

```cpp
// shared/xbehaviors/PriorityArbiter.cpp
base::ubf::AbstractAction* PriorityArbiter::genComplexAction(base::List* const actionSet)
{
   const auto complexAction = new PlaneAction;
   int maxPitchVote{}, maxRollVote{}, maxHeadingVote{},
       maxFireVote{}, maxThrottleVote{}, maxPitchTrimVote{};

   const base::List::Item* item{actionSet->getFirstItem()};
   while (item != nullptr) {
      const auto action = dynamic_cast<const PlaneAction*>(item->getValue());
      if (action != nullptr) {
         if (action->isHeadingChanged() && action->getVote() > maxHeadingVote) {
            complexAction->setHeading(action->getHeading());
            maxHeadingVote = action->getVote();
         }
         if (action->isPitchChanged() && action->getVote() > maxPitchVote) { /* ... */ }
         if (action->isRollChanged()  && action->getVote() > maxRollVote)  { /* ... */ }
         // ... e assim para fire, throttle, pitchTrim
      }
      item = item->getNext();
   }
   // ...
}
```

**REGRA — a decisão é POR CAMPO, não por ação.** A ação resultante é uma **colagem**: o
*pitch* pode vir de um comportamento e o *roll* de outro, cada qual o de maior voto para
aquele campo. É isso que distingue o UBF de uma máquina de estados ou de uma árvore de
comportamento, onde um único ramo vence de uma vez.

**REGRA** — o par `campo` + `campoChanged` no `Action` é o que torna a fusão possível:

```cpp
// shared/xbehaviors/PlaneAction.hpp
   double pitch{};        bool pitchChanged{};
   double roll{};         bool rollChanged{};
   double heading{};      bool headingChanged{};
   double throttle{};     bool throttleChanged{};
   bool fireMissile{};    bool fireMissileChanged{};
```
```cpp
void PlaneAction::setPitch(const double x) { pitch = x; pitchChanged = true; }
```

Sem o *flag*, o árbitro não distinguiria "quer *pitch* zero" de "não opinou sobre *pitch*".

### O `Action`: executar no ator

```cpp
// shared/xbehaviors/PlaneAction.cpp
bool PlaneAction::execute(base::Component* actor)
{
   const auto airVehicle = dynamic_cast<models::AirVehicle*>(actor);
   if (airVehicle != nullptr) {
      airVehicle->setControlStick(getRoll(), getPitch());

      double throttles[8]{};
      for (int i = 0 ; i < airVehicle->getNumberOfEngines() ; i++) throttles[i] = getThrottle();
      if (isThrottleChanged())  airVehicle->setThrottles(throttles, 2);
      if (isPitchTrimChanged()) airVehicle->setTrimSwitch(0, getPitchTrim());

      if (getFireMissile()) {
         models::StoresMgr* sms{airVehicle->getStoresManagement()};
         if (sms != nullptr) {
            sms->setWeaponDeliveryMode(models::StoresMgr::A2A);
            airVehicle->event(base::Component::WPN_REL_EVENT);
         }
      }
      return true;
   }
   return false;
}
```

**ARMADILHA** — o vetor `throttles` é preenchido para `getNumberOfEngines()` motores, mas
passado com **contagem fixa `2`**: `setThrottles(throttles, 2)`. Erro real; num avião de
1 ou 4 motores, o comando fica errado.

### A configuração

```
// mainUbf1/configs/players/parts/agents.epp
#define UBF_AGENT1()                                         \
( UbfAgent                                                   \
   state: ( PlaneState )                                     \
   behavior: ( PriorityArbiter                               \
      behaviors: {                                           \
         ( PlaneFlyStraight vote: 10                         \
              voteOnCriticalAltitude: 100                    \
              criticalAltitude: ( Meters 3500 ) )            \
         ( PlaneFire vote: 50 )                              \
         ( PlaneFollowEnemy vote: 50 )                       \
         ( PlaneTurn vote: 15 )                              \
         ( PlaneTrim vote: 100 )                             \
      }                                                      \
   )                                                         \
)
```
```
// mainUbf1/configs/players/player01.epp
   components: {
      dynamicsModel: JSBSIM_DYNAMICS("f16")
      ubf1: UBF_AGENT1()                  // <<< o agente é só mais um componente do player
      pilot: ( Autopilot ... )
      ...
   }
```

**REGRA** — o `UbfAgent` (nome de fábrica `"UbfAgent"`) entra como **componente comum do
`Player`**. O `Arbiter` é ele próprio um `AbstractBehavior`, então árbitros podem
aninhar-se recursivamente.

**PADRÃO** — `UBF_AGENT2()` no mesmo arquivo omite o *slot* `state:`, demonstrando que o
agente pode herdar o estado de um agente-pai.

**NESTA POC** — o UBF é a alternativa nativa ao BehaviorTree.CPP usado em `poc/02` e
`poc/03`. Comparação honesta:

| | UBF (nativo) | BehaviorTree.CPP (`poc/02`, `poc/03`) |
|---|---|---|
| Composição | fusão **por campo**, ponderada por voto | seleção de **um ramo** (Fallback/Sequence) |
| Configuração | EDL, junto do cenário | XML separado (`mission.xml`) |
| Estado compartilhado | `AbstractState` tipado | *blackboard* por chave |
| Dependência | nenhuma (já está no `mixr_base`) | pacote Conan extra |
| Ferramental | nenhum | Groot, logs, testes da lib |
| Disponível no pacote desta PoC | **sim** (`mixr/base/ubf/`) | sim |

O UBF é competitivo e **não exige dependência externa**. A escolha por BT.CPP nesta PoC
é justificável por ferramental e legibilidade da árvore, não por ausência de alternativa
— e vale registrar que a alternativa nativa existe e está disponível.

## 10.2 PADRÃO: `StateMachine`

**Arquivos**: `testStateMach/` (4 máquinas + 3 submáquinas).

```cpp
// testStateMach/TestStateMachine04.cpp — a tabela de despacho
BEGIN_STATE_TABLE(TestStateMachine04)
   STATE_FUNC( INIT_STATE, stateFunc00)
   STATE_MACH( 1, "s01")             // <<< o estado 1 é uma SUBMÁQUINA
   STATE_FUNC( 2, stateFunc02)
   STATE_FUNC( 3, stateFunc03)
   STATE_MACH( 4, "s04")
   STATE_FUNC( 5, stateFunc05)
   // ...
   STATE_FUNC(99, stateFunc99)
END_STATE_TABLE()
```
```
// testStateMach/test4.edl — as submáquinas são componentes nomeados
( TestStateMachine04
   stateMachines: {
      s01: ( TestStateMachine04A )
      s04: ( TestStateMachine04B )
      s13: ( TestStateMachine04C )
   }
)
```

**REGRA** — `STATE_MACH(n, "nome")` delega o estado `n` a uma submáquina declarada no
*slot* `stateMachines:`. **A composição hierárquica é declarada em EDL, não em C++** —
trocar a submáquina do estado 1 não recompila nada.

**PADRÃO** — a alternativa de tabela única com `switch`, para máquinas pequenas:

```cpp
// testStateMach/TestStateMachine01.cpp
BEGIN_STATE_TABLE(TestStateMachine01)
   ANY_STATE_FUNC( anyStateFunc)
END_STATE_TABLE()

void TestStateMachine01::anyStateFunc(const double)
{
   switch (getState()) {
      case INIT_STATE : { next(); break; }
      case 1 :  { next(); break; }
      case 2 :  { goTo(4); break; }
      // ...
   }
}
```

**PADRÃO** — as quatro transições e o *call/return* com argumento:

```cpp
      case 4 : {
         if (getMode() != Mode::RTN_STATE) {
            call(CALL_01);                          // chama a "sub-rotina" no estado 11
         } else {
            /* voltamos da chamada */  next();
         }
         break;
      }
      case 13 : {
         if (getMode() != Mode::RTN_STATE) {
            const auto arg = new base::Integer(13);
            call(CALL_02, arg);                     // com argumento
            arg->unref();
         } else {
            const auto arg = dynamic_cast<const base::Boolean*>( getArgument() );
            if (arg != nullptr) std::cout << "arg(" << arg->getBoolean() << "); ";
            next();
         }
         break;
      }
      case 23 : {
         const auto arg = new base::Boolean(true);
         rtn(arg);                                   // retorna com valor
         arg->unref();
         break;
      }
```

**REGRA** — `next()` (próximo estado), `goTo(n)` (salto), `call(n[, arg])` (empilha e
salta, com retorno), `rtn([arg])` (desempilha). O estado detecta que **voltou** de uma
chamada testando `getMode() != Mode::RTN_STATE`. Argumentos são objetos MIXR
(`base::Integer`, `base::Boolean`) com `unref()` após a chamada.

**PADRÃO** — `preStateProc()`/`postStateProc()` para instrumentação por quadro:

```cpp
void TestStateMachine04::preStateProc(const double)
{
   std::cout << "Test #4 State(" << getState() << "," << getSubstate() << "): ";
}
void TestStateMachine04::postStateProc(const double) { std::cout << std::endl; }
```

**PADRÃO** — o laço de teste usa o **estado 99 como terminal**:

```cpp
// testStateMach/main.cpp
void theTest(mixr::base::StateMachine* stateMachine)
{
   const double dt{0.05};                       // dt fictício
   while (stateMachine->getState() != 99) {
      mixr::base::Timer::updateTimers(dt);
      stateMachine->updateTC(dt);
      stateMachine->updateData(dt);
   }
}
```

**PADRÃO** — o exemplo mantém a **saída esperada versionada** em
`testStateMach/results/test1..4.txt`. É o único exemplo da árvore com teste de regressão
de fato — padrão que vale copiar.

## 10.3 PADRÃO: busca em espaço de estados (`mainPuzzle1/2`)

Fora do domínio de simulação, mas mostra que `Component` serve de infraestrutura genérica:

```cpp
// mainPuzzle1/Controller.hpp
class Controller final: public mixr::base::Component
{
   static const int MAX_STATES{1000000};
   static const int MAX_REHASH{20};

   virtual const State* solve();                          // A*
   virtual void printPath(const State* tstate) const;
   virtual bool putHash(const State* const s);
   virtual void putOpen(State* const s);
protected:
   virtual State* getOpen();
   virtual void removeOpen(const State* const s);
private:
   State*       initState{};                              // slot
   const State* goalState{};                              // slot
   mixr::base::List* openStates{};                        // fila ordenada por f()
   std::array<const State*, MAX_STATES> hashTable{};
};
```

```cpp
// mainPuzzle1/State.hpp
   virtual int f() const  { return (g() + h()); }
   virtual int g() const;
   virtual int h() const;
   virtual int hash(int rh, int max) const;
   virtual const State* expand(const State* const goal, Controller* const);
```

E o estado inicial/objetivo vêm do EDL:

```
// mainPuzzle1/configs/puzzle.epp
engine: ( Controller
    initState: ( PuzzleState blocks: {
            ( Block2x2 position: [ 2 4 ] id: 1 )
            ( Block1x1 position: [ 1 1 ] id: 2 )
            ...
    } )
    goalState: ( PuzzleState blocks: { ( Block2x2 position: [ 2 1 ] ) } )
)
```

**POR QUÊ** — A* clássico (`f = g + h`, lista aberta, tabela hash) construído inteiramente
sobre `Component`/`Object`/`List`, com o problema descrito em EDL. Demonstra que a
infraestrutura do `base` não é específica de simulação.

---

# 11. PADRÕES DE GRÁFICOS (referência — indisponível neste fork)

> **ARMADILHA (repetida de §0.2)** — `mixr_graphics`, `mixr_ui_glut` e
> `mixr_instruments` **não existem** no pacote `mixr/1.0.5` desta PoC. Esta seção é
> referência conceitual e material para eventual reimplementação; nada aqui compila aqui.

## 11.1 PADRÃO: `Display` → `Page` → `Graphic` e a paginação

```
// tutorial07/file0.edl
( GlutDisplay
  fullScreen: false
  name: "Glut Main Window"
  left: -10.5   right: 10.5   bottom: -10.5   top: 10.5     // coordenadas lógicas
  vpX: 100  vpY: 200  vpWidth: 600  vpHeight: 400           // pixels
  antiAliasing: true

  page: p1                                                  // página inicial
  pages: {
    p1: ( Page components: {
            ( Worm selectName: 111 color: green speed: 10 startAngle: ( Degrees 30 ) )
            ( Worm selectName: 112 color: blue  speed: 5  startAngle: ( Degrees 155 ) )
          } )
  }
)
```

**REGRA** — o `Display` separa **coordenadas lógicas** (`left/right/bottom/top`, o
ortho do OpenGL) de **coordenadas físicas** (`vpX/vpY/vpWidth/vpHeight`, pixels). O
desenho é escrito no espaço lógico e independe da resolução.

**PADRÃO — paginação por evento de tecla**:

```
// tutorial08/configs/file0.epp
    p1: ( MyPager
          pagingEvent: { n: p2   p: p2 }      // tecla 'n' e 'p' levam a p2
          components: { ... } )
    p2: ( MyPager
          pagingEvent: { n: p1   p: p1 }
          components: { ... } )
```
```
// demoInstruments/configs/testadi.epp — cadeia circular de páginas de teste
testadi: ( TestAdi
    pagingEvent: { n: testspeedbrake   p: testadi2 }
    components: { ... #include "mixr/instruments/adi/adi.epp" }
)
```

**PADRÃO — `onEntry()`** para reinicializar tudo ao entrar na página:

```cpp
// tutorial08/MyPager.cpp
bool MyPager::onEntry()
{
   mixr::base::PairStream* components{getComponents()};
   if (components != nullptr) {
      mixr::base::List::Item* item{components->getFirstItem()};
      while (item != nullptr) {
         const auto pair = static_cast<mixr::base::Pair*>(item->getValue());
         const auto cp = static_cast<mixr::base::Component*>(pair->object());
         if (cp != nullptr) cp->event(RESET_EVENT);
         item = item->getNext();
      }
   }
   return true;
}
```

**PADRÃO — `Graphic` próprio**: sobrescrever `drawFunc()` (OpenGL puro) + `updateTC()`
(movimento, tempo crítico) + `updateData()` (histórico, tempo de fundo):

```cpp
// tutorial07/Worm.cpp
void Worm::updateTC(const double dt)       // posição: tempo crítico
{
   BaseClass::updateTC(dt);
   xPos += dx*dt;
   if (xPos > right) { xPos = right - (xPos - right); dx = -dx; }
   // ...
}

void Worm::updateData(const double dt)     // rastro: tempo de fundo
{
   BaseClass::updateData(dt);
   if (nTrails == 0 || xPos != xOld || yPos != yOld) {
      trail[index++].set(xPos, yPos);
      if (index >= MAX_HIST) index = 0;
      if (nTrails < MAX_HIST) nTrails++;
      xOld = xPos;  yOld = yPos;
   }
}

void Worm::drawFunc()                      // só desenha o que já foi calculado
{
   glBegin(GL_LINE_STRIP);
   int idx{index - 1};
   for (int i = 0; i < nTrails; i++) {
      if (idx < 0) idx = MAX_HIST - 1;
      lcVertex2v(trail[idx--].ptr());
   }
   glEnd();
}
```

**REGRA** — `drawFunc()` **não calcula**: apenas desenha estado já pronto. A separação
cálculo/desenho é o que permite que o desenho rode a taxa diferente da física.

**PADRÃO** — recursos do display declarados uma vez e referenciados por nome:
`colorTable:` (cores), `fonts:` + `normalFont:`, `textures:`, `materials:`. Depois, um
componente escreve `color: green`, `texture: tex1`, `material: m1`.

## 11.2 PADRÃO: mapa com símbolos (`SymbolLoader`)

**Arquivo**: `mainSim3/MapPage.cpp`.

```cpp
void MapPage::updateData(const double dt)
{
    BaseClass::updateData(dt);

    // (1) cachear loader e station (padrão do §8.3)
    if (loader == nullptr) { /* findByType(typeid(SymbolLoader)) */ }
    if (stn == nullptr)    { /* findContainerByType(typeid(Station)) + refLat/refLon */ }

    // (2) montar a lista nova de players
    // (3) casar nova × antiga: remover símbolos de quem saiu, criar de quem entrou
    // (4) atualizar posição dos que permaneceram
}
```

**PADRÃO — reconciliação de listas por quadro**: monta `newPlayers[]`, percorre a lista
antiga marcando correspondências (anulando o item novo casado), remove símbolos sem
correspondência (`loader->removeSymbol(playerIdx[i])`), e no fim adiciona os novos
remanescentes. É o padrão para manter símbolos sincronizados com uma lista dinâmica.

**PADRÃO** — a referência geodésica do mapa vem do `WorldModel`:

```cpp
const auto sim = dynamic_cast<mixr::models::WorldModel*>(stn->getSimulation());
if (sim != nullptr) {
    setReferenceLatDeg(sim->getRefLatitude());
    setReferenceLonDeg(sim->getRefLongitude());
}
```

## 11.3 PADRÃO: painel de instrumentos

**Arquivos**: `mainSim2/InstrumentPanel.cpp`, `shared/xpanel/`,
`demoInstruments/test_pages/`.

O padrão é sempre o mesmo triplo:

```cpp
// 1) membro + SendData por grandeza
   double altitude{};   SendData altitudeSD;
   double heading{};    SendData headingSD;
   double gForce{};     SendData gForceSD;   SendData gForce2SD;  // dois destinos

// 2) ler do ownship em updateData()
   const auto av = dynamic_cast<mixr::models::AirVehicle*>( getOwnship() );
   if (av != nullptr) {
      av->ref();
      gForce   = av->getGload();
      aoa      = av->getAngleOfAttackD();
      airSpeed = av->getCalibratedAirspeed();
      heading  = av->getHeadingD();
      altitude = av->getAltitudeFt();
      vvi      = av->getVelocity();
      slip     = av->getSideSlipD();
      pitch    = av->getPitchD();
      roll     = av->getRollD();
      mach     = av->getMach();
      av->unref();
   }

// 3) enviar por nome (§8.1)
   send("altitude", UPDATE_INSTRUMENTS, altitude, altitudeSD);
```

**PADRÃO** — os *getters* do `Player`/`AirVehicle` já vêm em unidade de apresentação:
`getHeadingD()` (graus), `getAltitudeFt()` (pés), `getTotalVelocityKts()` (nós),
`getPitchD()`, `getRollD()`, `getAngleOfAttackD()`, `getSideSlipD()`. Sufixo `D` = graus,
`Ft` = pés, `Kts` = nós, `M` = metros. **Use-os em vez de converter manualmente.**

**PADRÃO** — páginas de teste sem simulação: cada `TestXxx` de `demoInstruments` é uma
`Page` que varre a grandeza de ponta a ponta em `updateData()`, invertendo a taxa nos
limites:

```cpp
// demoInstruments/test_pages/TestAdi.cpp
void TestAdi::updateData(const double dt)
{
    BaseClass::updateData(dt);
    pitch += (pitchRate * dt);
    if (pitch >  90) { pitch =  90; pitchRate = -pitchRate; }
    if (pitch < -90) { pitch = -90; pitchRate = -pitchRate; }
    // ... idem roll e slip ...
    send("sideslip", UPDATE_INSTRUMENTS, slip,  slipSD);
    send("adi",      UPDATE_INSTRUMENTS, pitch, pitchSD);
    send("adi",      UPDATE_VALUE,       roll,  rollSD);
}
```

**POR QUÊ** — permite validar o instrumento **sem** simulação nenhuma. Padrão excelente,
transponível: um "gerador de telemetria sintética" para exercitar um consumidor
(analogamente, um alimentador sintético do exportador Tacview desta PoC).

**PADRÃO** — a biblioteca `xpanel` traz componentes que se **ligam a um objeto do modelo**:

```cpp
// mainCockpit/TestDisplay.cpp
   rdrDisplay->setRadar(rdr);      // o widget passa a ler daquele Radar
   rwrDisplay->setRwr(rwr);
```

---

# 12. PADRÕES DE GRAVAÇÃO E ANÁLISE (`recorder`)

Disponível neste fork (`libmixr_recorder.so`), e o mecanismo nativo usado por
`poc/05-formation-flight`.

## 12.1 PADRÃO: anexar um gravador à simulação

```
// testRecordData/configs/dataRecorder.epp
#include "mixr/simulation/dataRecorderTokens.hpp"

dataRecorder: ( XDataRecorder
   outputHandler: ( RecorderOutputHandler
      components: {
         ( RecorderFileWriter filename: MIXR_DATA_RECORDER_FILE )   // binário
#if 0
         ( RecorderNetOutput netHandler: ( UdpUnicastHandler
               localIpAddress: localhost ipAddress: "127.0.0.1"
               port: 4950 localPort: 4960 shared: true ) )
#endif
         ( TabPrinter msgHdrOptn: NEW_MSG filename: "./zTabPrinter.csv" )   // CSV
         ( PrintPlayer playerName: "duck11" )                               // console
         ( PrintSelected
            filename: "./zPrintSelected2.csv"
            messageToken: REID_PLAYER_DATA
            fieldName:   "mixr.Recorder.Pb.PlayerId.name"
            compareToValS: "duck12" )
      }
   )
)
```

**REGRA** — `dataRecorder:` é *slot* da **`Station`**. O `RecorderOutputHandler` é um
**multiplexador**: todos os `components:` recebem **cada** registro. Ligar/desligar uma
saída é acrescentar/remover um componente.

**PADRÃO** — as quatro saídas nativas cobrem os quatro usos:

| Handler | Uso |
|---|---|
| `RecorderFileWriter` | arquivo binário protobuf, para pós-processamento |
| `RecorderNetOutput` | stream ao vivo para outro processo |
| `TabPrinter` | CSV completo; aceita `enabledList:` / `disabledList:` por token |
| `PrintPlayer` / `PrintSelected` | filtro por *player* ou por campo, para depuração |

**PADRÃO** — filtro por campo com nome **totalmente qualificado do protobuf**:
`fieldName: "mixr.Recorder.Pb.PlayerId.name"` + `compareToValS: "duck12"`.

**PADRÃO** — taxa de gravação **por *player***, via *slot* do próprio `Player`:

```
// mainCockpit/configs/player01.epp
    dataLogTime: ( Seconds 0.1 )      // test recording at 10hz
    // e nos mísseis:
               dataLogTime: ( Seconds 0.2 ) // record missile at 5hz
```

**NESTA POC** — `poc/05-formation-flight` usa exatamente esse arranjo
(`RecorderFileWriter` + `TabPrinter` gerando `mission.dat`/`mission.csv`), incluído por
`dataRecorder: #include "recorder.epp"` — e foi ali que surgiu a armadilha de rótulo
duplicado do §5.4.

## 12.2 PADRÃO: ler uma gravação

```
// testRecorderRead/configs/test.epp
#include "mixr/simulation/dataRecorderTokens.hpp"

( DataRecordTest
   inputHandler: ( RecorderFileReader filename: MIXR_DATA_RECORDER_FILE )
   // ou: ( RecorderNetInput netHandler: ( UdpUnicastHandler ... ) )

   outputHandler: ( RecorderOutputHandler components: {
         ( TabPrinter  msgHdrOptn: NEW_MSG  filename: "./zTabPrinter2.csv" )
         ( TabPrinter  disabledList: [ REID_PLAYER_DATA ]
                       msgHdrOptn: NEW_MSG  filename: "./zTabPrinter2a.csv" )
         ( PrintPlayer playerName: "duck11" filename: "./zPrintPlayer2.csv" )
         ( PrintSelected ... )
         ( PrintMyData )                    // <<< handler próprio
   } )
)
```

```cpp
// testRecorderRead/DataRecordTest.cpp — o laço de leitura inteiro
void DataRecordTest::runTest()
{
   if (inputHandler == nullptr)  { std::cerr << "runTest() -- missing input handler!\n";  return; }
   if (outputHandler == nullptr) { std::cerr << "runTest() -- missing output handler!\n"; return; }

   bool finished{};
   while (!finished) {
      const mixr::recorder::DataRecordHandle* p{inputHandler->readRecord()};
      if (p != nullptr) outputHandler->processRecord(p);
      finished = (p == nullptr) || (p->getRecord()->id() == REID_END_OF_DATA);
   }
}
```

**PADRÃO** — o programa de leitura **reaproveita os mesmos `OutputHandler`** do programa
de gravação. Ler um arquivo e gravá-lo ao vivo são a mesma operação com uma fonte
diferente.

**PADRÃO — `PrintHandler` próprio**:

```cpp
class PrintMyData final: public mixr::recorder::PrintHandler
{
   DECLARE_SUBCLASS(PrintMyData, mixr::recorder::PrintHandler)
protected:
   void processRecordImp(const mixr::recorder::DataRecordHandle* const handle) final;
};

void PrintMyData::processRecordImp(const mixr::recorder::DataRecordHandle* const handle)
{
   if (handle == nullptr) return;
   const mixr::recorder::pb::DataRecord* dataRecord{handle->getRecord()};
   if (dataRecord == nullptr) return;

   switch (dataRecord->id()) {
      case REID_MY_DATA_EVENT : {
         if (dataRecord->HasExtension( mixr::xrecorder::pb::my_data_msg )) {
            std::stringstream sout;
            const auto* msg = &dataRecord->GetExtension( mixr::xrecorder::pb::my_data_msg );
            if (msg->has_fee()) sout << "fee= " << msg->fee() << ";  ";
            printToOutput( sout.str().c_str() );
         }
         break;
      }
   }
}
```

**REGRA** — sobrescreva **`processRecordImp`** (não `processRecord`), monte a linha num
`std::stringstream` e emita por `printToOutput()`. O `PrintHandler` base cuida de
arquivo vs. console conforme o *slot* `filename:`.

## 12.3 PADRÃO: eventos de gravação próprios

Recapitulando §7.6, com o código completo do *handler*:

```cpp
// shared/xrecorder/DataRecorder.cpp
BEGIN_RECORDER_HANDLER_TABLE(DataRecorder)
   ON_RECORDER_EVENT_ID( REID_MY_DATA_EVENT, recordMyData )
END_RECORDER_HANDLER_TABLE()

bool DataRecorder::recordMyData(const base::Object* objs[4], const double values[4])
{
   const auto msg = new recorder::pb::DataRecord();
   timeStamp(msg);                                   // (1) cabeçalho de tempo
   msg->set_id( REID_MY_DATA_EVENT );                // (2) id do evento

   pb::MyDataMsg* myDataMsg = msg->MutableExtension( pb::my_data_msg );   // (3) payload
   myDataMsg->set_fee( static_cast<unsigned int>(base::nintd(values[0])) );
   myDataMsg->set_fi(  static_cast<unsigned int>(base::nintd(values[1])) );
   myDataMsg->set_fo(  static_cast<unsigned int>(base::nintd(values[2])) );

   sendDataRecord(msg);                              // (4) despacha
   return true;
}
```

**REGRA** — a assinatura de todo *handler* é
`(const base::Object* objs[4], const double values[4])`: **quatro** objetos e **quatro**
valores, fixos. É o contrato das macros `SAMPLE`. Documentado em
`shared/xrecorder/dataRecorderTokens.hpp`:

```
//    2) P1 to P4 are the required objects passed to the SAMPLE macro, and
//       V1 to V4 are the required values passed to the SAMPLE macros.
```

**PADRÃO** — sobrescrever um *handler* nativo para **estender** o registro existente
(`recordMarker` ganha o campo `foo` via `markerMsg->SetExtension( pb::foo, ... )`), em
vez de criar um evento novo. Preserva compatibilidade com leitores existentes.

---

# 13. PADRÕES DE REDE

`testNetHandler` é o catálogo: 11 arquivos `.edl`, um por transporte/papel.

```
configs/
├── senderUdpUnicast.edl     echoUdpUnicast.edl
├── senderUdpBroadcast.edl   echoUdpBroadcast.edl
├── senderUdpMulticast.edl   echoUdpMulticast.edl
├── senderTcpClient.edl      echoTcpSingle.edl
│                            echoTcpMultiple.edl
└── senderReqZeroMQ.edl      echoRepZeroMQ.edl
```

## 13.1 PADRÃO: `Endpoint` — a base comum de quem fala rede

```cpp
// testNetHandler/Endpoint.hpp
class Endpoint : public mixr::base::Component
{
    static const unsigned int MAX_SIZE{1024};
    bool areNetworksEnabled() const;
    bool initNetworks();
    bool sendData(const char* const msg, const unsigned int size);
    int  recvData(char* const msg, const unsigned int maxsize);
    void reset() override;
protected:
    void closeConnections();
    int getLoops() const { return loops; }
private:
    mixr::base::safe_ptr<mixr::base::NetHandler> netHandler;  // I/O, ou só O se netInput existir
    mixr::base::safe_ptr<mixr::base::NetHandler> netInput;    // entrada opcional
    int  loops{};
    bool networkInitialized{};
    bool networkInitFailed{};
    bool noWaitFlag{};
};
```

**PADRÃO — três *slots* para dois papéis**: `netHandler` (entrada+saída, ou só saída),
`netInput` (entrada dedicada), `netOutput` (**alias** de `netHandler`). O arquivo de
configuração escolhe o arranjo:

```
// testNetHandler/configs/senderUdpUnicast.edl — os três arranjos, dois comentados
( Sender
    noWait: false
    // Single handler test
    //netHandler: ( UdpUnicastHandler ipAddress: "172.17.160.92" port: 3001 localPort: 3002 )

    // Double handler test
    //netInput:   ( UdpUnicastHandler port: 3002 shared: true )
    //netOutput:  ( UdpUnicastHandler ipAddress: "172.17.160.92" port: 3001 localPort: 3002 shared: true )

    // Single handler (localhost) test
    netHandler: ( UdpUnicastHandler
                     localIpAddress: localhost
                     ipAddress:      localhost
                     port:           3001
                     localPort:      3002 )
)
```

**PADRÃO** — inicialização **preguiçosa e à prova de repetição**: os *flags*
`networkInitialized`/`networkInitFailed` garantem que `initNetworks()` tente uma vez e
não fique repetindo em caso de falha. `reset()` é quem dispara.

**PADRÃO** — o laço de aplicação é `updateData()`, com máquina de dois estados
(`recvMode`):

```cpp
// testNetHandler/Sender.cpp
void Sender::updateData(const double dt)
{
    BaseClass::updateData(dt);
    if (areNetworksEnabled()) {
        if (recvMode) {                          // esperando resposta
            char buffer[MAX_SIZE+1]{};
            int n{recvData(buffer, MAX_SIZE)};
            if (n > 0) { buffer[n] = 0; std::cout << "RECV: " << buffer << "\n"; recvMode = false; }
        } else {                                 // enviando
            char buffer[MAX_SIZE]{};
            sprintf(buffer, "Message(%d)", ++msgCounter);
            base::msleep(1000);
            if (sendData(buffer, std::strlen(buffer))) { std::cout << "SENT: ...\n"; recvMode = true; }
        }
        if (!recvMode && getLoops() > 0 && msgCounter >= getLoops()) {
            closeConnections();
            std::exit(0);
        }
    }
}
```

**ARMADILHA** — `base::msleep(1000)` **dentro** de `updateData()` bloqueia a *thread* de
fundo por 1 s por quadro. Aceitável num teste de eco; **inaceitável** numa simulação. Se
você copiar este arquivo, remova o `msleep`.

## 13.2 PADRÃO: os handlers de socket disponíveis

| Nome de fábrica | Papel |
|---|---|
| `UdpUnicastHandler` | ponto a ponto |
| `UdpBroadcastHandler` | difusão na sub-rede (usa `networkMask:`) |
| `UdpMulticastHandler` | grupo multicast |
| `TcpClient` | cliente TCP |
| `TcpServerSingle` | servidor TCP, uma conexão |
| `TcpServerMultiple` | servidor TCP, várias conexões |
| `ZeroMQHandler` (exemplo) | REQ/REP/PUB/SUB/… via ZeroMQ |

*Slots* comuns (de `PosixHandler`): `localIpAddress`, `ipAddress`, `port`, `localPort`,
`shared`, `sendBuffSizeKb`, `recvBuffSizeKb`, `ignoreSourcePort`, `networkMask`.

```
// testNetHandler/configs/echoTcpMultiple.edl — na íntegra
( Echo
    noWait: false
    netHandler: ( TcpServerMultiple port: 3000 )
)
```

**REGRA** — `localIpAddress: localhost` aceita o identificador `localhost` sem aspas
(é um `Identifier` do EDL), mas endereços numéricos vão entre aspas (`"127.0.0.1"`).
Ambas as formas aparecem na árvore.

**NESTA POC** — nenhuma poc usa `NetHandler`; o servidor Tacview é socket POSIX direto
(§7.7). A armadilha de rede documentada no `CLAUDE.md` — **bind em `0.0.0.0` e não em
`127.0.0.1`**, por causa do encaminhamento WSL2↔Windows — vale igualmente caso alguma
poc passe a usar `UdpUnicastHandler`: o *slot* correspondente é `localIpAddress`.

---

# 14. PADRÕES DE BUILD

## 14.1 PADRÃO: variáveis de ambiente como raiz de tudo

```sh
# examples/setenv.sh — condensado
MIXR_ROOT=$PWD/../mixr;                   export MIXR_ROOT
MIXR_EXAMPLES_ROOT=$PWD;                  export MIXR_EXAMPLES_ROOT
MIXR_EXAMPLES_LIB_PATH=$PWD/lib;          export MIXR_EXAMPLES_LIB_PATH
MIXR_DATA_ROOT=$PWD/../mixr-data;         export MIXR_DATA_ROOT
MIXR_3RD_PARTY_ROOT=$PWD/../mixr-3rdparty; export MIXR_3RD_PARTY_ROOT
LD_LIBRARY_PATH=$MIXR_3RD_PARTY_ROOT/lib:${LD_LIBRARY_PATH}; export LD_LIBRARY_PATH
PATH=$MIXR_3RD_PARTY_ROOT/bin:$PATH;      export PATH
```

**PADRÃO** — o *layout* pressuposto é de **quatro repositórios irmãos**:

```
<workspace>/
├── mixr/            ← o framework (MIXR_ROOT)
├── mixr-examples/   ← os exemplos (MIXR_EXAMPLES_ROOT)
├── mixr-data/       ← fontes, texturas, JSBSim, terreno, DAFIF (MIXR_DATA_ROOT)
└── mixr-3rdparty/   ← CIGI, JSBSim, FTGL, OSG… (MIXR_3RD_PARTY_ROOT)
```

**ARMADILHA** — **`mixr-data` não está neste repositório.** Todo `.epp` que usa
`MIXR_DATA_*` depende dele: fontes (`arialn.ttf`), texturas (`.bmp`), JSBSim
(`f16`, `F4N`), terreno (`srtm3/*.hgt`, `dted/*`), gravações e mapas. Sem `mixr-data`, o
preprocessamento até funciona (as macros expandem para caminhos inexistentes), mas a
execução falha ao abrir os arquivos.

**NESTA POC** — a solução adotada foi **vendorizar** o pouco que se usa:
`poc/04-jsbsim-6dof/data/jsbsim/{aircraft/F4N,engine,systems}` copiado do *checkout* do
pacote Conan `jsbsim/1.1.11`, e `poc/05-formation-flight/data/terrain/srtm/*.hgt.gz`
baixado do *mirror* público AWS. `rootDir:` aponta para caminho **relativo** ao repo, não
para uma macro de ambiente. Divergência consciente do padrão upstream, e a certa: o
repositório fica autocontido.

## 14.2 PADRÃO: `makedefs` e a hierarquia de `Makefile`

```makefile
# examples/makedefs — condensado
include ${MIXR_ROOT}/src/makedefs
LDFLAGS  += -L$(MIXR_ROOT)/lib
LDFLAGS  += -L$(MIXR_EXAMPLES_LIB_PATH)
ifdef MIXR_3RD_PARTY_ROOT
   LDFLAGS += -L$(MIXR_3RD_PARTY_ROOT)/lib -L$(MIXR_3RD_PARTY_ROOT)/lib64 ...
endif
CPPFLAGS += -I$(MIXR_EXAMPLES_ROOT)/shared
EPPFLAGS  = -I$(MIXR_DATA_ROOT) ... -DMIXR_DATA_PATH=... (ver §5.2)
```

```makefile
# examples/Makefile — o orquestrador
PROJECTS = shared $(APPLICATIONS)
all: $(PROJECTS)
$(PROJECTS):
	$(MAKE) -C $@
$(APPLICATIONS): shared          # <<< toda aplicação depende de shared/
edl:
	-for d in $(PROJECTS); do (cd $$d; $(MAKE) edl ); done
clean:
	-for d in $(PROJECTS); do (cd $$d; $(MAKE) clean ); done
```

```makefile
# mainCockpit/Makefile — o Makefile de uma aplicação, na íntegra em espírito
include ../makedefs
PGM  = mainCockpit
OBJS = SimStation.o TestDisplay.o TestIoHandler.o factory.o main.o
LDFLAGS += ...            # ver §14.3
EPPFLAGS += -I.
all: edl $(PGM)
$(PGM): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)
edl:
	cpp configs/test1.epp >test1.edl $(EPPFLAGS)
clean:
	-rm -f *.o *.edl $(PGM)
```

**PADRÃO** — `all: edl $(PGM)` — **gerar o `.edl` faz parte do build**. Toda aplicação com
`.epp` tem alvo `edl` e o declara como pré-requisito de `all`.

**PADRÃO** — `EPPFLAGS += -I.` na aplicação, para que `#include "scenario.epp"` resolva
relativo ao diretório do projeto.

## 14.3 PADRÃO: ordem de ligação

```makefile
# mainCockpit/Makefile
LDFLAGS += -lmixr_interop_dis -lmixr_interop
LDFLAGS += -lmixr_models -lJSBSim
LDFLAGS += -lxzmq -l:libzmq.a
LDFLAGS += -lxpanel
LDFLAGS += -lmixr_recorder -lmixr_models
LDFLAGS += -lmixr_simulation -lmixr_instruments
LDFLAGS += -lmixr_linkage
LDFLAGS += -lmixr_ighost_cigi -lmixr_ighost_pov
LDFLAGS += -lmixr_terrain
LDFLAGS += -lmixr_ui_glut -lmixr_graphics -lmixr_base
LDFLAGS += -lprotobuf -lJSBSim
LDFLAGS += -lftgl -lfreetype -lcigicl
LDFLAGS += -lglut -lGLU -lGL -lX11
LDFLAGS += -lpthread -lrt
```

**REGRA** — ligação estática do GNU ld exige **dependentes antes de dependências**; daí
`mixr_base` por último e `mixr_models`/`JSBSim` **repetidos** (dependência circular entre
`models` e `simulation`/`recorder` — ver `MIXR-CONTEXT.md` §28.5).

**PADRÃO** — a ordem geral é: `interop` → `models` → libs de exemplo → `recorder` →
`simulation` → `instruments` → `linkage` → `ighost` → `terrain` → `ui_glut` → `graphics`
→ `base` → terceiros → sistema.

**NESTA POC** — **esse problema não existe aqui.** O `.pc` gerado pelo Conan
(`mixr.pc`) já lista as nove bibliotecas na ordem correta, e o `meson.build` resolve tudo
com uma única declaração `dependency('mixr')`. É uma das simplificações reais que a
migração para Conan+Meson trouxe.

## 14.4 PADRÃO: Meson (só nos tutoriais)

```meson
# examples/tutorial01/meson.build — na íntegra
executable(
    'tutorial01',
    files('./main.cpp'),
    dependencies : [ mixr_base_dep, thread_dep ],
    link_args : [static_flags],
    install : true,
    install_rpath : '$ORIGIN/../lib'
)
```

```meson
# examples/meson.build — na íntegra
subdir('./tutorial01')
subdir('./tutorial02')
subdir('./tutorial03')
subdir('./tutorial04')
subdir('./tutorial05')
subdir('./tutorial06')
```

**ARMADILHA** — o `meson.build` de exemplo usa `mixr_base_dep` (uma dependência **por
biblioteca**), enquanto o `.pc` do pacote Conan desta PoC expõe um `mixr` agregado. Não
copie a forma; copie a ideia.

**NESTA POC** — o padrão de `poc/NN-slug/` (raiz dá `subdir()`, cada subprojeto tem
`meson.build` que só faz `subdir('./src')`, e `src/meson.build` declara o `executable()`)
é **invenção deste repositório**, não upstream. O upstream só tem exemplo Meson para os
seis primeiros tutoriais, todos de arquivo único.

---

# 15. CATÁLOGO DE ARMADILHAS DOS PRÓPRIOS EXEMPLOS

Erros e inconsistências **reais** presentes na árvore oficial. Copiar um exemplo sem ler
esta lista é herdar o defeito.

## 15.1 Vazamentos e ciclo de vida

| Onde | Problema |
|---|---|
| `mainSim2/SimIoHandler.cpp`, `mainCockpit/TestIoHandler.cpp` | `myNav->ref()` sem `unref()` correspondente, a cada acionamento da chave de *steerpoint* |
| vários | `getPlayers()` devolve `PairStream` referenciado; esquecer o `unref()` vaza (os exemplos acertam, mas o risco é sistemático) |

## 15.2 Código incorreto que compila

| Onde | Problema |
|---|---|
| `mainSim2/SimIoHandler.cpp`, `mainCockpit/TestIoHandler.cpp` | `mixr::base::Boolean sw(sw);` — variável sombreia o `bool sw` externo e se inicializa consigo mesma |
| `shared/xbehaviors/PlaneAction.cpp` | `setThrottles(throttles, 2)` com contagem fixa `2`, embora o vetor seja preenchido para `getNumberOfEngines()` |
| `shared/xbehaviors/PlaneBehaviors.cpp` | `setSlotVoteOnCriticalAltitude`/`OnIncomingMissile` desreferenciam `num` sem checar `nullptr` |
| todos com `-f` | `argv[++i]` sem verificar `i+1 < argc` |

## 15.3 Duplicação e cópia sem revisão

| Onde | Problema |
|---|---|
| `mainCockpit/factory.cpp`, `mainUbf1/factory.cpp`, `testRecordData/factory.cpp` | `mixr::instruments::factory(name)` chamado duas vezes |
| `mainCockpit/TestDisplay.cpp` | bloco de ~12 linhas "achar Tws ou Gmti conforme modo A/A ou A/G" repetido 4× |
| 38 `main.cpp` | `builder()` copiado literalmente, comentários inclusive |
| `mainCockpit/configs/player01.epp`, `testRadar/configs/test1.epp` | 4 mísseis idênticos escritos por extenso (56 linhas) — `mainUbf1` mostra a alternativa com macro |

## 15.4 Inconsistências de convenção

| Onde | Problema |
|---|---|
| `testRecorderWrite/main.cpp` | `recorder::factory` **depois** de `base::factory`, invertendo a regra da cadeia |
| `mainUbf1/main.cpp` | `event(RESET_EVENT)` seguido de `reset()` — *reset* duplo |
| `mainQt1/StnTimerObject.cpp` | `dt` nominal fixo em vez de medido, ao contrário dos exemplos GLUT |
| vários | `tcPriority` com valores díspares (0.0 / 0.5 / 1) sem critério documentado |
| `examples/README.md` | lista `testRng` (inexistente); grafa `mainIGViewer`; cita `mainy1` (renomeado para `mainCockpit`) |

## 15.5 Dependências ausentes

| Onde | Problema |
|---|---|
| `interop/dis.epp`, `ighosts/cigi.epp` | `#include` de `DisIncomingEntityTypes.epp`, `DisOutgoingEntityTypes.epp`, `CigiTypeMap.epp` — **nenhum existe** na árvore vendorizada (moram em `mixr-data`) |
| todos os `.epp` com `MIXR_DATA_*` | dependem do repositório `mixr-data`, ausente |
| todos os gráficos | dependem de `mixr_graphics`/`mixr_ui_glut`/`mixr_instruments`, **ausentes do pacote Conan** (§0) |

## 15.6 Bloqueio de thread

| Onde | Problema |
|---|---|
| `testNetHandler/Sender.cpp` | `base::msleep(1000)` dentro de `updateData()` |
| `mainSim1/main.cpp` | `msleep(1000)` após `createTimeCriticalProcess()` — espera arbitrária, não sincronização |

## 15.7 Armadilhas de EDL

| Assunto | Detalhe | § |
|---|---|---|
| Rótulo duplicado | arquivo incluído com rótulo + `#include` com rótulo = erro de sintaxe | §5.4 |
| `#include` no `.epp` | exige preprocessador C; o `edl_parser` não entende | §5.1 |
| Macros com aspas | `MIXR_DATA_*` já expandem com aspas; não coloque mais | §5.2 |
| Numeração de canal | `ai:`/`di:` de 1; `channel:` de 0 | §9.1 |
| `initPosition: [x y z]` | Z é **NED**: negativo = para cima | §6.2 |
| `antennaName`/`trackManagerName` | nome errado **não** gera erro; o sensor apenas nunca funciona | §6.3 |
| *Slot* homônimo em derivada | o da base fica inalcançável | §4.8 |
| `Antenna` já é `ScanGimbal` | não crie um `ScanGimbal` separado | §6.3 |

---

# 16. RECEITAS: "QUERO FAZER X → OLHE Y"

| Objetivo | Exemplo de referência | Aproveitável nesta PoC? |
|---|---|---|
| Criar uma classe MIXR do zero | `tutorial02` → `tutorial03` | **sim** |
| Dar *slots* configuráveis a ela | `tutorial03`, `tutorial05` | **sim** |
| Família de modelos intercambiáveis por EDL | `tutorial04` | **sim** |
| Colocá-la na árvore de componentes | `tutorial06` | **sim** |
| Rodar simulação sem `Station` | `mainNonRT1` | **sim** |
| Simulação mínima com `Station` e *thread* T/C | `mainSim1` | **sim** |
| Cenário com radar, RWR, mísseis | `testRadar/configs/test1.epp`, `mainCockpit/configs/player01.epp` | **sim** (só o EDL) |
| Cenário IR com *seeker* | `testInfrared/configs/` | **sim** (só o EDL) |
| Terreno real | `mainGndMapRdr/configs/srtm.epp`, `mainTerrain` | **sim** (só o EDL) |
| Rota e piloto automático | `mainSim1/configs/test2/`, `mainLaero/configs/test.epp` | **sim** |
| **Criar um sensor novo** | `mainGndMapRdr/RealBeamRadar` | **sim** — é *o* gabarito |
| Estender `WorldModel` | `testDafif/models/WorldModel` | **sim** (o padrão; DAFIF não existe aqui) |
| Estender `Station` | `mainSim2/SimStation`, `mainSim3/Station` | parcial (sem a parte de display) |
| Entrada de joystick/HOTAS | `mainSim2/SimIoHandler` + `configs/linkage/` | **sim** |
| Simular hardware ausente | `testLinkage/configs/linkage/MockDevice.epp` | **sim** |
| Gravar dados da simulação | `testRecordData/configs/dataRecorder.epp` | **sim** |
| Ler/analisar gravação | `testRecorderRead/` | **sim** |
| Evento de gravação próprio | `shared/xrecorder/` | **sim** |
| IA por votação (nativa) | `mainUbf1` + `shared/xbehaviors/` | **sim** |
| IA por máquina de estados | `testStateMach` | **sim** |
| Comunicação entre componentes | `testEvents/`, `mainLaero/AdiDisplay.cpp` | **sim** |
| Transporte de rede novo | `shared/xzmq/` | **sim** |
| DIS entre duas instâncias | `testRadar/configs/test3a.epp` + `test3b.epp` | **sim** (`interop_dis` existe) |
| Teste de escalabilidade | `mainSim1/configs/test3/`, `testRadar/test2a-c` | **sim** |
| Empacotar extensões reutilizáveis | `shared/x*/` | **sim** |
| Qualquer coisa gráfica | `demo*`, `tutorial07/08`, `testGraphics` | **não** (§0.2) |
| **Criar um `DynamicsModel` novo** | *nenhum exemplo* | — use `RacModel.cpp` do fonte do framework |
| **Criar uma arma nova** | *nenhum exemplo* | — use `AamMissile`/`Effect` do fonte |

---

# 17. RESUMO EXECUTIVO — OS DEZ PONTOS QUE MAIS IMPORTAM

1. **O pacote `mixr/1.0.5` desta PoC não tem gráficos.** Nove bibliotecas, sem
   `graphics`/`glut`/`instruments`/`ighost`/`dafif`. A maioria dos exemplos abre janela
   GLUT e, portanto, não compila aqui — mas seus **padrões de C++ e de EDL** são todos
   aproveitáveis. (§0)

2. **Todo `main.cpp` MIXR tem quatro peças**: `factory()` encadeada, `builder()` de cinco
   passos, `main()` que reseta e cria *threads*, e um laço. O `builder()` é literalmente
   o mesmo texto em 38 arquivos. (§2)

3. **A ordem da cadeia de fábricas é uma regra, não um estilo**: classes da aplicação →
   bibliotecas de exemplo → framework do específico ao genérico → `base` por último.
   Registrar uma classe com o **mesmo nome de fábrica** de uma do framework e pô-la antes
   substitui a original em todos os `.epp` sem editá-los. (§2.1, §7.4)

4. **Existem seis arranjos de laço principal** e todos separam `updateTC()` de
   `updateData()`. O que muda é quem agenda cada um: nada (tutorial), o próprio laço com
   `msleep` de tempo absoluto (`mainSim1` — o padrão desta PoC), GLUT, Qt, ou uma
   `PeriodicThread` própria. **`event(RESET_EVENT)` antes do primeiro quadro é
   obrigatório.** (§3)

5. **O `.epp` não é EDL**: passa por um preprocessador C antes. Isso habilita `#include`,
   `#define` de macro-função e — o recurso mais elegante do framework — **um cabeçalho
   `.hpp` compartilhado entre C++ e EDL** (`channel_map.hpp`, `dataRecorderTokens.hpp`).
   (§5.1, §5.2)

6. **Ao modularizar EDL, escolha uma das duas convenções de rótulo e não misture**: ou o
   arquivo incluído carrega o rótulo e o `#include` é solto, ou o inverso. Misturar
   produz `x: x: (...)`, erro de sintaxe — a mesma pedra em que `poc/05` tropeçou. (§5.4)

7. **A árvore `components:` de um *player* tem ordem canônica**
   (`dynamicsModel`/`pilot`/`nav`/`antennas`/`sensors`/`obc`/`sms`) e é atravessada por
   uma **cadeia de nomes** (`antennaName`, `trackManagerName`, `leadPlayerName`) que
   **falha em silêncio** se um nome não resolver. (§6.3)

8. **O ponto de extensão dominante é `Station` (11 exemplos)**, sempre com as mesmas três
   invariantes: recurso pesado no `reset()` guardado por *flag*, `container(this)` no
   `setSlot`, `updateTC()` propaga mas `updateData()` não. Já **`DynamicsModel` e
   `AbstractWeapon` não são estendidos por nenhum exemplo** — para física própria, o
   objetivo desta PoC, o gabarito mais próximo é `RealBeamRadar` (estender um `System`)
   e o fonte de `RacModel`. (§7)

9. **`send()` resolve o nome entre os FILHOS de quem chama** — não em si mesmo, não na
   árvore inteira. É a causa raiz do *bug* achado em `poc/08-event-relay`, e a razão de o
   padrão de navegação ser sempre `findContainerByType` **subindo** até a `Station` e
   descendo de lá, com o ponteiro **cacheado** no primeiro `updateData()`. (§8.1, §8.3)

10. **O framework já traz duas arquiteturas de IA** — UBF (fusão de ações **por campo**,
    ponderada por voto, configurada em EDL) e `StateMachine` (com `call`/`rtn` e
    submáquinas declaradas em EDL). Ambas estão no `mixr_base` deste pacote. A escolha
    desta PoC pelo BehaviorTree.CPP é defensável por ferramental, mas **não** por
    ausência de alternativa nativa. (§10)
