# `Navstar-3` — um satélite de navegação orbitando a Terra

## O que é isto

Um modelo MIXR (`libNavstar-3.so`, `.so` carregado por `dlopen`, ver
[`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
PRÉVIA") que simula um satélite de navegação da família GPS/NAVSTAR em órbita circular ao redor
da Terra. Primeiro modelo deste repositório na categoria `others` (`models/others/`) — as duas
outras categorias (`players`/`systems`) seguem a mesma convenção de `make new-model`.

O MIXR **não tem mecânica orbital nativa nenhuma** — investigado antes de escrever qualquer
código, ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para os detalhes confirmados no fonte
vendorizado. O corpo é um `( SpaceVehicle )` nativo do framework, **sem nenhum `DynamicsModel`
anexado**: quem calcula a órbita é a camada `domain/` deste modelo (um propagador circular de
dois corpos, mais um teste geométrico de sol/sombra), e o resultado é aplicado ao `Player` a cada
ciclo de decisão via `setGeocPosition(ecef, slaved=true)`.

Elementos orbitais default são valores **representativos de uma órbita GPS/MEO moderna**
(~20.180 km de altitude, ~55° de inclinação, período ~12h) — não é reivindicada precisão
histórica para o Navstar-3 real (SVN-3, GPS Block I, lançado em 1978 e há muito desativado).

A única decisão que a árvore de comportamento toma é **`SUNLIT`/`ECLIPSE`** — se o satélite está
iluminado pelo Sol ou na sombra da Terra, a partir de uma direção de sol fixa (configurável por
slot; o MIXR não tem efeméride solar).

## O que tem em cada arquivo

```
Navstar-3/
├── include/
│   ├── domain/
│   │   ├── CircularOrbit.hpp        # o propagador orbital -- sem MIXR, sem BT.CPP
│   │   └── EclipseGeometry.hpp      # o teste de sombra cilindrica
│   ├── bt/
│   │   ├── NodeContext.hpp          # o que a arvore PRODUZ num tick (Navstar3Decision)
│   │   ├── DecisionContext.hpp      # a interface que mantem os nos livres de MIXR (2 metodos)
│   │   ├── bt_factory.hpp           # a lista de nos deste modelo
│   │   └── nodes/                   # IsEclipsedCondition (leitura pura) + SetSunLabelAction
│   ├── ubf/
│   │   ├── Navstar3State.hpp        # percepcao: confirma o ator, guarda altitude (diagnostico)
│   │   ├── Navstar3BtBehavior.hpp   # decisao: propaga a orbita, calcula sol/sombra, tem os slots
│   │   └── Navstar3Action.hpp       # atuacao: move o Player (setGeocPosition), escreve no xboard
│   └── xnative/
│       ├── Navstar3AgentTC.hpp      # o agente -- decide na FASE 3 do frame de tempo critico
│       └── factory.hpp              # registro das 4 classes acima (sem corpo fisico proprio)
├── src/                              # a implementacao de cada header acima, no mesmo layout
├── configs/navstar3_sun_tree.xml     # a arvore -- DADO do modelo, instalada junto com o .so
├── tools/                            # dump-tree-model + update_bt_models.py (create-bt/update-bt)
├── tests/
│   ├── domain/                       # o propagador + a geometria de sombra, sem MIXR, sem Station
│   ├── tree/test_navstar3_tree.cpp   # a arvore de PRODUCAO contra um contexto falso
│   ├── native/                       # as classes MIXR proprias, SEM Station (bancada)
│   └── check_contract.sh             # forma do .so: 1 simbolo T, deps resolvidas
├── docs/ARCHITECTURE.md              # as decisoes de design, o "porque" de cada uma
├── CHANGELOG.md
├── Makefile                          # build autocontido -- ver abaixo
└── meson.build                       # o unico shared_module(): navstar3_lib
```

## Compilar e testar, sozinho

Este diretório é um projeto de build **independente** — não é preciso abrir o resto do
repositório para compilar só isto.

```bash
# uma vez, na raiz do repositorio:
cd ../../.. && make configure && make sdk

# daqui em diante, so aqui dentro:
cd models/others/Navstar-3
make build            # compila -> ./dist/lib/mixr-plugins/libNavstar-3.so
make test             # domain (orbita + sombra) + tree + native + a forma do .so -- 5 testes
make install-host     # copia o .so + a arvore para ../../../plugins/ -- ver a proxima secao
```

`make help` lista todos os alvos.

## Por que existe um passo separado para "publicar no host"

`make install-host` só copia até `../../../plugins/`, o depósito compartilhado com terceiros —
quem sincroniza dali para `dist/`, onde um cenário de fato procura, é `make install` do projeto
raiz. O fluxo do dia a dia na raiz é `make configure && make models && make install`.

## Nomes de fábrica

`Navstar3State`, `Navstar3BtBehavior`, `Navstar3Action`, `Navstar3AgentTC`. O `.edl` de qualquer
cenário que carregue este plugin precisa declarar exatamente estes quatro em `provides:` —
**não** inclui `SpaceVehicle`, que é nativo do host (`mixr::models::factory` já a despacha).

## Gotcha mais importante ao escrever um cenário

Este `( SpaceVehicle )` **não tem `dynamicsModel:`** — declarar um quebraria a premissa do
modelo (a órbita deixaria de ser a única coisa movendo o player). Também **não** declare
`gamingAreaRange:` finito no `( WorldModel )`: uma órbita MEO facilmente excede qualquer raio de
"área de jogo" militar típico, e `Player::setGeocPosition()` marcaria a posição como inválida.
Ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para os demais gotchas (por que `spd=` no
dump reflete velocidade relativa ao referencial ECEF em rotação, não velocidade orbital
inercial; por que `eciPosition()`/`groundTrack()` têm que ficar separadas).

## Demonstração rodável

[`src/poc/navstar3-orbit`](../../../src/poc/navstar3-orbit) — o satélite sozinho, cenário
hermético (sem `terrain:`, sem `networks:`). Determinismo confirmado com 1, 2 e 4 threads T/C
(`tests/determinism/check_determinism.sh`, 1200 frames, dumps byte-idênticos).

## Se você quiser mais contexto

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — todas as decisões de design, com o porquê de
  cada uma, medidas contra o fonte do MIXR (inclusive o veredito de que a mecânica orbital não
  existe nativamente, e o prior art recuperável do histórico de git)
- [`../players/paratrooper/docs/ARCHITECTURE.md`](../players/paratrooper/docs/ARCHITECTURE.md) —
  o agente de tempo crítico (`AgentTC`) e o padrão de três armadilhas resolvidas, copiado aqui
- [`../players/template/docs/CONTRATO.md`](../players/template/docs/CONTRATO.md) — o contrato
  completo que qualquer modelo, desta categoria ou de qualquer outra, precisa cumprir
- [`../../../src/poc/navstar3-orbit/README.md`](../../../src/poc/navstar3-orbit/README.md) — a
  demonstração rodável deste modelo, com o que foi medido rodando
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
  PRÉVIA" — visão geral de `models/`, o contrato de plugin, e o build orquestrado pelo Makefile
  da raiz
