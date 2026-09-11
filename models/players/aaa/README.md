# `aaa` — uma antiaérea (bateria de superfície-ar) estacionária

## O que é isto

Exercício (ver `TODO.md`, raiz do repositório): "implementar uma antiaérea com domo que dispara o
mesmo míssil (já implementado num A-4) que entra no seu raio de atuação" — verificar como isso é
modelado de forma idiomática no MIXR, não um sistema de armas de produção. Continua o exercício
anterior (`models/players/missile`): reusa o **mesmo** `( GuidedMissile )`, sem nenhuma modificação,
disparado agora por um lançador terrestre em vez de um A-4.

`aaa` é um projeto Meson **independente** (mesmo padrão de `models/players/A-4`/`missile`) — a
aplicação principal (o "host") não sabe nada do fonte deste diretório, só carrega `libaaa.so` em
tempo de execução via `dlopen`. Ver [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é
um plugin, construído numa etapa PRÉVIA", para a visão geral.

## A decisão de arquitetura: BT/UBF completo, não uma classe mínima

Confirmado com o usuário (`AskUserQuestion`), ao contrário de `models/players/missile` (guiagem é
controle contínuo, sem árvore): esta antiaérea usa a pilha completa `domain/` → `bt/` → `ubf/` →
`xnative/`, com uma árvore de comportamento de verdade — mesmo a decisão sendo, na prática, uma
regra única ("alvo hostil no domo + arma disponível → dispara"). A alternativa mais enxuta (uma
classe `xnative` só, decidindo direto em `updateTC()`) teria bastado para a regra em si; a pilha
completa foi escolhida deliberadamente para espelhar a arquitetura do A-4 e demonstrar o padrão de
novo num contexto mais simples. Ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para o detalhe
completo, inclusive por que **não** há nenhuma subclasse de `AgentTC` aqui — `( UbfAgent )` nativo
já basta.

## Estrutura

```
aaa/
├── include/
│   ├── domain/DomePolicy.hpp          # regra pura: alcance dentro de [min,max]?
│   ├── bt/
│   │   ├── NodeContext.hpp            # AaaDecision -- o que a arvore PRODUZ
│   │   ├── DecisionContext.hpp        # interface que mantem os nos livres de MIXR
│   │   ├── bt_factory.hpp
│   │   └── nodes/                     # TargetInDome (condicao), FireMissile + Watch (acoes)
│   ├── ubf/
│   │   ├── AaaState.hpp               # percepcao: radar de aquisicao proprio + StoresMgr
│   │   ├── AaaBehavior.hpp            # decisao: carrega/tica a arvore, tem o slot treeFile
│   │   └── AaaAction.hpp              # atuacao: dispara de verdade + escreve no xboard
│   └── xnative/
│       ├── AaaSite.hpp                # o Player estacionario (subclasse de SamVehicle)
│       └── factory.hpp
├── src/                                # implementacao de cada header acima
├── configs/aaa_tree.xml                # a arvore -- DADO do modelo, instalada junto com o .so
├── tests/
│   ├── domain/test_DomePolicy.cpp
│   ├── tree/test_aaa_tree.cpp          # a arvore de PRODUCAO contra um contexto falso
│   └── check_contract.sh
├── docs/ARCHITECTURE.md
├── CHANGELOG.md
├── Makefile
└── meson.build                         # UM artefato: libaaa.so
```

## Compilar e testar

```bash
# uma vez, na raiz do repositorio:
cd ../../.. && make configure && make sdk

# daqui em diante, so aqui dentro:
cd models/players/aaa
make build            # -> ./dist/lib/mixr-plugins/libaaa.so
make test             # domain (DomePolicy) + tree (aaa_tree.xml) + tree-model-sync + contract
make install-host     # copia o .so para ../../../plugins/ (make install da raiz sincroniza pra dist/)
```

`make help` lista todos os alvos.

## Ler também

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — as camadas, a armadilha `SamVehicle`/`Sam`, e
  por que não há subclasse de `AgentTC`.
- [`../missile/README.md`](../missile/README.md) — o `GuidedMissile` que esta antiaérea dispara,
  sem nenhuma modificação.
- [`../A-4/CHANGELOG.md`](../A-4/CHANGELOG.md) — a extensão de RWR/evasão do lado do A-4 que reage
  ao disparo desta antiaérea.
- [`../../../sandbox/AAA-A4-6DOF/README.md`](../../../sandbox/AAA-A4-6DOF/README.md) — o cenário
  de demonstração, com as medições de disparo/detecção/evasão.
- [`models/template/docs/CONTRATO.md`](../../template/docs/CONTRATO.md) — a lista completa e
  autoritativa do que qualquer modelo precisa fazer para o host carregá-lo e rodar com ele.
