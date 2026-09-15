# Contribuindo com um modelo novo

Este arquivo não repete o que já está escrito em outro lugar — ele COSTURA, na ordem certa, os
documentos que já são autoridade sobre cada assunto. Cada passo abaixo aponta para o documento
certo; leia-o antes de seguir para o próximo passo. Este roteiro é sobre escrever um **modelo**
novo — para mudar o *core* (`app/`, `src/`, `libs/`) não há roteiro equivalente. O mais próximo é
consultar, pontualmente, a seção de `CLAUDE.md` que cobre a parte específica que você for mexer
(ex.: "`./app` — TUI (*Text User Interface*, interface de texto interativa em terminal) de
controle/monitoramento" para o painel, "`src/rl`" para o wrapper de RL, ou
o `README.md` de cada `libs/x<nome>` para uma lib) — nunca o arquivo inteiro como leitura de
fundo, ver a ressalva do próprio [`TOUR.md`](TOUR.md), seção "Antes de começar".

> **Para só escrever um modelo do zero, as seções 0–7 bastam.** A seção 8 (mais da metade deste
> arquivo) é um estudo de caso opcional, mais longo — código real de como *estender* um modelo que
> já existe — e não é um passo necessário do roteiro; pule direto para ela só se for esse o seu
> caso, ou volte depois de terminar a seção 7. **"Bastam" é sobre o roteiro, não sobre não sair
> deste arquivo**: como este documento COSTURA em vez de duplicar (ver o parágrafo acima), as
> seções 0–7 ainda mandam abrir `models/REGISTRO.md`, `CONTRATO.md`/`PRIMEIROS-PASSOS.md` inteiros
> e algumas seções específicas de `CLAUDE.md` — é o preço de ter uma fonte de verdade só por
> assunto, em vez de uma segunda cópia aqui que envelheceria em silêncio.

## Sumário

- [0. O que você vai construir](#0-o-que-você-vai-construir)
- [1. Pré-requisitos e SDK do core](#1-pré-requisitos-e-sdk-do-core-uma-vez-por-máquina)
- [2. Escolha o ponto de partida](#2-escolha-o-ponto-de-partida)
- [3. O contrato: o que TODO modelo tem que fazer](#3-o-contrato-o-que-todo-modelo-tem-que-fazer)
- [4. Escreva a lógica](#4-escreva-a-lógica)
  - [Editando e depurando a árvore com o Groot](#editando-e-depurando-a-árvore-com-o-groot)
    (editar uma árvore existente, criar uma árvore nova, depurar/monitorar ao vivo)
- [5. Publique e aponte um cenário](#5-publique-e-aponte-um-cenário)
  - [5.1 Verificar o `.so`](#51-verificar-o-so) · [5.2 Registrar num cenário](#52-registrar-num-cenário)
    · [5.3 Terreno](#53-terreno-quando-o-cenário-sai-da-área-coberta) ·
    [5.4 Cobertura de teste](#54-cobertura-de-teste-automática-opcional)
- [6. Teste](#6-teste)
- [7. Onde consultar durante o trabalho](#7-onde-consultar-durante-o-trabalho)
- [8. Exemplo guiado completo (opcional)](#8-exemplo-guiado-completo-opcional-um-nó-novo-do-a-4-do-c-até-um-cenário-de-sandbox)
  — estudo de caso mais longo, pule se as seções 0–7 já bastaram para o seu caso
  - [8.1 Condition](#81-anatomia-de-uma-condition-fuellowcondition) ·
    [8.2 Action](#82-anatomia-de-uma-action-reportandevadeaction) ·
    [8.3 DecisionContext](#83-a-interface-que-mantém-os-nós-livres-do-mixr) ·
    [8.4 Registrando na fábrica](#84-registrando-um-nó-na-fábrica) ·
    [8.5 Colando na árvore](#85-colando-o-nó-na-árvore-e-mantendo-o-treenodesmodel-em-dia)
  - [8.6 Testando](#86-testando-o-nó-sem-nenhuma-simulação) ·
    [8.7 Recompilando](#87-recompilando-e-verificando-o-so) ·
    [8.8 Carregando num cenário](#88-carregando-o-modelo-num-cenário-o-bloco-pluginmodule-real) ·
    [8.9 Rodando em sandbox](#89-rodando-num-cenário-de-sandbox) ·
    [8.10 Fechando o ciclo](#810-fechando-o-ciclo)

## 0. O que você vai construir

Um **modelo** é uma biblioteca (`.so`) compilada à parte, aberta em tempo de execução via
`dlopen` (o `.so` é aberto em *runtime* pelo core, nunca linkado em tempo de compilação) — o core
nunca vê seu código-fonte. Esse é o mecanismo em si; a garantia complementar — que o core nem
sequer precisa do FONTE do modelo para **compilar**, porque os dois são projetos Meson (o sistema
de build usado por este repositório) separados em etapas de build distintas — é o que a seção de
`CLAUDE.md` abaixo detalha. Se algo disso é
novidade, confira agora — é uma consulta pontual a essa seção, não leitura do arquivo inteiro —
[`CLAUDE.md`](CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa PRÉVIA".

> **Resumo de uma tela**, para quem só quer os fatos mínimos antes de abrir os documentos abaixo:
>
> - Um **modelo** decide via um agente **UBF** (*Unified Behavior Framework*, o mecanismo nativo
>   do MIXR para plugar decisão externa num `Player`) — hoje, quase sempre uma árvore do
>   BehaviorTree.CPP por trás dele.
> - `playerId` (usado em toda chamada ao `xboard` — `mixr::xboard::Readout`, a estrutura
>   compartilhada e protegida por mutex, indexada por id de player, que reporta o estado da
>   decisão para status/testes) é `player->getID()` — o `mixr::models::Player` que hospeda o
>   agente; ele já chega pronto no contexto de decisão (`genAction()`/`execute()`).
> - `provides:` no `.edl` (**EDL**, a linguagem de configuração declarativa do MIXR — cenário,
>   *players*, sensores; ver o parágrafo de abertura do [`README.md`](README.md) para a definição
>   completa) é **igualdade exata de conjunto** contra o que o `.so` exporta — um nome
>   a mais ou a menos aborta a inicialização, com mensagem mostrando as duas listas lado a lado
>   ("cenário declarou" / "a .so entrega").
> - Escrever no `xboard` (seção 3 do [`CONTRATO.md`](models/template/docs/CONTRATO.md)) é
>   obrigatório e **falha em silêncio** se esquecido — sem erro, só `bt=--`/`dec=0` para sempre
>   (os rótulos literais que aparecem nos dumps `frame=...` — `-deterministic`, `check_determinism.sh`
>   — e, sob outra forma, na própria tela do `./app`; `dec` abrevia "contagem de decisões". Detalhe
>   de cada token em [`tests/README.md`](tests/README.md): `frame=`/`-deterministic` na seção
>   "Camada 3 — cenário", `dec`/`check_determinism.sh` na seção "Camada 5 — determinismo").
> - O ponto de partida copiável é `models/template/`; `make new-model NAME=... CATEGORY=player`
>   copia e renomeia por você (`CATEGORY` é obrigatório: `player`/`system`/`others`, decide a
>   subpasta de `models/` — não existe `CATEGORY=event`, ver seção 2).
> - Quer ver tudo isto costurado, com código real do modelo de produção (A-4) — criar um nó de
>   árvore, registrá-lo, colá-lo no XML, publicar o `.so` e rodar num cenário — do início ao fim?
>   → seção 8, "Exemplo guiado completo".
>
> Isto não substitui os documentos abaixo — é só o suficiente para não se perder na primeira
> passada por eles.

Antes de começar, confira [`models/REGISTRO.md`](models/REGISTRO.md) — a tabela de quem já está
mexendo em qual modelo. Se o modelo que você quer escrever já tem alguém trabalhando nele, evite
duplicar; se não, essa é a hora de reservar seu lugar com uma linha informal — só as colunas
**Modelo** (o nome pretendido) e **Responsável** (você) preenchidas, **Pasta**/**Última
atualização**/**Observação** como `—` (a pasta ainda não existe antes da seção 2) — a atualização
formal, com os detalhes finais, fica para quando você abrir o PR (é o
único momento que o próprio `REGISTRO.md` documenta: "ao abrir um PR de modelo novo, acrescente
sua própria linha no fim da tabela"); a reserva antecipada é só uma convenção deste roteiro, para
o próximo que for checar a tabela não duplicar o seu trabalho.

Para entender o **framework** por baixo dos modelos (MIXR/BehaviorTree.CPP) além do necessário
para este roteiro → seção 7, "Onde consultar durante o trabalho".

## 1. Pré-requisitos e SDK do core (uma vez por máquina)

→ [`README.md`](README.md), seções "Pré-requisitos" e "Build" ([`INSTALL.md`](INSTALL.md) tem o
passo a passo comentado, se faltar algo). Para escrever um modelo, pare na etapa `make sdk` — o
alvo que publica em `dist/` o contrato de ABI (*Application Binary Interface* — o formato binário
que um `.so` de modelo tem que respeitar para o core conseguir carregá-lo,
`libs/xplugin/PluginAbi.hpp`) mais as bibliotecas compartilhadas core↔modelo (`libs/x<nome>`;
lista completa em [`CLAUDE.md`](CLAUDE.md), seção "O SDK de plugin") — não precisa compilar o core
inteiro (`make build`/`make install`) ainda.

Sem isso, todo `Makefile` autocontido de modelo (o seu vai ter um) falha em `check-root` dizendo
exatamente isto.

## 2. Escolha o ponto de partida

Um único ponto de partida copiável — [`models/template`](models/template/): já
nasce em camadas (`domain/`→`bt/`→`ubf/`→`xnative/` — esta última é a camada que expõe as classes
que o MIXR de fato instancia via `factory.cpp`: os `State`/`Behavior`/`Action` concretos e os
utilitários específicos do framework, a fronteira nativa do modelo), com uma única decisão de
exemplo em cada camada.
Se o seu modelo decide com uma regra/condição só, apague o que não precisar — o "porquê" de cada
camada está no `README.md`/`docs/` do diretório.

**O caminho recomendado é o gerador automático**, que já existe neste repositório:

```bash
make new-model NAME=meu-modelo CATEGORY=player
```

`CATEGORY` é **obrigatório** e decide em qual subpasta de `models/` o scaffold entra —
`player`→`models/players/`, `system`→`models/systems/`, `others`→`models/others/` (a mesma
taxonomia que `MODELOS_PRODUCAO`, no `Makefile` raiz, já descobre por `find` sob qualquer
subpasta de `models/`). Não existe `CATEGORY=event`: `models/events/` não é uma pasta de
projetos-modelo, um por evento — é **um** projeto Meson (o sistema de build usado por este
repositório) só (a lib `events`), e um evento novo
ganha uma pasta `payloads/<TOKEN>/` *dentro* dele, não um scaffold de `template/` novo (ver
[`models/events/README.md`](models/events/README.md)).

Ele faz a cópia e a renomeação mecânica por você (projeto, módulo, namespace, `ROOT` do Makefile
pela profundidade real) — já apagando sozinho, antes de qualquer renomeação, `src/mirror.cpp` e o
artefato `template_mirror`, que NÃO fazem parte do scaffold (ver o aviso no topo do próprio
arquivo) — e termina imprimindo um checklist do que ainda sobra manual (regra de negócio,
`xboard`, `xnative/factory.cpp`, prosa do README, `provides:`, `git add`, cenário/`REGISTRO.md`).
Se preferir fazer à mão (ou quiser entender o que o gerador faz por dentro), o roteiro completo
está em [`template`, `docs/PRIMEIROS-PASSOS.md`](models/template/docs/PRIMEIROS-PASSOS.md) — o
gerador cobre exatamente a receita dos Passos 1-4 (cópia, renome, contrato), mas o Passo 5 dali
(a ordem de escrever a lógica: domain → ubf/State → bt/nodes+árvore → ubf/Behavior → ubf/Action →
xnative/factory → plugin.cpp) continua sendo o roteiro certo mesmo tendo usado `make new-model` —
volte a esse documento na seção 4 abaixo.

## 3. O contrato: o que TODO modelo tem que fazer

→ [`models/template/docs/CONTRATO.md`](models/template/docs/CONTRATO.md) — leia
inteiro. Três obrigações merecem destaque:

- **`provides:` bate EXATAMENTE com o que o `.so` exporta** (seção 2) — se não bater, o processo
  aborta na inicialização mostrando as duas listas lado a lado ("cenário declarou" / "a .so
  entrega"); não é silencioso, mas é a causa mais comum de "meu cenário não sobe".
- **escrever no `xboard`** (seção 3) — a única obrigação que falha em **silêncio**: sem isso, o
  core sobe, roda, e a tela de status mostra `bt=--`/`dec=0` para sempre, sem erro em lugar
  nenhum.
- **namespace aninhado sob `mixr::models::x<nome>`** (seção 6) — sem isso, dois `.so` carregados
  juntos no mesmo processo podem colidir em RTTI (*Run-Time Type Information*, o mecanismo de C++
  por trás de `dynamic_cast`/`typeid`) silenciosamente.

## 4. Escreva a lógica

Depois do contrato, é trabalho de domínio — não tem receita mecânica. Se veio do `template`,
`docs/PRIMEIROS-PASSOS.md`, passo 5, sugere a ordem completa (domain → ubf/State → **bt/nodes +
a árvore em `configs/`, onde a decisão de fato mora** → ubf/Behavior → ubf/Action →
xnative/factory → `src/plugin.cpp`) — ver o passo 5 na íntegra, não um resumo abreviado aqui.

**Se você nunca usou BehaviorTree.CPP (BT.CPP), o vocabulário abaixo já é o mínimo pra acompanhar
o resto desta seção e a seção 8** — não é preciso ler
[`contexts/BTCPP-CONTEXT.md`](contexts/BTCPP-CONTEXT.md) inteiro antes de escrever o primeiro nó:
o próprio documento se declara pensado para consulta pontual (alimentar um RAG — *Retrieval
Augmented Generation*, busca que recupera só o trecho relevante), seção por seção, cada uma
autocontida — não para leitura linear de ponta a ponta (tem ~55 mil palavras — ver o cabeçalho
"COMO USAR ESTE DOCUMENTO" do próprio arquivo). Consulte-o por seção, conforme a dúvida aparecer
(seção 7 detalha como); um nó da árvore expõe um método `tick()`, chamado a cada avaliação, que
devolve
`SUCCESS`, `FAILURE` ou `RUNNING` (em andamento — raro no uso deste repositório, que tickeia a
árvore inteira a cada frame); um `Fallback` tenta os filhos em ordem até um devolver `SUCCESS`
(senão falha ele também); um `Sequence` executa os filhos em ordem até um devolver `FAILURE`
(senão sucede ele também). É assim que a prioridade da árvore de produção é expressa (seção 8.5).

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
São o exemplo de referência para o que sua própria árvore precisa ter. **Esses arquivos são
rastreados pelo git** — se for só experimentar (arrastar/soltar sem intenção de manter a edição),
`git diff`/`git checkout -- <arquivo>` revisa ou descarta depois, sem precisar copiar o arquivo
para uma pasta à parte antes.

**Para a árvore do SEU modelo**, que ainda não tem nada disso:

1. Abra o `.xml` do seu modelo diretamente pelo caminho real onde ele já está —
   `make open-groot` → `File > Load...` → navegue até
   `models/players/<seu-modelo>/configs/<sua-arvore>.xml`. Não é preciso copiar o arquivo para
   nenhum outro lugar antes: o Groot navega até qualquer diretório do disco.
2. **Verifique se o comentário de cabeçalho tem `--` (hífen duplo)** — o parser XML do Groot
   (`QDomDocument`, estrito) recusa o arquivo **inteiro** se tiver; o parser que o core usa
   (`tinyxml2`) é tolerante e deixa passar, então esse problema só aparece no Groot, nunca ao
   rodar a simulação de verdade. Sintoma: Groot recusa o arquivo com um erro genérico de sintaxe,
   sem apontar a causa real.
   ```bash
   grep -n -- '--' models/players/<seu-modelo>/configs/<sua-arvore>.xml   # se aparecer fora de <!-- / -->, troque por "-" e pronto
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
4. Edite arrastando/soltando, salve. O arquivo salvo continua carregando normalmente em
   `createTreeFromFile()` — o `<TreeNodesModel>` é ignorado pelo executor, só existe para o
   Groot. Se preferir não misturar experimentos com o histórico do arquivo versionado,
   `git diff`/`git checkout -- <arquivo>` revisa ou descarta a edição antes de commitar — não é
   preciso trabalhar numa cópia à parte para isso.

#### Criar uma árvore nova (com os nós que você já implementou)

Nada aqui monta a árvore por você — o Groot não tem "começar em branco com meus nós" (a paleta só
é populada a partir do `<TreeNodesModel>` de um arquivo já carregado). O que existe é um gerador
que produz o **arquivo inteiro, pronto pra abrir**: uma árvore vazia (um `<Fallback>` só, de
partida) mais o `<TreeNodesModel>` com os nós do SEU `bt_factory.cpp`, os dois no mesmo `<root>`.

O gerador (`make create-bt`) já vem pronto tanto no `template` quanto no `A-4` (produção) — um
único alvo de Makefile. Para experimentar sem esbarrar em nada já commitado, use o `template`:

```bash
cd models/template && make create-bt
```

Cria `models/template/configs/bt.xml` (recusa se o arquivo já existir — renomeie/mova a árvore
anterior antes de rodar de novo; um `.gitignore` dedicado a esse caminho existe justamente para
`make create-bt` continuar funcionando em toda cópia nova do `template`). `make open-groot` →
`File > Load...` → `models/template/configs/bt.xml`. A paleta já mostra todos os nós do modelo (em
azul, distintos dos nativos do BT.CPP); arraste da paleta pro canvas, conecte arrastando de uma
saída pra uma entrada, e `File > Save` — depois de validada, renomeie para o nome definitivo e
aponte o `treeFile:` do seu `.edl` pra ele. Registrou um nó novo em
`bt_factory.cpp`/`bt_factory_sdk.cpp` depois disso? `make update-bt` atualiza o `<TreeNodesModel>`
de **toda** árvore de `configs/` (descobertas por conteúdo, não por nome) de uma vez, inclusive
árvores já existentes que ainda não tinham o bloco.

> **No `A-4` (produção) o mesmo `make create-bt` falha de propósito.**
> `models/players/A-4/configs/bt.xml` já está commitado neste repositório — decisão registrada no
> próprio `.gitignore` (comentário acima da exceção `models/template/configs/bt.xml`) —, então
> rodar o comando ali recusa com "já existe" até você renomear/remover esse arquivo primeiro. Não é
> um artefato esquecido: é o mesmo arquivo já citado em "Editar uma árvore" acima, que o Groot já
> consegue abrir sem precisar gerar nada.

**Se você veio do `template` (seção 2), estes dois alvos já vêm prontos** — o próprio
`models/template/Makefile` já declara `create-bt`/`update-bt`/`open-groot` (o mesmo texto que
`models/players/A-4/Makefile` tem), com `tools/dump_tree_model.cpp`/`tools/update_bt_models.py` já
dentro do scaffold — então todo modelo gerado por `make new-model` já nasce com os mesmos alvos
que o `A-4` tem (`make help` lista todos), sem copiar nada à mão. Isso só não vale se o seu projeto de modelo **não** partiu
do `template` (por exemplo, um `.so` de terceiro trazido para dentro do repositório): nesse caso,
copie `tools/dump_tree_model.cpp` + `tools/update_bt_models.py` do `A-4` ou do `template` — os
três arquivos não têm nada amarrado ao nome/pastas de origem, então dá pra copiar `tools/` inteiro
para `models/<categoria>/<seu-modelo>/tools/` sem editar uma linha: o `.cpp` só monta uma
`BT::BehaviorTreeFactory`, chama os `registerNodes()`/`registerSdkNodes()` (ou equivalente) do SEU
`bt_factory.cpp`, e imprime `BT::writeTreeNodesModelXML(factory)` — a função nativa do BT.CPP que
faz o trabalho de verdade; o `.py` descobre as árvores do SEU projeto em `configs/` sozinho.

Ver a seção 8 para o ciclo completo com código real — de escrever o nó em C++ até ele aparecer
rodando num cenário.

#### Depurar/monitorar ao vivo

O modelo de produção (`A-4`) já tem esse hook pronto, opt-in por variável de ambiente:

```bash
make run-node-monitor PLAYER=a4_1 SCENARIO=sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in
```

O alvo já **abre o Groot** (`GROOT=0` pula, para máquina sem display); lá, aba **Monitor** →
conectar em `localhost` (portas 1666/1667, fixas). A árvore daquele player aparece se colorindo em
tempo real conforme tica. `run-app-monitor` faz o mesmo com o TUI — mas o TUI desliga o log no
console, então para **diagnosticar** prefira o `node`, que loga no terminal.

> **O `PLAYER` tem de existir no cenário.** O exemplo aqui já foi `falcon1`, nome que não existe em
> cenário nenhum de `sandbox/` — e um alvo que não casa era 100% silencioso. Hoje o core avisa
> (`LOG(WARNING)`) nomeando os players reais. E **hoje só o modelo A-4 tem o hook**: um player de
> outro modelo é aceito pela checagem e mesmo assim não liga porta nenhuma.

**Se o SEU modelo também usa uma árvore do BT.CPP e você quer essa mesma capacidade**, ela não vem
de graça do framework — é código do modelo. Replique o padrão de
`models/players/A-4/src/ubf/BtBehavior.cpp` (função `startGrootMonitorIfRequested()`): depois de
`btFactory.createTreeFromFile(...)` ter sucesso, construa um `BT::PublisherZMQ(tree)` (ZMQ =
*ZeroMQ*, a biblioteca de mensageria usada para publicar o estado da árvore em rede, nas portas
1666/1667) se uma variável de ambiente bater com o nome do player, e derrube esse objeto
(`.reset()`) **antes** de
qualquer recriação da árvore (`reset()`, `shutdownNotification()`, cópia) — ele guarda uma
referência a ela. **Copiar o hook copia junto as armadilhas** — duas, com rótulos diferentes na
seção do Groot no `CLAUDE.md`: a "armadilha nº3" (o Monitor do Groot 1.0.0 fechava sozinho) e o
item 2 de "Duas armadilhas do `PublisherZMQ` nativo" (o destrutor tem um use-after-free) — vale
ler as duas antes de replicar.

Lista completa de armadilhas já pagas (o motivo de cada regra acima, com detalhe de
implementação) → [`CLAUDE.md`](CLAUDE.md), seção "Groot — editor e monitor ao vivo".

## 5. Publique e aponte um cenário

### 5.1 Verificar o `.so`

```bash
make models
make install                                                              # sync-plugins: plugins/ -> dist/
nm -D --defined-only dist/lib/mixr-plugins/libmeu-modelo.so | grep ' T '   # 1 linha só
ldd dist/lib/mixr-plugins/libmeu-modelo.so | grep 'not found'             # vazio
```

`make models` sozinho deposita só em `plugins/` (a raiz do repositório) — nunca em `dist/`; é o
`make install` acima quem sincroniza os dois (ver "Desacoplando `models` de `dist/`" em
`CLAUDE.md`). Sem ele, os dois comandos de checagem abaixo falham com "No such file or directory".

Se veio do `template`, os mesmos comandos estão em `docs/PRIMEIROS-PASSOS.md`, na seção
"Checklist rápido, para revisar antes do primeiro commit" (depois do Passo 7).

### 5.2 Registrar num cenário

A poc não tem código — três passos de **dado**, detalhados em [`CLAUDE.md`](CLAUDE.md), seção "Ao
adicionar um subprojeto novo": uma pasta `src/poc/<nome>/` com `configs/scenario.edl.in` + `data/`
+ `README.md`; o bloco `( PluginLoader )`/`( PluginModule provides: {...} )` como **primeira**
entrada de `components:` no `.edl.in` (`provides:` é igualdade EXATA de conjunto contra o que o
`.so` exporta); e nada mais a registrar — sem catálogo estático, a poc já fica alcançável por
`./app -folder <pasta> -scenario <nome>` assim que `configs/` tiver um único `.edl`/`.edl.in`.
(`.edl.in` é só um `.edl` com tokens — `@NUM_TC_THREADS@`, `@include:...@` — que
`app/ScenarioTemplate.*` substitui antes de o parser ler o arquivo final; ver `CLAUDE.md`, seção
"Estrutura de um subprojeto", tabela "A aplicação". Sem token nenhum para substituir, `.edl` puro
já basta — as duas extensões convivem no repositório conforme o cenário precisa ou não desse
passo.)

**Um exemplo mínimo e literal**, para não ficar só na descrição acima: o caminho mais rápido para
conferir que o SEU `.so` carrega — sem ainda decidir entre virar uma poc de verdade em `src/poc/`
ou ganhar cobertura de teste automática (seção 5.4) — é uma pasta solta em `sandbox/`, copiando um
cenário que já funciona como base e trocando só o bloco `PluginModule` (seção 5.1) pelo do seu
modelo:

```bash
mkdir -p sandbox/meu-teste/configs sandbox/meu-teste/data/{logs,recordings,messages}
touch sandbox/meu-teste/data/logs/.gitkeep sandbox/meu-teste/data/recordings/.gitkeep \
      sandbox/meu-teste/data/messages/.gitkeep
cp src/poc/dis/flight/configs/scenario.edl.in sandbox/meu-teste/configs/scenario.edl.in
# dentro da copia: troque o bloco PluginModule (file:/provides:) pelo do SEU .so -- seção 5.1 --
# e o treeFile:/nome das classes do agente pelos que o SEU modelo de fato exporta

./build/app/src/app -folder ./sandbox -scenario meu-teste
```

`sandbox/` é gitignorado por padrão — publicar essa pasta como exemplo versionado é opcional (a
seção 8.9, que repete esta MESMA receita usando o A-4 como modelo concreto em vez de placeholders,
mostra como).

### 5.3 Terreno: quando o cenário sai da área coberta

O `terrain:` do `.edl` nomeia **um** tile SRTM (*Shuttle Radar Topography Mission*, o
levantamento de elevação de terreno de acesso público da NASA que este projeto usa), e é ele que o
`Player::updateElevation()` nativo lê
(AGL — *above ground level*, altitude acima do solo, por oposição a MSL/*mean sea level* ou
HAE/*height above ellipsoid*; anti-CFIT — *controlled flight into terrain*, o acidente de uma
aeronave em pleno controle colidindo contra o terreno, que o piso calculado a partir do AGL existe
para evitar). O repositório versiona **cinco** tiles reais, todos em torno da Serra do Mar (RJ),
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

**Não há passo de registro.** `app/src/app/TerrainQuery.cpp` varre a pasta inteira na primeira consulta e
indexa por nome — tile novo aparece sozinho, sem editar código. A vista de Mapa do `./app` enxerga
**todos** os tiles em disco (com teto de 12 residentes em memória, troca por LRU — *Least Recently
Used*, descarta o tile menos usado recentemente para abrir espaço para um novo); a simulação em
si continua lendo só o que o `terrain:` do cenário nomeia.

Detalhe completo, incluindo por que o carregador é preguiçoso e a cobertura medida dos cenários da
família `A4-*DOF`: [`shared/data/terrain/srtm/README.md`](shared/data/terrain/srtm/README.md).

### 5.4 Cobertura de teste automática (opcional)

A poc já roda sem nenhuma linha em `tests/meson.build`. Decida se vale a pena por esta tabela
(marcadores no próprio `tests/meson.build` apontam de volta pra cá):

| a poc... | cobertura | precedente |
|---|---|---|
| segue o formato dual `intruder`/`lowfuel` com rótulos `EVADE`/`SUPPORT`/`RTB` (*Return To Base* — retornar à base) | entra na lista `pocs` — ganha `scenario-*`/`memory-*`/`determinism-*` de graça, via `foreach` | `flight`, `python-flight` |
| não segue esse formato, mas tem uma propriedade que vale a pena provar | bloco(s) `test()` manuais, reaproveitando `scenario_runner`/`leak_runner`/`determinism_sh` fora do `foreach` | `onnx-policy` |
| nenhuma das duas — é só composição de players já testados em outro lugar | nenhuma entrada; documente o porquê no `README.md` da própria poc | — |

## 6. Teste

→ [`README.md`](README.md), seção "Testes", para o `-Dtests=true`/`make test` gerais, e
[`tests/README.md`](tests/README.md) para as camadas e o que cada uma prova.

Específico de modelo: `make test-models` roda só a suíte do(s) modelo(s) (pula a do core); de
dentro do seu próprio `models/players/<nome>/`, `make test` roda sozinho, sem tocar no resto do
repositório.

### `make test-asan` não cobre o seu modelo automaticamente

`make test-asan` (raiz, ver [`README.md`](README.md#make-test-asan) para o passo a passo) é hoje
**hardcoded para o A-4**: reconstrói `models/players/A-4` com `-Dasan=true` e roda uma fixture da poc
`flight` (que carrega `libA-4.so`, o plugin do A-4) sob LeakSanitizer. Ele **não**
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
e então o binário do core apontando pro seu cenário, com `LSAN_OPTIONS=suppressions=<seu
arquivo de supressões>` (comece copiando [`tests/memory/asan.supp`](tests/memory/asan.supp) — as
supressões de lá são do framework MIXR em si, não deste repositório, então valem para qualquer
modelo).

## 7. Onde consultar durante o trabalho

Três camadas diferentes, para perguntas diferentes:

**Aprender o framework do zero** — comece pelos dois manuais técnicos completos vendorizados,
[`docs/books/mixr-report.pdf`](docs/books/mixr-report.pdf) (MIXR) e
[`docs/books/bt-report.pdf`](docs/books/bt-report.pdf) (BehaviorTree.CPP): são a leitura de ponta a
ponta pensada para quem nunca viu o framework.
[`contexts/MIXR-CONTEXT.md`](contexts/MIXR-CONTEXT.md),
[`contexts/MIXR-PATTERN-CONTEXT.md`](contexts/MIXR-PATTERN-CONTEXT.md) e
[`contexts/BTCPP-CONTEXT.md`](contexts/BTCPP-CONTEXT.md) são a destilação vendorizada dos dois
PDFs — um resumo para consulta pontual enquanto essa leitura está em andamento, ou depois dela —,
e `contexts/src/` (vendorizado e versionado no próprio repositório) tem o fonte completo das duas
árvores para quando nem os PDFs nem a destilação bastarem. [`docs/manual/`](docs/manual/) (`make
open-docs`, página estática) complementa com cinco visões interativas: a animação do ciclo de
execução do MIXR sobre uma árvore de componentes real, um catálogo buscável das classes do fork
(fábrica, slots, participação por fase), um ensaio sobre a cadeia de decisão `FlightAgentTC → UBF
→ BehaviorTree`, entre outras.

**Consulta rápida sobre uma classe/API específica (inclusive por um agente de IA)** — os três
`contexts/*-CONTEXT.md` (`MIXR-CONTEXT.md`, `MIXR-PATTERN-CONTEXT.md`, `BTCPP-CONTEXT.md`) são
destilações voltadas a resposta rápida: é o recurso indicado aqui, não os PDFs completos acima
([`mixr-report.pdf`](docs/books/mixr-report.pdf)/[`bt-report.pdf`](docs/books/bt-report.pdf)), que
continuam sendo o ponto de partida recomendado para quem está aprendendo do zero (ver acima).
Quando a destilação não basta ou parece contraditória, a autoridade final é o fonte vendorizado em
`contexts/src/` (versionado neste repositório) ou os headers instalados pelo Conan. O agente
`mixr-vendor-lookup` (`.claude/agents/`) já sabe essa ordem de consulta e devolve só o resumo, sem
despejar C++ de terceiro na conversa.

**`.claude/` — automação e contexto para quem trabalha com um agente Claude Code neste repo**:

- `.claude/rules/*.md` carregam contexto extra sozinhas quando você edita um caminho que casa com
  o glob delas (`app/`+`src/`+`libs/`, `models/`, arquivos `.edl`/`.edl.in`/`.edl.frag`) — não
  precisam ser lidas à mão, mas valem uma olhada se quiser entender por que um hook bloqueou algo.
- `.claude/hooks/*.sh` rodam automaticamente depois de editar os arquivos correspondentes: core
  opaco (`check-core-opaco.sh`), colisão de nome de fábrica (`check-colisao-fabrica.sh`) e lint de
  EDL (`check-edl-lint.sh`) — a mesma checagem que `make test` (suíte `tools`) ou
  `python3 src/ui/scripts/edl_lint.py <arquivo>` direto fariam, só que sem esperar o próximo build.
- `.claude/skills/README.md` e `.claude/mcp/README.md` documentam por que não há nenhum dos dois
  hoje, e quando criar um.

## 8. Exemplo guiado completo (opcional): um nó novo do A-4, do C++ até um cenário de sandbox

**Opcional — não é parte do roteiro de "escrever um modelo do zero".** As seções 2–6 cobrem isso,
a partir do `template/`. Esta seção é um estudo de caso à parte, bem mais longo (código real,
~600 linhas): como estender um modelo que **já existe**, usando o A-4
(`models/players/A-4/`, o modelo de produção) como referência viva, não um exemplo inventado. O
fio condutor é seguir um nó real de ponta a ponta — `FuelLowCondition` (uma condição) e
`ReportAndEvadeAction` (uma ação), os dois já em produção — desde o C++ até aparecer rodando num
cenário de `sandbox/`. Cada subseção termina com a generalização: o que muda se você estiver
escrevendo um nó **seu**, novo.

> **Namespace simplificado nos exemplos abaixo, de propósito.** O A-4 real já aninha TODO o
> próprio namespace — `domain::`/`bt_nodes::` incluídos — sob `mixr::models::xA_4` (a mesma
> obrigação da seção 3 acima, "namespace aninhado sob `mixr::models::x<nome>`"): ele nasceu como
> a exceção histórica, com os dois soltos no escopo global, mas isso já foi corrigido — ver
> `CHANGELOG.md` daquele modelo e `CONTRATO.md`, seção 6, que hoje descreve isto como caso
> encerrado, não como exceção em aberto. Os trechos de código desta seção 8 OMITEM esse wrapper
> externo (`namespace mixr { namespace models { namespace xA_4 { ... } } }`) só para manter o
> foco na anatomia de cada nó — copie a FORMA (classe, `tick()`, portas, interface), mas aninhe
> `domain::`/`bt_nodes::` (ou o namespace equivalente) sob `mixr::models::x<seu-modelo>` desde o
> início, como a seção 3/6 já exige.

Mapa de onde cada peça mora, para não se perder (`Station` — citada abaixo, e usada nos exemplos
de EDL como `( ClockStation ... )` — é o objeto raiz de uma simulação MIXR, dono do laço de
execução; a hierarquia completa (`Station` → `WorldModel` → players) está no parágrafo de abertura
do [`README.md`](README.md)):

```
models/players/A-4/
├── include/bt/
│   ├── DecisionContext.hpp       # a interface que os nos enxergam (8.3)
│   ├── NodeContext.hpp           # o que um no recebe no construtor + o que a arvore produz (8.3)
│   ├── bt_factory.hpp/.cpp       # registro dos nos SEM SDK (8.4)
│   ├── bt_factory_sdk.hpp/.cpp   # registro dos nos que PRECISAM do SDK -- xinfer/xpyembed (8.4)
│   └── nodes/*.hpp               # um header por no (8.1, 8.2)
├── src/bt/nodes/*.cpp            # a implementacao de cada no (8.1, 8.2)
├── configs/flight_tree*.xml      # a(s) arvore(s) -- onde o no e USADO (8.5)
├── tools/
│   ├── dump_tree_model.cpp       # gera o <TreeNodesModel> pro Groot (8.5)
│   └── update_bt_models.py       # sincroniza esse bloco em TODA arvore de configs/ (8.5)
└── tests/tree/                   # carrega a arvore de PRODUCAO sem Station nenhuma (8.6)
```

### 8.1 Anatomia de uma Condition: `FuelLowCondition`

`FuelLowCondition` decide se o combustível já caiu abaixo da reserva do avião mais uma margem que
a própria árvore pode ajustar. É o nó mais simples do modelo — uma condição que só lê, nunca
escreve:

```cpp
// models/players/A-4/include/bt/nodes/FuelLowCondition.hpp
#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace bt_nodes {

// SUCCESS quando o combustivel (lido do JSBSim, nao de um modelo nosso)
// esta abaixo da reserva do comportamento mais a margem do XML.
//
// PORT 'margin': convencao do BehaviorTree.CPP para parametrizar um no pelo
// XML (providedPorts + getInput). A reserva e propriedade da AERONAVE
// (slot EDL do BtBehavior); a margem e propriedade da ARVORE.
class FuelLowCondition final : public BT::ConditionNode
{
public:
   FuelLowCondition(const std::string& name, const BT::NodeConfiguration& config,
                    const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
```

```cpp
// models/players/A-4/src/bt/nodes/FuelLowCondition.cpp
#include "bt/nodes/FuelLowCondition.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

FuelLowCondition::FuelLowCondition(const std::string& name, const BT::NodeConfiguration& config,
                                   const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::PortsList FuelLowCondition::providedPorts()
{
   return { BT::InputPort<double>("margin", 0.0,
                                  "margem somada a reserva de combustivel (fracao 0..1)") };
}

BT::NodeStatus FuelLowCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   double margin{};
   const BT::Optional<double> input{getInput<double>("margin")};
   if (input) margin = input.value();

   const auto& snap = context_.behavior->snapshot();
   const bool low{snap.fuelFraction < (context_.behavior->getFuelReserve() + margin)};
   return low ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
```

Três coisas a notar, generalizáveis para qualquer `Condition` sua:

- **`providedPorts()`** é a lista de atributos que o XML pode passar (`<FuelLow margin="0.05"/>`)
  — cada `BT::InputPort<T>(nome, default, descrição)` vira um `<input_port>` no `<TreeNodesModel>`
  (seção 8.5) e uma entrada lida via `getInput<T>("nome")` dentro de `tick()`.
- **`tick()` só enxerga o mundo através de `context_.behavior`** — nunca um `mixr::models::Player`,
  nunca um header do MIXR. `context_.behavior` é do tipo `bt_nodes::DecisionContext*` (seção 8.3),
  a interface abstrata — é isso que mantém este `.cpp` compilável sem MIXR (testável sem
  `Station`, seção 8.6).
- **`context_.behavior == nullptr` é checado primeiro, sempre.** Em `dump-tree-model` (seção 8.5)
  a factory é montada com um `NodeContext{}` vazio — `behavior` fica `nullptr` de propósito, e
  nenhum nó chega a ser de fato instanciado ali (só o manifesto de portas é lido) — mas o padrão
  defensivo é o mesmo em todo nó do projeto.

### 8.2 Anatomia de uma Action: `ReportAndEvadeAction`

`ReportAndEvadeAction` executa a manobra de evasão e, se o contato ainda está sendo visto, marca
o pedido de alerta tático para os outros aviões — um exemplo de `Action` que **escreve** na
decisão em vez de só ler:

```cpp
// models/players/A-4/include/bt/nodes/ReportAndEvadeAction.hpp
#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Manobra de evasao E pede a transmissao do alerta aos demais avioes.
class ReportAndEvadeAction final : public BT::SyncActionNode
{
public:
   ReportAndEvadeAction(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
```

```cpp
// models/players/A-4/src/bt/nodes/ReportAndEvadeAction.cpp
#include "bt/nodes/ReportAndEvadeAction.hpp"

#include "domain/ThreatPolicy.hpp"
#include "bt/DecisionContext.hpp"

namespace bt_nodes {

ReportAndEvadeAction::ReportAndEvadeAction(const std::string& name,
                                           const BT::NodeConfiguration& config,
                                           const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

//------------------------------------------------------------------------------
// O no NAO calcula a manobra: ele so entrega o comando que a politica fixou
// na entrada da evasao (ver domain/ThreatPolicy.hpp -- o alvo e calculado uma
// vez e mantido, para o piloto automatico ter para onde convergir).
//
// Dois rotulos, porque sao dois estados diferentes e vale ve-los no status:
//    EVADE  -- quebrando COM o intruso na tela
//    BREAK  -- terminando a quebra no arrasto da histerese, ja sem pista
//------------------------------------------------------------------------------
BT::NodeStatus ReportAndEvadeAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const domain::ThreatPolicy& policy{context_.behavior->threatPolicy()};
   if (!policy.engaged()) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   FlightDecision& decision{context_.behavior->decision()};

   decision.take(policy.command(), policy.contactLive() ? "EVADE" : "BREAK");

   // O "influencia os demais": este no NAO alcanca outro player -- ele so
   // marca o pedido. Quem transmite e o AlertDatalink, na fase 1 do frame
   // seguinte, com a mensagem chegando aos outros como evento nativo.
   //
   // So se avisa o que se esta VENDO: no arrasto da histerese a posicao do
   // contato ja e velha, e retransmiti-la manteria os outros convergindo
   // para um ponto que nao vale mais.
   if (policy.contactLive()) {
      decision.broadcastAlert = true;
      decision.alertContactName = snap.contactName;
      decision.alertNorthM = snap.contactNorthM;
      decision.alertEastM = snap.contactEastM;
      decision.alertAltitudeM = snap.contactAltitudeM;
      decision.alertRangeM = snap.contactRangeM;
   }

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
```

`providedPorts()` volta vazio (`{}`) — este nó não é parametrizado pelo XML, ao contrário de
`FuelLowCondition`. O que ele **produz** é `FlightDecision& decision()` — a estrutura que a árvore
inteira preenche a cada tick (seção 8.3) — via `decision.take(comando, rótulo)`. Repare que o nó
**não fala com outro avião**: ele só marca `broadcastAlert = true`; quem de fato transmite é
`xnative::AlertDatalink`, fora da árvore, na fase seguinte do frame. Generalizando: uma `Action`
sua também só toca `context_.behavior->decision()` (para comandar algo) e/ou os planos
(`patrolPlan()`/`rtbPlan()`, se precisar de estado que sobrevive entre ticks) — nunca um objeto
MIXR direto.

### 8.3 A interface que mantém os nós livres do MIXR

Os dois nós acima só tocam `bt_nodes::DecisionContext` — nunca `ubf::BtBehavior` (a implementação
concreta, que mora do lado de dentro do `.so` e inclui headers MIXR pesados). A interface
completa, hoje com 13 métodos (o modelo cresceu desde que este exemplo foi escrito — acrescentou
acrobacia e ameaça de RWR (*Radar Warning Receiver*, receptor que detecta emissão de radar hostil
de forma passiva); o padrão de "só domínio, nunca MIXR" continua o mesmo em cada método
novo):

```cpp
// models/players/A-4/include/bt/DecisionContext.hpp
#pragma once

#include "bt/NodeContext.hpp"
#include "domain/AerobaticPlan.hpp"
#include "domain/LaunchPolicy.hpp"
#include "domain/PatrolPlan.hpp"
#include "domain/RtbPlan.hpp"
#include "domain/ThreatPolicy.hpp"
#include "domain/WorldView.hpp"

namespace bt_nodes {

class DecisionContext
{
public:
   virtual ~DecisionContext() = default;

   // percepcao do frame
   virtual const domain::WorldView& snapshot() const = 0;

   // o que a arvore preenche neste tick
   virtual FlightDecision& decision() = 0;

   // planos de voo, com o estado que sobrevive entre ticks
   virtual domain::PatrolPlan& patrolPlan() = 0;
   virtual domain::RtbPlan& rtbPlan() = 0;
   virtual const domain::ThreatPolicy& threatPolicy() const = 0;

   // quando fazer a proxima acrobacia e por quanto tempo mante-la -- o no
   // ( SlowRoll ) so avanca/le este plano, quem o semeia e configurePlans()
   virtual domain::AerobaticPlan& aerobaticPlan() = 0;

   // SEGUNDA instancia de ThreatPolicy, dedicada a ameaca de RWR (emissor
   // hostil detectado passivamente) -- independente da threatPolicy() acima
   virtual const domain::ThreatPolicy& rwrThreatPolicy() const = 0;

   // alcance/cone dentro dos quais o disparo de missil e permitido
   virtual const domain::LaunchEnvelope& launchEnvelope() const = 0;

   // parametros do ciclo e dos slots do EDL
   virtual double getFrameDt() const = 0;
   virtual double getFuelReserve() const = 0;
   virtual double getSupportSpeedKts() const = 0;

   // O piso anti-CFIT (domain/TerrainFloor.hpp) -- so domain::ThreatPolicy::
   // breakCommand() aplicava este piso; RTB e SUPPORT comandavam altitude
   // (rtbAltitude fixo do EDL, ou a altitude ABSOLUTA de um contato
   // reportado por outro player) sem NENHUMA validacao contra o terreno em
   // runtime. Cada no que comanda altitude fora do ramo de evasao deve
   // passar por aqui antes de decision().take() -- ver
   // ReturnToBaseAction/SupportAlertAction/PatrolAction.
   virtual double clampAltitudeToTerrain(double altitudeM) const = 0;

   // ha altitude de sobra para COMECAR uma acrobacia agora? -- so a borda
   // Idle->Rolling de AerobaticPlan::update() consulta isto
   virtual bool hasAerobaticAltitudeMargin() const = 0;
};

} // namespace bt_nodes
```

`ubf::BtBehavior` implementa esta interface sem escrever um método novo — as assinaturas já eram
os próprios membros dele. O que ela compra: `bt/nodes/*.cpp` e `bt/bt_factory.cpp` passam a
compilar contra BehaviorTree.CPP + `domain/` apenas, nunca contra o MIXR — o que abre a porta para
o teste da seção 8.6.

E o que todo nó recebe no construtor, mais o que a árvore produz a cada tick:

```cpp
// models/players/A-4/include/bt/NodeContext.hpp
#pragma once

#include "domain/FlightCommand.hpp"

#include <string>

namespace bt_nodes {

//------------------------------------------------------------------------------
// FlightDecision -- o que a arvore PRODUZ num tick.
//
// Os nos nao tocam em nenhum objeto MIXR: eles so preenchem esta estrutura.
// Quem transforma isto em atuacao e a xnative::FlightAction do UBF.
//------------------------------------------------------------------------------
struct FlightDecision
{
   bool taken{};
   domain::FlightCommand command{};
   std::string label{"?"};

   // pedido de transmissao do alerta tatico para os outros avioes
   bool broadcastAlert{};
   std::string alertContactName;
   double alertNorthM{};
   double alertEastM{};
   double alertAltitudeM{};
   double alertRangeM{};

   void reset() { *this = FlightDecision{}; }

   void take(const domain::FlightCommand& cmd, const std::string& text)
   {
      taken = true;
      command = cmd;
      label = text;
   }
};

// O ponteiro e para a INTERFACE (bt/DecisionContext.hpp), nao para a classe
// concreta: e o que mantem os nos compilaveis sem o MIXR.
class DecisionContext;

struct NodeContext
{
   DecisionContext* behavior{};
};

} // namespace bt_nodes
```

### 8.4 Registrando um nó na fábrica

Um nó só existe para a árvore depois de registrado numa `BT::BehaviorTreeFactory`. O A-4 usa DOIS
pontos de registro — `bt_factory.cpp` (sem MIXR, compilado também no alvo de teste `test-tree`) e
`bt_factory_sdk.cpp` (para nós que dependem do SDK — hoje só `OnnxPolicy`/`OnnxScore`/`PyDecide`,
que linkam `xinfer`/`xpyembed`):

```cpp
// models/players/A-4/src/bt/bt_factory.cpp
#include "bt/bt_factory.hpp"

#include "bt/nodes/AlertReceivedCondition.hpp"
#include "bt/nodes/ContactDetectedCondition.hpp"
#include "bt/nodes/FuelLowCondition.hpp"
#include "bt/nodes/NavigateAction.hpp"
#include "bt/nodes/PatrolAction.hpp"
#include "bt/nodes/ReportAndEvadeAction.hpp"
#include "bt/nodes/ReturnToBaseAction.hpp"
#include "bt/nodes/SupportAlertAction.hpp"

namespace bt_nodes {

namespace {

// registerBuilder<T>(ID, builder) e o ponto de extensao do BehaviorTree.CPP
// v3 para construtores com argumentos extras (a sobrecarga variadica de
// registerNodeType so existe em versoes posteriores).
template <typename NodeType>
void registerWithContext(BT::BehaviorTreeFactory& factory, const std::string& id,
                         const NodeContext& context)
{
   BT::NodeBuilder builder{
      [context](const std::string& name, const BT::NodeConfiguration& config) {
         return std::make_unique<NodeType>(name, config, context);
      }};
   factory.registerBuilder<NodeType>(id, builder);
}

}

// Registrar um no aqui e' so metade do trabalho: o Groot (deps/groot/,
// CLAUDE.md "Groot -- editor e monitor ao vivo") NAO enxerga estas classes --
// ele e' um app a parte, nunca viu este .so. Os configs/flight_tree*.xml
// de producao carregam um <TreeNodesModel> colado a mao, com o MESMO ID
// desta chamada, so' pra ele reconhecer os nos. Registrou um no novo aqui ou
// em bt_factory_sdk.cpp? Atualize o bloco em TODOS esses arquivos tambem
// (make update-bt faz isso por voce, ver a secao 8.5 logo abaixo), ou o
// Groot recusa a arvore com "This model has not been registered: <ID>".
void registerNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context)
{
   registerWithContext<FuelLowCondition>(factory, "FuelLow", context);
   registerWithContext<ReturnToBaseAction>(factory, "ReturnToBase", context);
   registerWithContext<ContactDetectedCondition>(factory, "ContactDetected", context);
   registerWithContext<ReportAndEvadeAction>(factory, "ReportAndEvade", context);
   registerWithContext<AlertReceivedCondition>(factory, "AlertReceived", context);
   registerWithContext<SupportAlertAction>(factory, "SupportAlert", context);
   registerWithContext<PatrolAction>(factory, "Patrol", context);
   registerWithContext<NavigateAction>(factory, "Navigate", context);
   // ... o modelo de producao registra mais nos aqui hoje (acrobacia,
   // ameaca de RWR, disparo de missil) -- omitidos deste exemplo porque o
   // padrao de registro e identico; ver models/players/A-4/src/bt/bt_factory.cpp
   // para a lista completa e atual.
}

} // namespace bt_nodes
```

`bt_factory_sdk.cpp` segue o MESMO molde (o mesmo `registerWithContext<T>` local, repetido porque
este arquivo é compilado num alvo diferente que não pode depender de `bt_factory.cpp`):

```cpp
// models/players/A-4/src/bt/bt_factory_sdk.cpp (miolo)
void registerSdkNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context)
{
   registerWithContext<OnnxScoreCondition>(factory, "OnnxScore", context);
   registerWithContext<OnnxPolicyAction>(factory, "OnnxPolicy", context);
   registerWithContext<PyDecideAction>(factory, "PyDecide", context);
}
```

**Generalização — para o SEU nó novo**, três linhas mecânicas: incluir o header do nó, e uma
chamada `registerWithContext<SeuTipo>(factory, "SeuID", context)` dentro de `registerNodes()` (ou
`registerSdkNodes()`, se o nó depender do SDK). `"SeuID"` é a tag que vai valer no XML — sem
espaço, sem acento: é o nome que aparece como `<SeuID/>` na árvore.

### 8.5 Colando o nó na árvore e mantendo o `<TreeNodesModel>` em dia

Trecho real de `configs/flight_tree.xml` — a tag `<FuelLow margin="0.05"/>` é a MESMA string
`"FuelLow"` passada em `registerWithContext` acima:

```xml
<Fallback name="root">

  <Sequence name="rtb_sequence">
    <FuelLow margin="0.05"/>
    <ReturnToBase/>
  </Sequence>

  <Sequence name="engage_sequence">
    <ContactDetected/>
    <ReportAndEvade/>
  </Sequence>

  <Sequence name="support_sequence">
    <AlertReceived/>
    <SupportAlert/>
  </Sequence>

  <Patrol/>

</Fallback>
```

Colar a tag no XML já basta para a árvore de PRODUÇÃO funcionar (`tinyxml2`, o parser que o
executor usa, não exige o `<TreeNodesModel>` — ele só lê `<BehaviorTree>`). O que falta é
puramente para o **Groot** — sem um `<TreeNodesModel>` descrevendo `ID`/portas, ele recusa o
arquivo com *"This model has not been registered: FuelLow"*. Esse bloco não é escrito à mão:

- **`make create-bt`** (dentro de `models/players/A-4/`) — gera `configs/bt.xml`: uma árvore
  vazia (`<Fallback name="root"/>`) mais o `<TreeNodesModel>` já populado com TODOS os nós que
  `registerNodes()`/`registerSdkNodes()` registram hoje. Recusa se `configs/bt.xml` já existir.
- **`make update-bt`** — depois de registrar um nó novo (seção 8.4), este é o passo que sincroniza
  o `<TreeNodesModel>` de **toda** árvore de `configs/` (descobertas por conteúdo — todo `.xml`
  com `<BehaviorTree>` dentro, nunca uma lista fixa de nomes) com o que a fábrica de fato exporta
  agora.

Por baixo dos dois, `tools/dump_tree_model.cpp` monta a MESMA `BT::BehaviorTreeFactory` que o
modelo real monta e chama a função nativa do BT.CPP para isto:

```cpp
// models/players/A-4/tools/dump_tree_model.cpp (miolo do main())
BT::BehaviorTreeFactory factory;

// behavior=nullptr e seguro aqui: registerBuilder<T>() so guarda um
// construtor (lambda) na factory, nunca instancia um no. Nenhum no e de
// fato criado -- so' o manifesto (ID + portas) e' lido por
// writeTreeNodesModelXML(), via factory.manifests().
bt_nodes::NodeContext context;
bt_nodes::registerNodes(factory, context);
bt_nodes::registerSdkNodes(factory, context);

std::string model = extractTreeNodesModel(factory);   // extrai so o miolo <TreeNodesModel>...
```

O modo `--skeleton [ID]` (usado por `make create-bt`) tem uma checagem que vale a pena conhecer,
achada por auditoria: recusa gerar uma árvore nova cujo `ID` colida com o de um nó já registrado
(`Patrol`, `FuelLow`, `Navigate`...) — porque o BT.CPP resolve uma tag XML nua primeiro contra
`factory.builders()` e só depois contra `tree_roots()`, então uma subárvore com ID colidente
resolveria **silenciosamente** para o nó, nunca para a subárvore, sempre que referenciada pela
mesma tag nua noutra árvore.

E `tools/update_bt_models.py` (chamado por `make update-bt` como
`python3 tools/update_bt_models.py --binary build/tools/dump-tree-model`) roda esse binário e
aplica o resultado em cada `.xml` de `configs/` — substituindo o bloco existente, ou inserindo
antes de `</root>` se a árvore ainda não tiver nenhum. Duas armadilhas já resolvidas no próprio
script, que valem a pena conhecer antes de mexer nele: a regex que acha o bloco existente tolera
qualquer indentação (`[ \t]*<TreeNodesModel>...`, não só exatamente dois espaços — o Groot
resalva com indentação própria), e todo comentário XML é mascarado antes da busca — um comentário
que **menciona** a tag (como "o bloco `<TreeNodesModel>` abaixo é gerado, não edite à mão") não
pode ser confundido com o bloco de verdade, ou o script engoliria a árvore inteira ao "substituir".

**Rede de segurança automática**: a suíte `tree` (`meson test`, dentro de `models/players/A-4/`)
tem o alvo `tree-model-sync`, que roda `update_bt_models.py --check` — falha se qualquer árvore
estiver desatualizada. Esquecer `make update-bt` depois de registrar um nó novo quebra `make test`
antes de alguém precisar descobrir isso tentando abrir a árvore no Groot.

Editar/visualizar a árvore no Groot em si (arrastar nós, conectar, salvar) já está coberto na
subseção Groot da seção 4, acima — não repetido aqui.

### 8.6 Testando o nó sem nenhuma simulação

`models/players/A-4/tests/tree/` carrega o `flight_tree.xml` de **produção** contra um
`FakeDecisionContext` (uma implementação de teste da interface da seção 8.3) — sem `Station`, sem
MIXR, sem terreno. Dado um `WorldView` sintético (ex.: combustível baixo, ou um contato
detectado), o teste confirma qual ramo do `Fallback` venceu. `make test`, de dentro de
`models/players/A-4/`, roda essa suíte (`tree`) junto com `domain` e `native` — é a camada mais
barata para validar que um nó novo, colado numa `Sequence` nova, muda a prioridade certa.

### 8.7 Recompilando e verificando o `.so`

Mesmo padrão já usado na seção 5.1, nomeado para o A-4:

```bash
make -C models/players/A-4 build test install-core   # so este modelo, autocontido
cd ../../..   # de volta a raiz do repositorio, se necessario
make install                                          # sync-plugins: plugins/ -> dist/
nm -D --defined-only dist/lib/mixr-plugins/libA-4.so | grep ' T '   # 1 linha
ldd dist/lib/mixr-plugins/libA-4.so | grep 'not found'              # vazio
```

`make -C models/players/A-4 install-core` deposita em `plugins/` (a raiz do repositório) — só o
`make install` seguinte, na raiz, sincroniza `plugins/` → `dist/`, o lugar onde um cenário de
verdade procura (ver a seção "Desacoplando `models` de `dist/`" em `CLAUDE.md`).

### 8.8 Carregando o modelo num cenário: o bloco `PluginModule` real

Trecho verbatim de `src/poc/dis/flight/configs/scenario.edl.in` — a PRIMEIRA entrada de
`components:` da `Station`:

```
components: {

   plugins: ( PluginLoader
      searchPaths: {
         "./dist/lib/mixr-plugins/"
      }
      modules: {
         ( PluginModule
            file:     "libA-4.so"
            provides: { AlertDatalink TacticalAlert ThreadTagProbe FlightAgentTC
                        FlightState BtBehavior AltitudeSafetyBehavior
                        RLBridgeBehavior FlightAction }
         )
      }
   )
   ...
```

**Por que este bloco tem de vir ANTES de qualquer outro uso** — não é estilo, é a gramática: a
produção `arglist` do `edl_parser` é recursiva à esquerda, então formas irmãs são construídas na
ordem do TEXTO, e `parse()` (`factory(name)` → `setSlotByName` → `isValid()`) roda no
fecha-parênteses de cada uma. A carga do `.so` acontece dentro do `isValid()` do `PluginLoader` —
fora de ordem, o parser chega numa classe do plugin sem ninguém que responda por ela, e
`mixrFactory` aborta explicando exatamente isso (sem SIGSEGV, sem silêncio).

`provides:` é igualdade EXATA de conjunto contra o que o `.so` exporta hoje — 9 nomes, para
`libA-4.so`. Se o nó novo que você acabou de registrar (seções 8.4–8.5) é um nó de
**BehaviorTree**, `provides:` **não muda** — nome de nó de árvore não é nome de fábrica MIXR, os
dois vivem em registros completamente diferentes (`BT::BehaviorTreeFactory` vs.
`mixr::base::factory`). `provides:` só muda quando você acrescenta uma CLASSE MIXR nova (mais uma
entrada em `xnative::factory.cpp`) — nesse caso, em TODO cenário que carrega o `.so`, não só o que
motivou a mudança (já documentado em `.claude/rules/models-plugin.md`).

E o trecho, mais abaixo no mesmo arquivo, onde o agente do UBF entra dentro de cada player:

```
falcon1: ( Aircraft
   ...
   components: {
      ...
      // ------------------------------------------------------
      // O AGENTE.
      //
      // POR QUE E O ULTIMO DA LISTA: a lista de componentes e
      // percorrida na ordem declarada, e tres coisas rodam na
      // fase 3 -- Autopilot::process(), AirTrkMgr::process() e a
      // decisao. Declarado por ultimo, o agente ja enxerga as
      // pistas ATUALIZADAS deste frame; o comando que ele grava
      // no Autopilot vale a partir do frame seguinte.
      // ------------------------------------------------------
      agent: ( FlightAgentTC
         state: ( FlightState )
         behavior: ( BtBehavior
            treeFile: "./dist/share/mixr-plugins/A-4/flight_tree.xml"
            patrolHeading:  ( Degrees 90 )
            ...
            fuelReserve:    0.35
            ...
         )
      )
   }
)
```

`treeFile:` é o caminho para onde `make install`/`sync-plugins` publica a árvore junto com o
`.so` (`dist/share/mixr-plugins/A-4/`) — é essa cópia instalada, não o
`configs/flight_tree.xml` do projeto do modelo, que o cenário de fato carrega em runtime.

### 8.9 Rodando num cenário de sandbox

A família `sandbox/A4-*DOF` já carrega `libA-4.so` pelo MESMO mecanismo — prova de que o
padrão da seção 8.8 vale igual em produção e em sandbox
(`sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in`):

```
( ClockStation

   tcPriority: 0.5

   ownship: a4_1

   components: {

      plugins: ( PluginLoader
         searchPaths: {
            "./dist/lib/mixr-plugins/"
         }
         modules: {
            ( PluginModule
               file:     "libA-4.so"
               provides: { AlertDatalink TacticalAlert ThreadTagProbe FlightAgentTC
                           FlightState BtBehavior AltitudeSafetyBehavior
                           RLBridgeBehavior FlightAction }
            )
         }
      )
      ...
```

Para rodar sua própria variante — com o nó novo já valendo, já recompilado (seção 8.7) — sem mexer
em nenhum cenário existente, é a MESMA receita já mostrada na seção 5.2, agora com `flight` (o
cenário de produção do próprio A-4) como base concreta em vez de um placeholder — ele já
referencia `libA-4.so`, então só `treeFile:`/porta Tacview/callsign do EDL copiado precisam mudar,
não o bloco `PluginModule` inteiro:

```bash
mkdir -p sandbox/meu-teste/configs sandbox/meu-teste/data/{logs,recordings,messages}
touch sandbox/meu-teste/data/logs/.gitkeep sandbox/meu-teste/data/recordings/.gitkeep \
      sandbox/meu-teste/data/messages/.gitkeep
cp src/poc/dis/flight/configs/scenario.edl.in sandbox/meu-teste/configs/scenario.edl.in
# editar treeFile:/porta Tacview/callsign dentro da copia, se quiser distingui-la da producao

./build/app/src/app -folder ./sandbox -scenario meu-teste
./build/app/src/app -folder ./sandbox -scenario meu-teste -deterministic 600
```

`sandbox/` é gitignorado por padrão (`/*/` em `sandbox/.gitignore`, com exceções nomeadas para os
exemplos que já vêm no repositório) — sua pasta fica local até você decidir compartilhá-la. Para
publicar como exemplo versionado: acrescentar `!/meu-teste/` à lista de exceções do
`sandbox/.gitignore` e `git add sandbox/meu-teste/`.

### 8.10 Fechando o ciclo

Depois de rodando, confirme que o nó novo não quebrou o determinismo — o mesmo
`tests/determinism/check_determinism.sh` já documentado em `CLAUDE.md`, comparando dumps `frame=`
com 1, 2 e 4 threads de tempo crítico.

Recapitulando o ciclo inteiro, do C++ ao sandbox: **escrever o `.hpp`/`.cpp` do nó (8.1–8.2) →
registrar na fábrica (8.4) → colar a tag no XML e `make update-bt` (8.5) → testar sem simulação
(8.6) → `make ... install-core` + `make install` (8.7) → conferir `provides:`/`agent:` no `.edl`
— já satisfeito se o nó é só de árvore (8.8) → `-folder ./sandbox -scenario <nome>` (8.9)**.
