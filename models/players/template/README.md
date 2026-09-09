# `template` — ponto de partida, EM CAMADAS, para um modelo novo

## O que é isto

A aplicação principal deste repositório (o "host") não decide nada sozinha: ela carrega a lógica
de simulação — percepção, decisão, ação — de uma biblioteca compartilhada (`.so`) compilada à
parte e aberta em tempo de execução, sem que o host precise conhecer o código-fonte dela. Essa
biblioteca é o que este repositório chama de **modelo** (ver [`../../../CLAUDE.md`](../../../../CLAUDE.md),
seção "O MODELO é um plugin, construído numa etapa PRÉVIA", para a visão geral de como isso se
encaixa no resto do repositório).

Este diretório é o **único** ponto de partida copiável deste repositório, e hospeda DOIS artefatos
com papéis diferentes:

- **`template`** (`libtemplate.so`) — o esqueleto compilável e testável, do tamanho mínimo
  necessário para mostrar a separação em camadas (`domain/` → `bt/` → `ubf/` → `xnative/`) que os
  modelos reais deste repositório usam, com uma única decisão de exemplo (um Schmitt trigger sobre
  a altitude do player: "engajado" acima de um limiar, "não engajado" abaixo de outro) percorrendo
  as camadas de ponta a ponta — inclusive uma **árvore de comportamento** de verdade
  (`configs/example_tree.xml`, dois nós), o que dá ao projeto os mesmos alvos de `A-4`:
  `make create-bt`/`make update-bt`/`make open-groot`. Ele existe para ser **copiado e transformado** no seu modelo —
  ver [`docs/PRIMEIROS-PASSOS.md`](docs/PRIMEIROS-PASSOS.md) para o roteiro mecânico.
- **`template_mirror`** (`libtemplate_mirror.so`, fonte em `src/mirror.cpp`) — o mirror de
  contrato que este repositório usa nos próprios testes de plugin (`plugin-modelo-estranho`/
  `plugin-deposito-terceiro`), herdado do extinto `models/players/fixtures/stub`. **NÃO faz parte
  do scaffold copiável** — apague `src/mirror.cpp` e o bloco `template_mirror` de
  `meson.build`/`tests/meson.build` antes de personalizar uma cópia (ver o aviso no topo do
  próprio arquivo e o passo dedicado em `docs/PRIMEIROS-PASSOS.md`).

[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) tem a comparação completa entre os pontos de
referência que `models/players/` tem hoje (este `template` e `A-4`).

## O que tem em cada arquivo

```
template/
├── include/
│   ├── domain/ExampleThreshold.hpp   # a regra pura -- sem MIXR, sem BT.CPP
│   ├── bt/
│   │   ├── NodeContext.hpp           # o que a arvore PRODUZ num tick
│   │   ├── DecisionContext.hpp       # a interface que mantem os nos livres de MIXR
│   │   ├── bt_factory.hpp            # a lista de nos deste modelo (UMA so)
│   │   └── nodes/                    # um no por decisao (1 condicao + 1 acao)
│   ├── ubf/
│   │   ├── ExampleState.hpp          # percepcao: le o Player, guarda um numero cru
│   │   ├── ExampleBehavior.hpp       # decisao: carrega a arvore e tica, tem os slots
│   │   └── ExampleAction.hpp         # atuacao: escreve no xboard (a obrigacao muda)
│   └── xnative/factory.hpp           # registro das 3 classes acima -- SO do artefato 'template'
├── src/
│   ├── domain/ bt/ ubf/ xnative/     # a implementacao de cada header acima, no mesmo layout
│   └── mirror.cpp                    # o SEGUNDO artefato -- NAO e scaffold, ver o aviso no topo
├── configs/example_tree.xml          # a arvore -- DADO do modelo, instalada junto com o .so
├── tools/                            # dump-tree-model + update_bt_models.py (create-bt/update-bt)
├── tests/
│   ├── domain/test_ExampleThreshold.cpp   # 4 casos, sem MIXR, sem Station
│   ├── tree/test_example_tree.cpp         # a arvore de PRODUCAO contra um contexto falso
│   └── check_contract.sh                  # forma de CADA .so: 1 simbolo T, deps resolvidas
├── docs/
│   ├── ARCHITECTURE.md               # as camadas, o "porque" de cada uma, quando crescer
│   ├── CONTRATO.md                   # a lista completa do que QUALQUER modelo tem que fazer
│   └── PRIMEIROS-PASSOS.md           # o roteiro de copiar isto e virar um modelo com nome proprio
├── CHANGELOG.md
├── Makefile                          # build autocontido -- ver abaixo
└── meson.build                       # os DOIS shared_module(): template_lib, template_mirror_lib
```

## Compilar e testar, sozinho

Este diretório é um projeto de build **independente** (tem seu próprio arquivo de projeto Meson e
seu próprio `Makefile`) — não é preciso abrir o resto do repositório para compilar só isto.

O único pré-requisito é que o projeto principal do repositório (a pasta acima de `models/`, três
níveis para cima daqui) já tenha rodado, **uma vez**, os dois passos que publicam o que um modelo
precisa para linkar: o SDK de plugin e os pacotes de terceiros que o MIXR usa.

```bash
# uma vez, na raiz do repositorio:
cd ../../.. && make configure && make sdk

# daqui em diante, só aqui dentro:
cd models/players/template
make build            # compila -> ./dist/lib/mixr-plugins/{libtemplate.so,libtemplate_mirror.so} (bare `make` so mostra `make help`)
make test             # domain/ (o Schmitt trigger) + a arvore + a forma dos DOIS .so
make install-host     # copia os dois .so para ../../../plugins/ -- ver a proxima secao
```

`make help` lista todos os alvos. `./build` e `./dist` nascem e ficam dentro **deste**
diretório — nada aqui escreve fora dele, exceto `make install-host`.

**Este template nunca é construído pelo `make models` da raiz** (que descobre projetos de
produção por `find`, e exclui `template/` de propósito) — mas o alvo `models:` do Makefile raiz
instala o artefato `template_mirror` à parte, incondicionalmente, porque os testes de plugin do
host dependem dele (ver a seção anterior). O artefato `template` (o scaffold em si) não é
instalado por nenhum alvo automático — nenhum cenário existente aponta para ele. Depois de copiado
e renomeado (ver `docs/PRIMEIROS-PASSOS.md`), o modelo resultante pode (e provavelmente deveria)
entrar no fluxo orquestrado, do mesmo jeito que `A-4` já entra.

## Por que existe um passo separado para "publicar no host"

Um cenário só encontra um `.so` de modelo dentro de uma pasta específica, relativa à raiz do
repositório inteiro (não à raiz deste diretório). Compilar aqui dentro (`make build`/`make test`) já é
suficiente para editar o código e conferir que ele continua válido; para que um cenário de
verdade consiga carregar o resultado, o `.so` também precisa existir naquela pasta da raiz — e
`make install-host` é o único alvo deste `Makefile` que copia algo para lá (e mesmo assim só até
`plugins/`, o depósito compartilhado com terceiros — quem sincroniza dali para `dist/`, onde um
cenário de fato procura, é o `make install` do projeto raiz).

## Usando este diretório como ponto de partida para um modelo novo

Ver [`docs/PRIMEIROS-PASSOS.md`](docs/PRIMEIROS-PASSOS.md) — o roteiro completo, com os comandos
exatos de renomeação (projeto, módulo, namespace C++) e a ordem sugerida para substituir o
exemplo pela sua decisão de verdade.

## Se você quiser mais contexto

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — as camadas, o porquê de cada uma, e quando
  crescer para uma árvore de comportamento
- [`docs/CONTRATO.md`](docs/CONTRATO.md) — a lista completa e autoritativa do que QUALQUER modelo
  precisa fazer para o host carregá-lo e rodar com ele
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
  PRÉVIA" — visão geral de `models/`, o contrato de plugin, e o fluxo de build orquestrado pelo
  Makefile da raiz
