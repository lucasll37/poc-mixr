# `C-130` — aeronave de transporte, navegação nativa + liberação de paraquedista

## O que é isto

Modelo de produção (plugin `libC-130.so`, `dlopen`'d em tempo de execução pelo host) para uma
aeronave C-130 Hercules (dados JSBSim vendorizados em `shared/data/jsbsim/aircraft/C130/`, curados
para este modelo em `data/jsbsim/`). Duas capacidades:

1. **Navega de verdade** por `Route`/`Steerpoint` nativo do MIXR — mesmo padrão já provado em
   `src/poc/full-systems-nav/`: `Autopilot` nativo (`headingHoldMode`/`altitudeHoldMode`/
   `velocityHoldMode`, `navMode: false`) comandado por uma árvore de comportamento de **um nó só**
   (`Navigate`, sem `Fallback`) que relaia a guiagem que `Route::autoSequencer()`/
   `Steerpoint::compute()` já calculam a cada frame, independente do agente. Sem combate, sem
   evasão — este modelo só navega.
2. **Libera um paraquedista** a partir de um `Steerpoint`, tratado como **liberação de ARMA**
   (mesma família nativa de `Bomb`/`Decoy`/`Chaff`), não como um `Player` autônomo com UBF próprio
   — um paraquedista não decide nada, só cai. `ActionParatrooperRelease` (própria deste modelo,
   disparada pelo mesmo caminho nativo de `ActionWeaponRelease`/`ActionDecoyRelease`) acha, por
   `Player::getType()`, a próxima estação livre do `StoresMgr` cujo tipo bata com `storeType:`
   (default `"PARATROOPER"`) e libera — genérico contra `mixr::models::AbstractWeapon`, nunca
   contra uma classe concreta. O modelo `models/players/paratrooper` (tarefa futura, ainda um
   scaffold vazio) vai substituir só o EDL — a classe concreta declarada na estação + `provides:` —
   sem precisar de nenhuma mudança de C++ aqui. Até lá, `ParatrooperPlaceholder` (um `Effect`
   nativo trivial) faz o papel.

Ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para a arquitetura completa, e
[`src/poc/c130-airdrop/README.md`](../../../src/poc/c130-airdrop/README.md) para o cenário de
demonstração (a poc que exercita este modelo).

## Estrutura

```
models/players/C-130/
├── include/
│   ├── domain/{FlightCommand,WorldView,geometry}.hpp   # DTOs puros -- sem MIXR, sem BT.CPP
│   ├── bt/
│   │   ├── NodeContext.hpp        # FlightDecision -- o que a arvore PRODUZ num tick
│   │   ├── DecisionContext.hpp    # interface minima (3 metodos) que mantem os nos livres de MIXR
│   │   ├── bt_factory.hpp         # o registro de nos deste modelo (UM so -- Navigate)
│   │   └── nodes/NavigateAction.hpp
│   ├── ubf/
│   │   ├── FlightState.hpp        # percepcao: le o AirVehicle, monta o WorldView
│   │   ├── BtBehavior.hpp         # decisao: carrega a arvore de UM no e tica
│   │   └── FlightAction.hpp       # atuacao: comanda o Autopilot nativo + escreve no xboard
│   └── xnative/
│       ├── FlightAgentTC.hpp             # o agente de tempo critico (fase 3 do frame)
│       ├── ActionParatrooperRelease.hpp  # a liberacao generica (a novidade deste modelo)
│       ├── ParatrooperPlaceholder.hpp    # entidade provisoria ate paratrooper existir
│       └── factory.hpp                   # registro das 6 classes (nomes "C130*")
├── src/                            # a implementacao de cada header acima, mesmo layout
├── data/jsbsim/
│   ├── aircraft/C130/{C130.xml,c130ap.xml}   # curado do vendorizado, autopilot proprio
│   └── engine/{t56,t56_prop}.xml
├── configs/c130_nav_tree.xml       # a arvore -- DADO do modelo, instalada junto com o .so
├── tests/{domain,tree,native}/     # ver "Testar" abaixo
├── docs/ARCHITECTURE.md
├── CHANGELOG.md
├── Makefile                        # build autocontido
└── meson.build                     # UM artefato: libC-130.so
```

## Compilar e testar

Projeto Meson **independente** — não precisa do resto do repositório aberto. Único pré-requisito:
o SDK de plugin publicado uma vez pela raiz.

```bash
# uma vez, na raiz do repositorio:
cd ../../.. && make configure && make sdk

# daqui em diante, so aqui dentro:
cd models/players/C-130
make build            # -> ./dist/lib/mixr-plugins/libC-130.so
make test             # domain + tree + native + forma do .so
make install-host     # copia pra ../../../plugins/ (deposito compartilhado)
```

`make help` lista todos os alvos. Depois de `install-host`, `cd ../../.. && make install` publica
em `dist/`, onde um cenário de fato procura.

## Nomes de fábrica com prefixo `C130`

As seis classes deste modelo se registram como `"C130FlightAgentTC"`, `"C130ActionParatrooperRelease"`,
`"C130ParatrooperPlaceholder"`, `"C130FlightState"`, `"C130BtBehavior"`, `"C130FlightAction"` — **não**
`"FlightAgentTC"`/`"FlightState"`/... como `models/players/A-4` usa. `tests/guard/check_colisao_fabrica.py`
compara nome de fábrica par a par entre **todos** os modelos sob `models/`, independente de alguma
vez serem carregados juntos no mesmo `.edl` — reaproveitar os nomes da A-4 derrubaria essa guarda
na hora. O EDL de qualquer cenário que carregue este modelo precisa usar os nomes `C130*` — ver
`src/poc/c130-airdrop/configs/scenario_c130_airdrop.edl.in`.

## Se você quiser mais contexto

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — as camadas, o piloto automático JSBSim próprio,
  a liberação de paraquedista, e o que ainda falta calibrar
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
  PRÉVIA" — visão geral de `models/`, o contrato de plugin, e o fluxo de build orquestrado
- [`../template/docs/CONTRATO.md`](../template/docs/CONTRATO.md) — a lista completa e autoritativa
  do que qualquer modelo precisa fazer para o host carregá-lo e rodar com ele
