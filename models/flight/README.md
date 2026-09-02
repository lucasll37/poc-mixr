# `flight` — o modelo de produção

A política das duas pocs gêmeas: percepção, decisão (uma árvore do BehaviorTree.CPP arbitrada por
voto), atuação, e as classes MIXR próprias que o cenário nomeia.

**Não é compilado dentro de executável nenhum.** É um projeto Meson independente, construído numa
etapa anterior (`make models`) e carregado com `dlopen` durante o parse do `.epp`. Ver
[../README.md](../README.md) para o build em etapas e para escrever um modelo novo.

## As quatro camadas, e a dependência é de mão única

```
domain/     regras puras          libstdc++ só                    ← 47 testes, sem MIXR
   ↑
bt/         nós da árvore         BehaviorTree.CPP + domain/      ← 20 testes, sem MIXR
   ↑
ubf/        percepção/decisão/    MIXR + bt/ + domain/ + xnative/
            atuação
   ↕
xnative/    classes MIXR          MIXR + ubf/                     ← 9 testes, sem Station
            próprias
```

`bt/` não inclui **nada** de `ubf/`, nada do MIXR e nada de `xnative/` — e o namespace dele
(`bt_nodes`) fica deliberadamente fora de `mixr::`. É isso que permite ao `test-tree` carregar o
`flight_tree.xml` **de produção** contra um contexto falso, em ~10 ms, sem linkar uma lib do MIXR.

`bt/` e `ubf/` são **irmãos**, não pai e filho: cada um adapta uma biblioteca externa diferente —
`bt/` a BehaviorTree.CPP, `ubf/` o framework UBF do MIXR.

## Uma árvore, dois artefatos

| artefato | quem usa | diferença |
|---|---|---|
| `libflight.so` | `single-thread` | — |
| `libflight_tc.so` | `multi-thread` | `-DFLIGHT_TC_AGENT` liga o `FlightAgentTC` |

Era a única diferença entre as duas cópias que existiam antes — ~3.100 linhas duplicadas
sustentadas por um teste de guarda. Agora é um `#ifdef`.

## Build autocontido

Este diretório também tem um `Makefile` próprio, independente do Makefile da raiz do `poc-mixr` —
útil para abrir o VS Code só aqui e iterar sem levantar o host inteiro:

```bash
# uma vez, na raiz do poc-mixr:
cd ../.. && make configure && make sdk

# daqui em diante, so aqui dentro:
cd models/flight
make                 # -> ./dist/lib/mixr-plugins/{libflight,libflight_tc}.so + ./dist/share/...
make test            # domain + tree + native, neste build isolado
make install-host    # sincroniza ./dist para a raiz do poc-mixr (a UNICA escrita fora daqui)
```

`make help` lista os alvos. O fluxo de produção/CI continua sendo o Makefile da raiz (`make
models`, que constrói **os três** modelos de uma vez) — este é só para iterar neste modelo
sozinho. Ver [models/fixtures/stub/README.md](../fixtures/stub/README.md) para o "porquê" de
`install-host` ser o único alvo que escreve fora de `./dist`.

## Testes

```bash
make test-models        # domain (47) + tree (20) + native (9) = 76 casos, ~1 s -- via o Makefile da raiz
make test                                                                       # via este Makefile
```

Nenhuma das três levanta `Station`. A camada `native` linka o MIXR (as outras duas não) e testa a
coerência da fábrica, as tabelas de slot com **tipo e unidade**, e a fronteira de fase do
`AlertDatalink`.

**O que essa camada NÃO alcança**, e a fronteira é limpa: `RadarScan`, `FlightState::updateState`,
`FlightAction::execute` e os `genAction` precisam de um player vivo — `Player::reset()` aborta sem
uma `Simulation`. Esses ficam cobertos pelas camadas `scenario`/`determinism` do host e, no caso da
varredura, pela checagem do `.acmi` no teste do stub.

## A dissecação profunda

O [README da single-thread](../../src/single-thread/README.md) descreve este modelo peça por peça —
seções 7 a 10. Os arquivos moraram para cá; o texto continua valendo.

## Ler também

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — calibração do c310, armadilhas confirmadas
  específicas deste modelo
- [../README.md](../README.md) — visão geral de `models/`, o contrato de plugin, e o build
  orquestrado pelo Makefile da raiz
