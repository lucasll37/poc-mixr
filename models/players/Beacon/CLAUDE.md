# CLAUDE.md — models/players/Beacon

Complementa o `CLAUDE.md` da raiz e os `.md` deste projeto (`README.md`, `CHANGELOG.md`,
`docs/ARCHITECTURE.md`). Só entra aqui o que não está em nenhum dos dois.

## `BEGIN_EVENT_HANDLER` exige uma DECLARAÇÃO explícita de `event()` no header

Ao contrário de `DECLARE_SUBCLASS`, que já gera `clone()`/`operator=`/`copyData()`/`deleteData()`
automaticamente, ele **não** declara `bool event(const int, base::Object* const = nullptr)
override` — isso só aparece se a classe realmente sobrescrever `event()`. Escrever
`BEGIN_EVENT_HANDLER(Beacon)` no `.cpp` sem a declaração correspondente no `.hpp` falha em tempo de
COMPILAÇÃO (`"no declaration matches ... event(int, mixr::base::Object*)"`), não em runtime —
achado imediatamente ao compilar, não uma armadilha silenciosa, mas fácil de esquecer porque a
maioria das outras macros deste framework (`DECLARE_SUBCLASS`, `BEGIN_SLOTTABLE`) não pedem
declaração equivalente no header. `mixr::models::player::Player` já declara a própria (`public:`,
`Player.hpp`) — a assinatura na subclasse tem que bater exatamente (mesmos parâmetros/defaults).

## Testar contra um `WorldModel` de bancada, SEM Station: `reset()` antes de `addNewPlayer()`

`Simulation::getPlayers()` é populada por uma fila (`newPlayerQueue`, drenada por
`updatePlayerList()`), mas `Simulation::updatePlayerList()` só troca a lista quando há algo a
trocar — e o código dela assume que a lista ATUAL (`players`, privada) já é uma `PairStream`
válida (mesmo vazia), nunca `nullptr`. Sem chamar `world->reset()` primeiro (que sempre cria uma
`PairStream` nova, mesmo sem nenhum `origPlayers`), o primeiro `addNewPlayer()` seguido de
`updateData()` desreferenciaria um `safe_ptr` nulo. Sequência que funciona, confirmada rodando
(`tests/native/test_beacon.cpp`):

```cpp
world->reset();                 // inicializa 'players' para uma PairStream vazia
world->addNewPlayer("a", a);    // enfileira -- so' entra na lista no PROXIMO updateData()
world->addNewPlayer("b", b);
world->updateData(dt);          // drena a fila (container()+nome ja ajustados) E chama
                                 // player->updateData(dt) em cada um, na MESMA chamada
```

`Simulation::updateData()` chama `updatePlayerList()` **antes** do laço que invoca
`player->updateData(dt)` em cada player da lista — por isso um player recém-adicionado já participa
do PRÓPRIO tick em que foi inserido, sem precisar de um tick "de aquecimento" à parte.

## `Beacon::updateData()` nunca precisa de mutex — ao contrário de `AlertDatalink`

`Simulation::updateBgPlayerList()` (a fase de fundo, 10 Hz) é sequencial, sempre uma thread só,
mesmo com `numTcThreads` > 1 (esses threads são só para a fase 3, tempo crítico). Confirmado lendo
o fonte antes de escrever qualquer coisa: `contexts/src/mixr/src/simulation/Simulation.cpp`,
`updateData()`. `xnative::AlertDatalink::broadcastAlert()` (`models/players/A-4`) precisa de
`std::mutex` porque é chamada de dentro da decisão UBF (fase 3, um player por thread do pool) — o
mesmo mecanismo de evento, dois contextos de concorrência diferentes. Ver `docs/ARCHITECTURE.md`.

## `Simulation::getStationImp(): ERROR, unable to locate the Station class!` é esperado nesta suíte

Aparece em `stderr` durante `tests/native/test_beacon.cpp` (via `world->reset()`, que tenta achar
uma `Station` container para configurar prioridade de thread) — inofensivo: a bancada não tem
`Station` nenhuma de propósito (é o que mantém o teste rápido e sem MIXR de verdade rodando uma
simulação completa). Não indica teste quebrado; os 5 casos passam.
