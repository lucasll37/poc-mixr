# `flight` (A-4) — o modelo de produção

Um **modelo**: percepção, decisão (árvore do BehaviorTree.CPP) e atuação de uma aeronave, mais as
classes MIXR próprias que um cenário pode nomear. Compila para um `.so`, carregado por
`dlopen` — nenhum executável linka contra ele em tempo de compilação. Este diretório não sabe (e
não precisa saber) qual cenário vai usá-lo; isso é decidido depois, no `.edl` de quem consome o
plugin.

Assume o ambiente já configurado — Conan, Meson, o SDK do host publicado. `make check-root` confere
isoladamente esse pré-requisito (mensagem verde de OK, ou vermelha com o comando exato que falta).
Se faltar, comece pela raiz do repositório (`README.md`, `INSTALL.md`).

## As quatro camadas

```
domain/     regras puras                     libstdc++ só               42 testes, sem MIXR
   ↑
bt/         nós da árvore de comportamento    BT.CPP + domain/           15 testes, sem MIXR
   ↑
ubf/        percepção → decisão → atuação     MIXR + bt/ + domain/ + xnative/
   ↕
xnative/    classes MIXR próprias             MIXR + ubf/                9 testes, sem Station
```

A dependência sobe de baixo para cima; `bt/` não inclui nada de `ubf/`/`xnative`/MIXR — é o que
permite `test-tree` carregar a árvore de produção contra um contexto falso, sem linkar o MIXR.
`bt/` e `ubf/` são irmãos: cada um adapta uma biblioteca externa diferente (BehaviorTree.CPP e o
UBF do MIXR).

A aeronave (`data/jsbsim/`) e as árvores (`configs/flight_tree*.xml`) são dado **deste modelo**,
calibrado para o A-4 — não do cenário que o carrega.

## Build

`make` sozinho — sem alvo — só lista os alvos (roda `help`); nada compila por acidente. Os alvos do
dia a dia, em ordem:

```bash
make build           # -> ./build/ (compila libflight.so + libflight_tc.so)
make test            # domain + tree + native (~1 s, nenhuma levanta Station)
make install         # -> ./dist/lib/mixr-plugins/*.so + ./dist/share/mixr-plugins/flight/ (arvore + aeronave)
make install-host    # -> ../../../plugins/ -- ainda não é dist/ do HOST, ver CONTRIBUTING.md
```

`libflight.so` e `libflight_tc.so` saem da MESMA árvore de fontes — a única diferença
(`FlightAgentTC`, o agente que decide na fase 3 do frame de tempo crítico em vez de no laço de
background) fica atrás de `-DFLIGHT_TC_AGENT`.

`make help` (ou `make` sozinho) lista todos os alvos, com descrição. Fluxo completo (editar,
testar, editar a árvore com o Groot, publicar o plugin) → [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Testes

Três camadas, nenhuma levanta `Station`:

| suíte | prova | linka MIXR |
|---|---|---|
| `domain` | regras puras (histerese, alvo fixo, piso anti-CFIT, pernas de patrulha...) | não |
| `tree` | a árvore de produção carregada contra um contexto falso; e que os 5 `.xml` continuam sincronizados com o `<TreeNodesModel>` gerado (`tree-model-sync`) | não |
| `native` | coerência de fábrica, slots com tipo e unidade, fronteira de fase do `AlertDatalink` | sim |

O que essa suíte **não** alcança — `RadarScan`, `FlightState::updateState`, `FlightAction::execute`
e os `genAction()` — precisa de um player vivo (`Player::reset()` aborta sem uma `Simulation`).
Coberto pelas suítes `scenario`/`determinism` do host, fora deste diretório.

## Ler também

- [`CONTRIBUTING.md`](CONTRIBUTING.md) — o roteiro de desenvolvimento: editar, testar, editar a
  árvore com o Groot, gerar o `<TreeNodesModel>`, publicar o plugin
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — calibração do A-4, o gerador de
  `<TreeNodesModel>` em detalhe, armadilhas confirmadas
- [`docs/POLITICAS.md`](docs/POLITICAS.md) — decidir com um script Python ou com uma política ONNX
  em vez da árvore nativa
- [`CHANGELOG.md`](CHANGELOG.md) — o que mudou neste modelo, e por quê
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
  PRÉVIA" — visão geral de `models/`, o contrato de plugin, o build em etapas do repositório
  inteiro
