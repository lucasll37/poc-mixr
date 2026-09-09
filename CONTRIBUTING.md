# Contribuindo com um modelo novo

Este arquivo não repete o que já está escrito em outro lugar — ele COSTURA, na ordem certa, os
documentos que já são autoridade sobre cada assunto. Cada passo abaixo aponta para o documento
certo; leia-o antes de seguir para o próximo passo. Este roteiro é sobre escrever um **modelo**
novo — para mudar o *host* (`app/`, `src/`, `libs/`) não há roteiro equivalente; o mais próximo
é [`CLAUDE.md`](CLAUDE.md), que é referência de arquitetura, não passo a passo.

## 0. O que você vai construir

Um **modelo** é uma biblioteca (`.so`) compilada à parte, aberta em tempo de execução via
`dlopen` (o `.so` é aberto em *runtime* pelo host, nunca linkado em tempo de compilação) — o host
nunca vê seu código-fonte. Se este parágrafo é novidade, leia primeiro [`CLAUDE.md`](CLAUDE.md),
seção "O MODELO é um plugin, construído numa etapa PRÉVIA", antes de continuar.

> **Resumo de uma tela**, para quem só quer os fatos mínimos antes de abrir os documentos abaixo:
>
> - Um **modelo** decide via um agente **UBF** (*Unified Behavior Framework*, o mecanismo nativo
>   do MIXR para plugar decisão externa num `Player`) — hoje, quase sempre uma árvore do
>   BehaviorTree.CPP por trás dele.
> - `playerId` (usado em toda chamada ao `xboard`) é `player->getID()` — o `mixr::models::Player`
>   que hospeda o agente; ele já chega pronto no contexto de decisão (`genAction()`/`execute()`).
> - `provides:` no `.edl` é **igualdade exata de conjunto** contra o que o `.so` exporta — um nome
>   a mais ou a menos aborta a inicialização, com mensagem dizendo o que faltou/sobrou.
> - Escrever no `xboard` (seção 3 do [`CONTRATO.md`](models/players/template/docs/CONTRATO.md)) é
>   obrigatório e **falha em silêncio** se esquecido — sem erro, só `bt=--`/`dec=0` para sempre.
> - O ponto de partida copiável é `models/players/template/`; `make new-model NAME=... CATEGORY=player`
>   copia e renomeia por você (`CATEGORY` é obrigatório: `player`/`system`/`others`, decide a
>   subpasta de `models/` — não existe `CATEGORY=event`, ver seção 2).
>
> Isto não substitui os documentos abaixo — é só o suficiente para não se perder na primeira
> passada por eles.

Antes de começar, confira [`models/REGISTRO.md`](models/REGISTRO.md) — a tabela de quem já está
mexendo em qual modelo. Se o modelo que você quer escrever já tem alguém trabalhando nele, evite
duplicar; se não, essa é a hora de acrescentar sua própria linha.

Para entender o **framework** por baixo dos modelos (MIXR/BehaviorTree.CPP) além do necessário
para este roteiro → seção 7, "Onde consultar durante o trabalho".

## 1. Pré-requisitos e SDK do host (uma vez por máquina)

→ [`README.md`](README.md), seções "Pré-requisitos" e "Build" ([`INSTALL.md`](INSTALL.md) tem o
passo a passo comentado, se faltar algo). Para escrever um modelo, pare na etapa `make sdk` — não
precisa compilar o host inteiro (`make build`/`make install`) ainda.

Sem isso, todo `Makefile` autocontido de modelo (o seu vai ter um) falha em `check-root` dizendo
exatamente isto.

## 2. Escolha o ponto de partida

Um único ponto de partida copiável — [`models/players/template`](models/players/template/): já
nasce em camadas (`domain/`→`ubf/`→`xnative/`), com uma única decisão de exemplo em cada camada.
Se o seu modelo decide com uma regra/condição só, apague o que não precisar — o "porquê" de cada
camada está no `README.md`/`docs/` do diretório.

**O caminho recomendado é o gerador automático**, que já existe neste repositório:

```bash
make new-model NAME=meu_modelo CATEGORY=player
```

`CATEGORY` é **obrigatório** e decide em qual subpasta de `models/` o scaffold entra —
`player`→`models/players/`, `system`→`models/systems/`, `others`→`models/others/` (a mesma
taxonomia que `MODELOS_PRODUCAO`, no `Makefile` raiz, já descobre por `find` sob qualquer
subpasta de `models/`). Não existe `CATEGORY=event`: `models/events/` não é uma pasta de
projetos-modelo, um por evento — é **um** projeto Meson só (a lib `events`), e um evento novo
ganha uma pasta `payloads/<TOKEN>/` *dentro* dele, não um scaffold de `template/` novo (ver
[`models/events/README.md`](models/events/README.md)).

Ele faz a cópia e a renomeação mecânica por você (projeto, módulo, namespace, `ROOT` do Makefile
pela profundidade real) e termina com um checklist do que sobra manual — inclusive apagar
`src/mirror.cpp` e o artefato `template_mirror`, que NÃO fazem parte do scaffold (ver o aviso no
topo do próprio arquivo). Se preferir fazer à mão, o roteiro completo está em [`template`,
`docs/PRIMEIROS-PASSOS.md`](models/players/template/docs/PRIMEIROS-PASSOS.md) — mas o gerador
cobre exatamente essa receita.

## 3. O contrato: o que TODO modelo tem que fazer

→ [`models/players/template/docs/CONTRATO.md`](models/players/template/docs/CONTRATO.md) — leia
inteiro. Três obrigações merecem destaque:

- **`provides:` bate EXATAMENTE com o que o `.so` exporta** (seção 2) — se não bater, o processo
  aborta na inicialização dizendo o que entregou; não é silencioso, mas é a causa mais comum de
  "meu cenário não sobe".
- **escrever no `xboard`** (seção 3) — a única obrigação que falha em **silêncio**: sem isso, o
  host sobe, roda, e a tela de status mostra `bt=--`/`dec=0` para sempre, sem erro em lugar
  nenhum.
- **namespace aninhado sob `mixr::models::x<nome>`** (seção 6) — sem isso, dois `.so` carregados
  juntos no mesmo processo podem colidir em RTTI silenciosamente.

## 4. Escreva a lógica

Depois do contrato, é trabalho de domínio — não tem receita mecânica. Se veio do `template`,
`docs/PRIMEIROS-PASSOS.md`, passo 5, sugere a ordem (domain → ubf/State → ubf/Behavior →
ubf/Action → xnative/factory).

### Editando e depurando a árvore com o Groot

Se a decisão é uma árvore do BehaviorTree.CPP (o caso da maioria dos modelos deste repositório),
o **Groot** ajuda em duas pontas: edita o `.xml` visualmente, e monitora ao vivo uma árvore
rodando dentro da simulação. Instalação, uma vez por máquina → [`INSTALL.md`](INSTALL.md) §4
(`./scripts/deps.sh`, ou só `conan create ./deps/groot ...` se as outras dependências já vierem
do remote privado). Depois de instalado, `make open-groot` sempre abre a janela.

#### Editar uma árvore

**Para experimentar agora, sem esperar seu próprio modelo compilar**: os `models/players/A-4/
configs/flight_tree*.xml` de produção já têm tudo isso resolvido (comentário sem `--`, bloco
`<TreeNodesModel>` colado) — abrem direto no Groot, `File > Load...`, sem nenhum passo abaixo.
São o exemplo de referência para o que sua própria árvore precisa ter.

**Para a árvore do SEU modelo**, que ainda não tem nada disso:

1. Nunca abra o `.xml` de produção direto — copie:
   ```bash
   cp models/players/<seu-modelo>/configs/<sua-arvore>.xml /tmp/arvore_groot.xml
   ```
2. **Verifique se o comentário de cabeçalho tem `--` (hífen duplo)** — o parser XML do Groot
   (`QDomDocument`, estrito) recusa o arquivo **inteiro** se tiver; o parser que o host usa
   (`tinyxml2`) é tolerante e deixa passar, então esse problema só aparece no Groot, nunca ao
   rodar a simulação de verdade. Sintoma: Groot recusa o arquivo com um erro genérico de sintaxe,
   sem apontar a causa real.
   ```bash
   grep -n -- '--' /tmp/arvore_groot.xml   # se aparecer fora de <!-- / -->, troque por "-" e pronto
   ```
3. Sem um `<TreeNodesModel>`, o Groot não conhece os nós customizados do SEU modelo (eles só
   existem registrados dentro do `.so`, que o Groot nunca viu) — aparecem sem porta/genéricos.
   Se o seu modelo já tem o gerador nativo (próxima seção), use-o em vez de escrever o bloco à
   mão. Sem ele, cole algo assim dentro de `<root>` (a lista de `ID`s e portas é a mesma que você
   passou para `factory.registerBuilder<T>(ID, ...)`/`providedPorts()` no seu `bt_factory.cpp`; o
   exemplo abaixo é o do modelo de produção `A-4`, só para mostrar a forma):
   ```xml
   <TreeNodesModel>
       <Condition ID="FuelLow">
           <input_port name="margin" type="double" default="0.0">descricao da porta</input_port>
       </Condition>
       <Action ID="ReturnToBase"/>
       <!-- um <Condition>/<Action> por nó customizado que o SEU bt_factory.cpp registra -->
   </TreeNodesModel>
   ```
4. `make open-groot` → `File > Load...` → `/tmp/arvore_groot.xml`. Edite arrastando/soltando,
   salve. O arquivo salvo continua carregando normalmente em `createTreeFromFile()` — o
   `<TreeNodesModel>` é ignorado pelo executor, só existe para o Groot.

#### Criar uma árvore nova (com os nós que você já implementou)

Nada aqui monta a árvore por você — o Groot não tem "começar em branco com meus nós" (a paleta só
é populada a partir do `<TreeNodesModel>` de um arquivo já carregado). O que existe é um gerador
que produz o **arquivo inteiro, pronto pra abrir**: uma árvore vazia (um `<Fallback>` só, de
partida) mais o `<TreeNodesModel>` com os nós do SEU `bt_factory.cpp`, os dois no mesmo `<root>`.

No modelo `A-4` (produção), já está pronto — um único alvo de Makefile:

```bash
cd models/players/A-4 && make create-bt
```

Cria `models/players/A-4/configs/bt.xml` (recusa se o arquivo já existir — renomeie/mova a árvore
anterior antes de rodar de novo). `make open-groot` → `File > Load...` →
`models/players/A-4/configs/bt.xml`. A paleta já mostra todos os nós do modelo (em azul, distintos
dos nativos do BT.CPP); arraste da paleta pro canvas, conecte arrastando de uma saída pra uma
entrada, e `File > Save` — depois de validada, renomeie para o nome definitivo e aponte o
`treeFile:` do seu `.edl` pra ele. Registrou um nó novo em `bt_factory.cpp`/`bt_factory_sdk.cpp`
depois disso? `make update-bt` atualiza o `<TreeNodesModel>` de **toda** árvore de `configs/`
(descobertas por conteúdo, não por nome) de uma vez, inclusive árvores já existentes que ainda não
tinham o bloco.

**Se o SEU modelo não é o `A-4`**, estes dois alvos não existem automaticamente pra ele — é código
(`models/players/A-4/tools/dump_tree_model.cpp` + `tools/update_bt_models.py`, mais os alvos
`create-bt`/`update-bt` do `Makefile` daquele projeto). Copie o padrão de lá — os três arquivos
não têm nada amarrado ao nome/pastas do `A-4` especificamente, então dá pra copiar `tools/` inteiro
para `models/players/<seu-modelo>/tools/` sem editar uma linha: o `.cpp` só monta uma
`BT::BehaviorTreeFactory`, chama os `registerNodes()`/`registerSdkNodes()` (ou equivalente) do SEU
`bt_factory.cpp`, e imprime `BT::writeTreeNodesModelXML(factory)` — a função nativa do BT.CPP que
faz o trabalho de verdade; o `.py` descobre as árvores do SEU projeto em `configs/` sozinho.

#### Depurar/monitorar ao vivo

O modelo de produção (`A-4`) já tem esse hook pronto, opt-in por variável de ambiente:

```bash
MIXR_GROOT_MONITOR=falcon1 ./dist/bin/app -folder src/poc/dis -scenario flight
```

Em outro terminal com display: `make open-groot` → aba **Monitor** → conectar em `localhost`
(portas 1666/1667, fixas). A árvore daquele player aparece se colorindo em tempo real conforme
tica. **Se o SEU modelo também usa uma árvore do BT.CPP e você quer essa mesma capacidade**, ela
não vem de graça do framework — é código do modelo. Replique o padrão de
`models/players/A-4/src/ubf/BtBehavior.cpp` (função `startGrootMonitorIfRequested()`): depois de
`btFactory.createTreeFromFile(...)` ter sucesso, construa um `BT::PublisherZMQ(tree)` se uma
variável de ambiente bater com o nome do player, e derrube esse objeto (`.reset()`) **antes** de
qualquer recriação da árvore (`reset()`, `shutdownNotification()`, cópia) — ele guarda uma
referência a ela.

Lista completa de armadilhas já pagas (o motivo de cada regra acima, com detalhe de
implementação) → [`CLAUDE.md`](CLAUDE.md), seção "Groot — editor e monitor ao vivo".

## 5. Publique e aponte um cenário

### 5.1 Verificar o `.so`

```bash
make models
nm -D --defined-only dist/lib/mixr-plugins/libmeu_modelo.so | grep ' T '   # 1 linha só
ldd dist/lib/mixr-plugins/libmeu_modelo.so | grep 'not found'             # vazio
```

Se veio do `template`, o mesmo passo está em `docs/PRIMEIROS-PASSOS.md`, passo 6.

### 5.2 Registrar num cenário

A poc não tem código — três passos de **dado**, detalhados em [`CLAUDE.md`](CLAUDE.md), seção "Ao
adicionar um subprojeto novo": uma pasta `src/poc/<nome>/` com `configs/scenario.edl.in` + `data/`
+ `README.md`; o bloco `( PluginLoader )`/`( PluginModule provides: {...} )` como **primeira**
entrada de `components:` no `.edl.in` (`provides:` é igualdade EXATA de conjunto contra o que o
`.so` exporta); e nada mais a registrar — sem catálogo estático, a poc já fica alcançável por
`./app -folder <pasta> -scenario <nome>` assim que `configs/` tiver um único `.edl`/`.edl.in`.

### 5.3 Terreno: quando o cenário sai da área coberta

O `terrain:` do `.edl` nomeia **um** tile SRTM, e é ele que o `Player::updateElevation()` nativo lê
(AGL, anti-CFIT). O repositório versiona **cinco** tiles reais, todos em torno da Serra do Mar (RJ),
que é onde os cenários de demonstração voam:

```
S23W043  (o dos cenários)   S23W042  S22W043  S23W044  S22W044
```

Se o seu cenário fica **fora** dessa caixa, o tile precisa estar em disco antes de rodar — e a
falha, se não estiver, é **silenciosa**: `Player::updateElevation()` ignora o retorno de
`getElevation()`, deixa a elevação em `0.0` e ainda liga `tElevValid = true`. Ou seja, a aeronave
passa a raciocinar sobre um "terreno" ao nível do mar sem nenhum aviso.

**`scripts/fetch_srtm.sh`** baixa o que faltar, do espelho aberto *Terrain Tiles* da AWS Open Data
(SRTM1 real, `.hgt.gz`, **sem login** — no formato binário exato que `SrtmHgtFile` exige):

```bash
scripts/fetch_srtm.sh S23W042 S22W043          # tiles nomeados (canto SW, convenção SRTM)
scripts/fetch_srtm.sh --bbox -25 -20 -46 -40   # uma caixa: lat0 lat1 lon0 lon1
scripts/fetch_srtm.sh --brasil                 # o Brasil inteiro (1600 tiles, ~12 GB)
scripts/fetch_srtm.sh --brasil --dry-run       # só lista o que baixaria
```

É **idempotente**: pula todo tile já em disco e íntegro (`gzip -t`), então interromper no meio e
rodar de novo continua de onde parou. Um tile que não existe no espelho (oceano aberto) sai como
`ausente`, não como erro — só falha de rede/escrita vira `ERRO` e código de saída 1.
`SRTM_PARALELO=N` ajusta a concorrência (padrão 12).

**O que é versionado, e o que não é.** Só os cinco tiles acima entram no git (exceções nomeadas no
`.gitignore`); qualquer outro `.hgt.gz` fica **local**. Isso é deliberado: `--brasil` são ~12 GB,
que no histórico do git inviabilizariam o clone — e não há nada a preservar, porque o espelho é
público e o script reconstrói tudo. O `.hgt` descompactado nunca é versionado; é gerado sob demanda
(`app::makeTerrainSampler()`) na primeira consulta que cai dentro daquele tile.

**Não há passo de registro.** `app/TerrainQuery.cpp` varre a pasta inteira na primeira consulta e
indexa por nome — tile novo aparece sozinho, sem editar código. A vista de Mapa do `./app` enxerga
**todos** os tiles em disco (com teto de 12 residentes em memória, troca por LRU); a simulação em
si continua lendo só o que o `terrain:` do cenário nomeia.

Detalhe completo, incluindo por que o carregador é preguiçoso e a cobertura medida dos cenários da
família `A4-*DOF`: [`shared/data/terrain/srtm/README.md`](shared/data/terrain/srtm/README.md).

### 5.4 Cobertura de teste automática (opcional)

A poc já roda sem nenhuma linha em `tests/meson.build`. Decida se vale a pena por esta tabela
(marcadores no próprio `tests/meson.build` apontam de volta pra cá):

| a poc... | cobertura | precedente |
|---|---|---|
| segue o formato dual `intruder`/`lowfuel` com rótulos `EVADE`/`SUPPORT`/`RTB` | entra na lista `pocs` — ganha `scenario-*`/`memory-*`/`determinism-*` de graça, via `foreach` | `flight`, `python-flight` |
| não segue esse formato, mas tem uma propriedade que vale a pena provar | bloco(s) `test()` manuais, reaproveitando `scenario_runner`/`leak_runner`/`determinism_sh` fora do `foreach` | `onnx-policy` |
| nenhuma das duas — é só composição de players já testados em outro lugar | nenhuma entrada; documente o porquê no `README.md` da própria poc | `built-in_mixr_1`, `full-systems-nav` |

## 6. Teste

→ [`README.md`](README.md), seção "Testes", para o `-Dtests=true`/`make test` gerais, e
[`tests/README.md`](tests/README.md) para as camadas e o que cada uma prova.

Específico de modelo: `make test-models` roda só a suíte do(s) modelo(s) (pula a do host); de
dentro do seu próprio `models/players/<nome>/`, `make test` roda sozinho, sem tocar no resto do
repositório.

### `make test-asan` não cobre o seu modelo automaticamente

`make test-asan` (raiz, ver [`README.md`](README.md#make-test-asan) para o passo a passo) é hoje
**hardcoded para o A-4**: reconstrói `models/players/A-4` com `-Dasan=true` e roda uma fixture da poc
`flight` (que carrega `libflight.so`, o plugin do A-4) sob LeakSanitizer. Ele **não**
aceita `NAME=`, e a razão é mecânica — `make models ASAN=true` (que ele chama por baixo) passa
`ASAN=true` para **todo** projeto de modelo encontrado por `find` sob `models/` (qualquer subpasta
com `project()` no próprio `meson.build`, não só `models/players/`), mas só
`models/players/A-4/meson_options.txt` declara `option('asan', ...)`; `template` não tem essa
opção, então um modelo copiado dele hoje **ignora** a flag em silêncio
(o GNU Make não reclama de variável de linha de comando não consumida) — nada quebra, mas
`make test-asan` também não instrumenta nem exercita o `.so` do seu modelo.

Se o seu modelo precisa da mesma cobertura, replique três peças de
`models/players/A-4/meson.build`/`meson_options.txt`: (1) `option('asan', type: 'boolean', value:
false, ...)` no seu `meson_options.txt`; (2) `asan_cpp_args`/`asan_link_args` derivados de
`get_option('asan')`, passados como `cpp_args:`/`link_args:` no(s) seu(s) alvo(s)
`shared_library()`; (3) uma fixture/cenário próprio para exercitar o `.so` — o alvo da raiz não
sabe do seu modelo, então rodar sob ASan continua sendo manual: `meson configure
models/players/<seu-modelo>/build -Dasan=true && meson compile -C models/players/<seu-modelo>/build`
e então o binário do host apontando pro seu cenário, com `LSAN_OPTIONS=suppressions=<seu
arquivo de supressões>` (comece copiando [`tests/memory/asan.supp`](tests/memory/asan.supp) — as
supressões de lá são do framework MIXR em si, não deste repositório, então valem para qualquer
modelo).

## 7. Onde consultar durante o trabalho

Três camadas diferentes, para perguntas diferentes:

**Aprender o framework do zero** — `contexts/mixr-report.pdf`/`contexts/bt-report.pdf` são os
manuais técnicos completos (13 capítulos cada) do MIXR e do BehaviorTree.CPP, leitura contínua.
[`docs/manual/`](docs/manual/) (`make open-docs`, página estática) complementa com três visões
interativas: a animação do ciclo de execução do MIXR sobre uma árvore de componentes real, um
catálogo buscável das classes do fork (fábrica, slots, participação por fase) e um ensaio sobre a
cadeia de decisão `FlightAgentTC → UBF → BehaviorTree`.

**Consulta rápida sobre uma classe/API específica (inclusive por um agente de IA)** — os três
`contexts/*-CONTEXT.md` (`MIXR-CONTEXT.md`, `MIXR-PATTERN-CONTEXT.md`, `BTCPP-CONTEXT.md`) são
destilações voltadas a resposta rápida, não substituem os PDFs acima para quem está aprendendo do
zero. Quando a destilação não basta ou parece contraditória, a autoridade final é o fonte
vendorizado em `contexts/src/` (git-ignored — não vem num clone limpo) ou os headers instalados
pelo Conan. O agente `mixr-vendor-lookup` (`.claude/agents/`) já sabe essa ordem de consulta e
devolve só o resumo, sem despejar C++ de terceiro na conversa.

**`.claude/` — automação e contexto para quem trabalha com um agente Claude Code neste repo**:

- `.claude/rules/*.md` carregam contexto extra sozinhas quando você edita um caminho que casa com
  o glob delas (`app/`+`src/`+`libs/`, `models/`, arquivos `.edl`/`.edl.in`/`.edl.frag`) — não
  precisam ser lidas à mão, mas valem uma olhada se quiser entender por que um hook bloqueou algo.
- `.claude/hooks/*.sh` rodam automaticamente depois de editar os arquivos correspondentes: host
  opaco (`check-host-opaco.sh`), colisão de nome de fábrica (`check-colisao-fabrica.sh`) e lint de
  EDL (`check-edl-lint.sh`) — a mesma checagem que `make test` (suíte `tools`) ou
  `python3 src/ui/scripts/edl_lint.py <arquivo>` direto fariam, só que sem esperar o próximo build.
- `.claude/skills/README.md` e `.claude/mcp/README.md` documentam por que não há nenhum dos dois
  hoje, e quando criar um.
