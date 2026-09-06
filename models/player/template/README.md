# `template` — ponto de partida, EM CAMADAS, para um modelo novo

## O que é isto

A aplicação principal deste repositório (o "host") não decide nada sozinha: ela carrega a lógica
de simulação — percepção, decisão, ação — de uma biblioteca compartilhada (`.so`) compilada à
parte e aberta em tempo de execução, sem que o host precise conhecer o código-fonte dela. Essa
biblioteca é o que este repositório chama de **modelo** (ver [`../README.md`](../../README.md)
para a visão geral de como isso se encaixa no resto do repositório).

Este diretório **não é um modelo de produção** — é um esqueleto compilável e testável, do tamanho
mínimo necessário para mostrar a separação em camadas (`domain/` → `ubf/` → `xnative/`) que os
modelos reais deste repositório usam, com uma única decisão de exemplo (um Schmitt trigger sobre
a altitude do player: "engajado" acima de um limiar, "não engajado" abaixo de outro) percorrendo
as três camadas de ponta a ponta. Ele existe para ser **copiado e transformado** no seu modelo —
ver [`docs/PRIMEIROS-PASSOS.md`](docs/PRIMEIROS-PASSOS.md) para o roteiro mecânico.

**Se você só precisa da prova mínima de que "o contrato de plugin basta"** (sem camadas, um
arquivo só, ~270 linhas), o ponto de partida certo é
[`../fixtures/stub`](../fixtures/stub/README.md) — este diretório aqui serve ao caso oposto: você
sabe que vai precisar de mais de uma decisão coordenada, e quer começar já na forma que vai
crescer melhor. [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) tem a comparação completa entre os
quatro pontos de referência que `models/player/` tem hoje (`stub`, este `template`, `missile`,
`A4`).

## O que tem em cada arquivo

```
template/
├── include/
│   ├── domain/ExampleThreshold.hpp   # a regra pura -- sem MIXR
│   ├── ubf/
│   │   ├── ExampleState.hpp          # percepcao: le o Player, guarda um numero cru
│   │   ├── ExampleBehavior.hpp       # decisao: aplica a regra, tem os slots
│   │   └── ExampleAction.hpp         # atuacao: escreve no xboard (a obrigacao muda)
│   └── xnative/factory.hpp           # registro das 3 classes acima
├── src/                              # a implementacao de cada header, no mesmo layout
├── tests/
│   ├── domain/test_ExampleThreshold.cpp   # 4 casos, sem MIXR, sem Station
│   └── check_contract.sh                  # forma do .so: 1 simbolo T, deps resolvidas
├── docs/
│   ├── ARCHITECTURE.md               # as camadas, o "porque" de cada uma, quando crescer
│   └── PRIMEIROS-PASSOS.md           # o roteiro de copiar isto e virar um modelo com nome proprio
├── CHANGELOG.md
├── Makefile                          # build autocontido -- ver abaixo
└── meson.build
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
cd models/player/template
make build            # compila -> ./dist/lib/mixr-plugins/libtemplate.so (bare `make` so mostra `make help`)
make test             # 4 casos de domain/ (o Schmitt trigger) + a forma do .so
make install-host     # copia o .so para ../../../plugins/ -- ver a proxima secao
```

`make help` lista todos os alvos. `./build` e `./dist` nascem e ficam dentro **deste**
diretório — nada aqui escreve fora dele, exceto `make install-host`.

**Este template nunca é construído pelo `make models` da raiz**, e nenhum cenário existente
aponta para ele — ele não é produção, é ponto de partida. Depois de copiado e renomeado (ver
`docs/PRIMEIROS-PASSOS.md`), o modelo resultante pode (e provavelmente deveria) entrar no fluxo
orquestrado, do mesmo jeito que `A4`/`missile`/`fixtures/stub` já entram.

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
- [`../fixtures/stub/docs/CONTRATO.md`](../fixtures/stub/docs/CONTRATO.md) — a lista completa e
  autoritativa do que QUALQUER modelo precisa fazer para o host carregá-lo e rodar com ele
- [`../README.md`](../../README.md) — visão geral de `models/`, o contrato de plugin, e o fluxo
  de build orquestrado pelo Makefile da raiz
- [`../../../CLAUDE.md`](../../../CLAUDE.md) — visão geral do repositório inteiro
