# `paratrooper` — o corpo físico e a decisão de um paraquedista

## O que é isto

Um modelo MIXR (`libparatrooper.so`, `.so` carregado por `dlopen`, ver
[`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
PRÉVIA") que simula o ciclo de vida de um paraquedista: **queda livre → paraquedas aberto →
pousado**. Não pilota, não sensoreia, não decide combate — a única decisão é uma FSM de três
estágios, dirigida pela altitude acima do solo (AGL).

Deriva de `mixr::models::Effect` (o mesmo idioma nativo de `Chaff`/`Decoy`/`Flare`), e nasce
compatível com o mecanismo de liberação que `models/players/C-130` já usa hoje (um placeholder,
`C130ParatrooperPlaceholder`) — a substituição do placeholder por este modelo é tarefa futura, e é
puramente EDL (ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)). **Esta versão não está ligada
ao C-130** — é demonstrada sozinha, em [`src/poc/paratrooper-drop`](../../../src/poc/paratrooper-drop),
com os paraquedistas já em queda a partir de uma altitude inicial.

## O que tem em cada arquivo

```
paratrooper/
├── include/
│   ├── domain/ParachuteFsm.hpp         # a FSM pura -- sem MIXR, sem BT.CPP
│   ├── bt/
│   │   ├── NodeContext.hpp             # o que a arvore PRODUZ num tick (JumpDecision)
│   │   ├── DecisionContext.hpp         # a interface que mantem os nos livres de MIXR
│   │   ├── bt_factory.hpp              # a lista de nos deste modelo
│   │   └── nodes/                      # IsStageCondition (leitura pura) + SetStageLabelAction
│   ├── ubf/
│   │   ├── ParatrooperState.hpp        # percepcao: AGL + validade do terreno
│   │   ├── ParatrooperBtBehavior.hpp   # decisao: avanca a FSM, tica a arvore, tem os slots
│   │   └── ParatrooperAction.hpp       # atuacao: comanda o Paratrooper, escreve no xboard
│   └── xnative/
│       ├── Paratrooper.hpp             # o CORPO fisico -- Effect + fisica por estagio
│       ├── ParatrooperAgentTC.hpp      # o agente -- decide na FASE 3 do frame de tempo critico
│       └── factory.hpp                 # registro das 5 classes acima
├── src/                                 # a implementacao de cada header acima, no mesmo layout
├── configs/paratrooper_jump_tree.xml    # a arvore -- DADO do modelo, instalada junto com o .so
├── tools/                               # dump-tree-model + update_bt_models.py (create-bt/update-bt)
├── tests/
│   ├── domain/test_ParachuteFsm.cpp     # a FSM pura, sem MIXR, sem Station
│   ├── tree/test_paratrooper_tree.cpp   # a arvore de PRODUCAO contra um contexto falso
│   ├── native/                          # as classes MIXR proprias, SEM Station (bancada)
│   └── check_contract.sh                # forma do .so: 1 simbolo T, deps resolvidas
├── docs/ARCHITECTURE.md                 # as decisoes de design, o "porque" de cada uma
├── CHANGELOG.md
├── Makefile                             # build autocontido -- ver abaixo
└── meson.build                          # o unico shared_module(): paratrooper_lib
```

## Compilar e testar, sozinho

Este diretório é um projeto de build **independente** — não é preciso abrir o resto do
repositório para compilar só isto.

```bash
# uma vez, na raiz do repositorio:
cd ../../.. && make configure && make sdk

# daqui em diante, so aqui dentro:
cd models/players/paratrooper
make build            # compila -> ./dist/lib/mixr-plugins/libparatrooper.so
make test             # domain (a FSM) + tree + native + a forma do .so -- 5 testes
make install-host     # copia o .so + a arvore para ../../../plugins/ -- ver a proxima secao
```

`make help` lista todos os alvos.

## Por que existe um passo separado para "publicar no host"

`make install-host` só copia até `../../../plugins/`, o depósito compartilhado com terceiros —
quem sincroniza dali para `dist/`, onde um cenário de fato procura, é `make install` do projeto
raiz. O fluxo do dia a dia na raiz é `make configure && make models && make install`.

## Nomes de fábrica

`Paratrooper` (o corpo físico), `ParatrooperAgentTC` (o agente), `ParatrooperState`,
`ParatrooperBtBehavior`, `ParatrooperAction`. O `.edl` de qualquer cenário que carregue este
plugin precisa declarar exatamente estes cinco em `provides:`.

## Gotcha mais importante ao escrever um cenário

`Paratrooper` deriva de `Effect`/`AbstractWeapon`, que nasce em modo `INACTIVE` (a mesma proteção
que mantém uma arma pendurada numa estação sem "voar sozinha"). Um paraquedista declarado direto
em `players: {}` (como faz `src/poc/paratrooper-drop`, já que a liberação de verdade a partir de
um `StoresMgr` é tarefa futura) **precisa** do slot `mode: "ACTIVE"`, ou fica parado, sem decidir
nada, sem erro nenhum. Ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para os demais gotchas
(a rede de segurança contra o `CRASH_EVENT` genérico, o slot `requireTerrain`, a restrição sobre
`groundAgl`).

## Se você quiser mais contexto

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — todas as decisões de design, com o porquê de
  cada uma, medidas contra o fonte do MIXR
- [`../C-130/docs/ARCHITECTURE.md`](../C-130/docs/ARCHITECTURE.md) — o mecanismo de liberação
  genérica (hoje contra um placeholder) que este modelo já nasce compatível para substituir
- [`../template/docs/ARCHITECTURE.md`](../template/docs/ARCHITECTURE.md) — a separação em
  camadas que este modelo segue
- [`../../../src/poc/paratrooper-drop/README.md`](../../../src/poc/paratrooper-drop/README.md) —
  a demonstração rodável deste modelo
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
  PRÉVIA" — visão geral de `models/`, o contrato de plugin, e o build orquestrado pelo Makefile
  da raiz
