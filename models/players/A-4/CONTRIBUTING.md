# Desenvolvendo o modelo `flight` (A-4)

Este é o roteiro para mexer **neste modelo** — só o `.so` (`domain/`, `bt/`, `ubf/`, `xnative/`,
os `configs/flight_tree*.xml`, `data/jsbsim/`). Ele é um passo **anterior e independente** de
qualquer cenário: nada aqui sabe, ou precisa saber, quem vai carregá-lo. Terminar o roteiro abaixo
é ter um `.so` publicado e testado isoladamente — apontar um `.edl` para ele é decisão de quem
monta o cenário, feita depois, em outro lugar (`CONTRIBUTING.md` §5 na raiz,
`.claude/rules/models-plugin.md`).

Para criar um modelo **novo** do zero (não mexer neste), o ponto de entrada é
[`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md) — este arquivo assume que o modelo já
existe e o SDK do host já foi publicado (`cd ../../.. && make configure && make sdk`, uma vez).

## 1. O ciclo de iteração

`make` sozinho — sem alvo — não compila nada: `.DEFAULT_GOAL` é `help`, então lista os alvos
disponíveis (mesmo padrão do Makefile raiz do `poc-mixr`). O ciclo do dia a dia é sempre com o alvo
explícito:

```bash
make build      # configura (./build, uma vez, via 'configure') + compila -> libflight.so
make test       # domain + tree + native, ~1 s -- nenhuma levanta Station (builda antes, se preciso)
```

Um artefato só: `xnative::FlightAgentTC` (o único agente deste modelo — decide na fase 3 do frame
de tempo crítico, um por thread do pool) é sempre compilado e sempre registrado, sem `#ifdef`
nenhum. Editar `domain/`/`bt/`/`ubf/`/`xnative/` sempre afeta o mesmo (e único) artefato.

`.clangd`/`compile_commands.json` já apontam para `./build` (o `meson setup` que `make build`
dispara, via o alvo `configure`, gera esse arquivo) — IntelliSense funciona desde a primeira
compilação, sem passo extra.

**Alvos, na ordem em que normalmente se usa** (`make help` lista os mesmos, com descrição):

| alvo | o que faz |
|---|---|
| `make check-root` | só confere o pré-requisito (SDK do host publicado) — feedback verde de OK ou vermelho com o comando exato que falta |
| `make configure` | `meson setup` isolado neste projeto (depende de `check-root`) |
| `make build` | compila `libflight.so` (depende de `configure`) |
| `make test` | roda a suíte do modelo (depende de `build`) |
| `make install` | instala em `./dist` — a raiz DESTE projeto (depende de `build`) |
| `make install-host` | deposita em `../../../plugins/` (depende de `install`) — seção 4 |
| `make uninstall-host` | reverte só o que este modelo publicou em `../../../plugins/` |
| `make clean` | remove `./build` e `./dist` locais |

`check-root`/`configure`/`build`/`install` encadeiam por dependência — chamar só `make install`,
por exemplo, já dispara `build` (e `configure`/`check-root`) se precisar.

**Se a mudança acrescenta uma classe ou um slot que algum cenário de produção passa a usar**, o
mirror de contrato de [`template`](../template/) (`src/mirror.cpp`) — o "modelo estranho" que os
testes de plugin do host carregam para provar que o contrato basta — também precisa
aceitar/ignorar o mesmo slot. Ver seção 5.

## 2. Testes

```bash
make test
```

Três suítes, nenhuma levanta `Station`:

| suíte | prova | linka MIXR |
|---|---|---|
| `domain` | regras puras de `domain/` | não |
| `tree` | a árvore de produção contra um `FakeDecisionContext`; e `tree-model-sync` (seção 3) | não |
| `native` | fábrica, slots com tipo/unidade, fronteira de fase do `AlertDatalink` | sim |

O que fica de fora — `RadarScan`, `FlightState::updateState`, `FlightAction::execute`, os
`genAction()` — só roda com um player vivo, fora do escopo deste diretório: cobertura fica nas
suítes `scenario`/`determinism`/`memory` do host (`cd ../../.. && make test`, depois de publicar o
plugin — seção 4).

## 3. Editando a árvore de comportamento (Groot)

Pré-requisito, uma vez por máquina, a partir da raiz do repositório:

```bash
conan create ./deps/groot --build=missing --settings=build_type=Release
```

Dali em diante, `make open-groot` (raiz) sempre abre a janela. É uma janela Qt — só funciona numa
máquina com display.

### 3.1 Abrir uma árvore de produção

Os 5 `configs/flight_tree*.xml` já têm o que o Groot precisa — comentário de cabeçalho sem `--`
(hífen duplo: o parser XML do Groot, `QDomDocument`, é estrito e recusa o arquivo **inteiro** se
achar um, ao contrário do `tinyxml2` que o executor usa) e o bloco `<TreeNodesModel>` colado
(sem ele, o Groot recusa qualquer nó customizado — `FuelLow`, `ContactDetected`, `OnnxPolicy`,
`PyDecide`... — com `"This model has not been registered"`). Abrem direto:

```bash
cd ../../.. && make open-groot
# File > Load... > models/players/A-4/configs/flight_tree.xml
```

**Nunca edite o `.xml` de produção como primeiro rascunho.** Copie para `/tmp`, itere na cópia, e
só substitua o arquivo de produção quando a árvore estiver pronta — `git diff` mostra exatamente o
resultado, em vez de um histórico de tentativas.

### 3.2 Registrar um nó novo (e manter o Groot sabendo dele)

1. Implemente o nó em `src/bt/nodes/` e registre em `src/bt/bt_factory.cpp` (nó nativo, sem
   dependência do SDK) ou `src/bt/bt_factory_sdk.cpp` (nó que usa `xlog`/`xrandom`/`xinfer`/
   `xpyembed` — qualquer coisa que arraste `sdk_dep`). Acrescente o `.cpp` novo a
   `bt_sources`/`bt_sdk_sources` em [`meson.build`](meson.build) (raiz deste projeto) — é de lá
   que `dump-tree-model` (`tools/meson.build`), `test-tree` e `test-native`
   ([`tests/meson.build`](tests/meson.build)) compilam; esquecer esse passo faz o nó compilar
   para o `.so` de produção mas não aparecer no manifesto do Groot nem nos testes.
2. Recompile o gerador e atualize toda árvore de `configs/`:
   ```bash
   make update-bt
   ```
   (recompila `dump-tree-model` sozinho, via a dependência `build` do alvo). Descobre as árvores
   por CONTEÚDO (todo `.xml` de `configs/` com `<BehaviorTree>` dentro, não uma lista de nomes) e
   substitui o `<TreeNodesModel>` de cada uma pelo registro atual — insere o bloco do zero numa
   árvore que ainda não tem um.
3. `make test` cobra isso sozinho a partir daqui — o teste `tree-model-sync` (suíte `tree`) falha
   se o passo 2 for esquecido, listando exatamente quais arquivos ficaram desatualizados.

`tools/dump_tree_model.cpp` monta a MESMA `BT::BehaviorTreeFactory` que o modelo de verdade monta
(`registerNodes()` + `registerSdkNodes()`, com um `NodeContext{}` vazio — seguro, porque
`registerBuilder<T>()` só guarda um construtor, nunca instancia nó nenhum) e chama a função nativa
do BT.CPP, `BT::writeTreeNodesModelXML(factory)` — o bloco não é mantido à mão. Detalhe técnico e
armadilhas confirmadas do gerador → [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

### 3.3 Criar uma árvore nova, do zero

O Groot não tem "começar em branco com os meus nós" — a paleta só é populada a partir do
`<TreeNodesModel>` de um arquivo já carregado. O alvo `create-bt` cria esse ponto de partida direto
em `configs/bt.xml` (recusa se o arquivo já existir — renomeie/mova a árvore anterior primeiro):

```bash
make create-bt
cd ../../.. && make open-groot
# File > Load... > models/players/A-4/configs/bt.xml
```

Sai com uma raiz `<Fallback>` vazia (builtin do BT.CPP, não precisa de nó nenhum do modelo) e a
paleta já mostrando todos os nós que este modelo registra (em azul, distintos dos nativos do
BT.CPP) — arraste da paleta pro canvas, conecte arrastando de uma saída pra uma entrada, `File >
Save`. Itere em `configs/bt.xml` à vontade; só renomeie para o nome definitivo (e aponte um cenário
de produção pra ele) depois de validada — `make update-bt` mantém o `<TreeNodesModel>` de qualquer
nome que ela tiver em sincronia dali em diante.

### 3.4 Monitorar uma árvore ao vivo

O modelo já tem o hook pronto, opt-in por variável de ambiente — implementação em
`src/ubf/BtBehavior.cpp` (`startGrootMonitorIfRequested()`). Precisa de **qualquer** binário do
host que já carregue este plugin e tenha um player com esse nome no cenário — o comando abaixo é
só um exemplo, troque `-folder`/`-scenario` pelo cenário que você estiver usando para testar:

```bash
cd ../../..    # a raiz do poc-mixr
MIXR_GROOT_MONITOR=falcon1 ./dist/bin/app -folder <pasta-do-cenario> -scenario <nome>
```

Em outro terminal com display: `make open-groot` (raiz) → aba **Monitor** → conectar em
`localhost`, portas 1666 (status) / 1667 (topologia). A árvore daquele player aparece se colorindo
em tempo real conforme tica. Sem a variável de ambiente, nenhuma porta abre — zero custo por
padrão, e só uma instância de `BT::PublisherZMQ` pode existir por processo.

Armadilhas de ciclo de vida (por que o publisher precisa ser derrubado **antes** de qualquer
`reset()`/`shutdownNotification()`/cópia da árvore) → [`../../../CLAUDE.md`](../../../CLAUDE.md),
seção "Groot — editor e monitor ao vivo".

## 4. Gerar e publicar o plugin

`make build`/`make test` (seções 1-2) compilam e verificam a lógica isoladamente, mas não deixam
nada que um cenário consiga carregar — publicar é sempre em dois passos, e os dois são necessários:

```bash
make install-host              # -> ../../../plugins/ (o mesmo deposito que um .so de terceiro usaria)
cd ../../.. && make install     # sincroniza plugins/ -> dist/ -- unico alvo que toca dist/ pelo modelo
```

`make install-host` (deste `Makefile`) nunca escreve em `dist/` — só em `../../../plugins/` (lib,
flat) e `../../../plugins/data/flight/` (a árvore + `data/jsbsim/`), o depósito genérico que
qualquer `.so` — próprio ou de terceiro — usa. Só o `make install` da raiz (alvo `sync-plugins`)
copia dali para `dist/lib/mixr-plugins/`/`dist/share/mixr-plugins/`, que é de onde um binário do
host de fato `dlopen()`. Rodar só o primeiro comando deixa o `.so` pronto mas invisível a qualquer
cenário; rodar os dois é o que "publicar o plugin" quer dizer aqui.

**Este passo termina o trabalho deste diretório.** Fazer um cenário carregar o `.so` publicado —
escrever o bloco `( PluginModule file: "libflight.so" provides: {...} )`, escolher a porta do
Tacview, decidir a frota — é trabalho de quem monta o cenário, não deste modelo; ver
`CONTRIBUTING.md` §5 (raiz) para essa parte, se for você quem for fazer os dois.

## 5. Antes de propor a mudança

- `make test` (aqui). Se a mudança mexeu em algo que algum cenário de produção também exercita
  (slot novo, nome de fábrica novo, mudança de comportamento visível no dump `frame=`), publique
  (seção 4) e rode `cd ../../.. && make test` — as suítes `scenario`/`determinism`/`plugin` do host
  exercitam este `.so` de fora.
- **`provides:` é igualdade EXATA de conjunto** entre cada `.edl` que carrega este plugin e o que
  o `.so` exporta. Um nome de fábrica novo obriga atualizar `provides:` em **todo** cenário
  existente que carrega `libflight.so` — não só o que motivou a mudança.
  `python3 tests/guard/check_colisao_fabrica.py` (na raiz; o hook `check-colisao-fabrica.sh` já
  roda isso sozinho depois de editar `.cpp`/`.hpp` sob `models/`) cobra colisão de nome entre
  plugins carregados juntos no mesmo processo — já aconteceu de verdade (`CLAUDE.md`, "vigésima
  terceira passada", entre A-4 e o extinto modelo `missile`).
- **Se a mudança acrescenta uma classe ou um slot que algum `.edl` de produção passa a usar,
  atualize o mirror de contrato do [`template`](../template/) junto**
  ([`../template/docs/CONTRATO.md`](../template/docs/CONTRATO.md)). Ele roda o mesmo
  cenário de produção trocando só o `file:` do `( PluginModule )`, e existe justamente para
  quebrar quando o contrato muda — um slot/nome que só o `A-4` conhece derruba
  `plugin-modelo-estranho`/`plugin-deposito-terceiro` no host.
- **`CHANGELOG.md`** — uma entrada por mudança que alguém precisaria saber antes de mexer neste
  modelo, não uma por commit. A versão é a do `project()` em `meson.build`; as datas saem da data
  de commit, nunca da mensagem (todo commit deste repositório se chama `up`).

## Ler também

- [`README.md`](README.md) — as quatro camadas, o build, os testes
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — calibração do A-4, o gerador de
  `<TreeNodesModel>` em detalhe, armadilhas confirmadas
- [`docs/POLITICAS.md`](docs/POLITICAS.md) — decidir com Python ou com uma política ONNX em vez da
  árvore nativa
- [`../template/docs/CONTRATO.md`](../template/docs/CONTRATO.md) — o que TODO modelo
  (este incluído) tem que fazer
- [`../../../CLAUDE.md`](../../../CLAUDE.md) — arquitetura do repositório inteiro; seção "Groot —
  editor e monitor ao vivo" tem a lista completa de armadilhas do editor/monitor
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
  PRÉVIA" — o build em etapas do repositório (`configure` → `sdk` → `models` → `build` →
  `install`) e por que `plugins/`/`dist/` são decoplados de propósito
- [`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md) — criar um modelo **novo** (não é este
  roteiro, que é sobre mexer no `A-4` já existente)
