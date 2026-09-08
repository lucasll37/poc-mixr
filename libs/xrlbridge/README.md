# `libs/xrlbridge` — a ponte de comando/observação entre RL e o modelo

Uma troca síncrona de dois structs, `Command`/`Observation`, entre um host de RL em Python
(`src/rl/bindings/`, pybind11) e o comportamento UBF que decide por fora do processo MIXR
(`RLBridgeBehavior`, em `models/players/A-4`). Mais o contrato de dados que dá ordem aos 28 floats
que viram entrada de rede — `ObservationFields.hpp` — reusado por três consumidores diferentes.

## Como se usar

No `.edl`, `RLBridgeBehavior` entra no lugar de `( BtBehavior )` como a folha de decisão do
agente — hoje isso só acontece em `src/rl/configs/scenario_rl.edl` (as pocs de produção não usam
`( UbfArbiter )` — ver "SEM ARBITRO" no topo de `scenario.edl.in` — então o exemplo abaixo é
especificamente de como o ambiente Gymnasium monta o próprio cenário, não como a produção decide):

```
agent: ( FlightAgentTC
   state: ( FlightState )
   behavior: ( UbfArbiter
      behaviors: {
         ( AltitudeSafetyBehavior vote: 90 ... )   // o piso nativo continua acima
         // Decisao vem de fora (Python), via libs/xrlbridge --
         // ver models/players/A-4/include/ubf/RLBridgeBehavior.hpp.
         ( RLBridgeBehavior vote: 50 )
      }
   )
)
```

Do lado do **modelo** (`models/players/A-4/src/ubf/RLBridgeBehavior.cpp`), `genAction()` não decide
nada — só publica o `WorldView` deste frame e devolve o `Command` que o host deixou pendente:

```cpp
#include "xrlbridge/RLBridge.hpp"

base::ubf::AbstractAction* RLBridgeBehavior::genAction(
   const base::ubf::AbstractState* const state, const double)
{
   const auto flightState = dynamic_cast<const FlightState*>(state);
   if (flightState == nullptr) return nullptr;

   const FlightState::Snapshot& snap{flightState->snapshot()};

   xrlbridge::setObservation(toObservation(snap));   // campo a campo, WorldView -> Observation
   if (!snap.valid) return nullptr;

   const domain::FlightCommand cmd{toFlightCommand(xrlbridge::getPendingCommand())};
   const auto action = new FlightAction();
   action->setCommand(cmd);
   action->setLabel("RL");
   action->setVote(getVote());
   return action;
}
```

Do lado do **host** (`src/rl/bindings/NativeSimulation.cpp`), `step()` faz o inverso — publica o
comando, avança um frame, lê a observação que acabou de ser cacheada:

```cpp
std::pair<mixr::xrlbridge::Observation, bool> NativeSimulation::step(
   const mixr::xrlbridge::Command& cmd)
{
   mixr::xrlbridge::setPendingCommand(cmd);

   const double dt{1.0 / static_cast<double>(station_->getTimeCriticalRate())};
   station_->tcFrame(dt);      // fase 3: FlightAgentTC -> RLBridgeBehavior::genAction()
   station_->updateData(dt);

   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station_->getSimulation());
   bool terminated{};
   if (worldModel != nullptr) {
      const auto player = worldModel->findPlayerByName(playerName_.c_str());
      terminated = (player != nullptr) && player->isCrashed();
   }

   return {mixr::xrlbridge::getObservation(), terminated};
}
```

`src/rl/bindings/PyBindings.cpp` traduz `Observation` para um `py::dict` solto (`toDict()`), campo
a campo pela mesma X-macro — nenhuma lista de campos é reescrita ali. `observationFieldNames()`/
`observationBoolFields()` são expostas ao Python (`m.def("observation_field_names", ...)`) para que
`mixr_gym/env.py` monte o `observation_space` a partir delas, em vez de repetir a lista.

## Por que `Command`/`Observation` não reusam `domain::FlightCommand`/`domain::WorldView`

Os campos de `Observation` espelham `domain::WorldView` **campo a campo**, mas deliberadamente não
reusam o tipo: esta lib não pode incluir headers do modelo (`tests/guard/check_host_opaco.sh` — o
host não pode conhecer o fonte do modelo), então define sua própria cópia da forma. A conversão
`WorldView` → `Observation` é feita uma única vez, do lado do modelo
(`RLBridgeBehavior.cpp::toObservation()`); o host nunca vê `WorldView`, só `Observation`.

## Por que `shared_library()`, e não estática

Mesmo motivo estrutural de `libs/xboard::Board` (ver o cabeçalho de `RLBridge.hpp`): quem
**escreve** o comando e **lê** a observação é o host (executável); quem **lê** o comando e
**escreve** a observação é o modelo (um `.so` aberto com `dlopen`). Uma lib estática daria a cada
lado a sua própria cópia dos dois globais (`g_command`/`g_observation` em `RLBridge.cpp`) — o
host nunca veria o comando chegar no modelo, e vice-versa. Instalada pelo mesmo motivo de `xboard`:
o consumidor em `dist/bin/`/`dist/python/` precisa achá-la em `dist/lib/`.

Concorrência é o mesmo padrão de `Board.hpp`: um mutex só, protegendo um mapa minúsculo — aqui, os
dois structs globais, sem chave por player id. **V1 é um único agente RL por processo**:
`genAction()` não tem como descobrir o ID do player que o hospeda sem subir a árvore de
componentes por `container()`, caminho já documentado como frágil neste framework para objetos
aninhados em slot (a mesma armadilha de `TacviewOutput::resolveInfo()`). Generalizar para vários
agentes trocaria `setPendingCommand`/`getObservation` por um mapa por `playerId` — não feito
porque nenhum cenário precisa disso ainda.

## Por que `RLBridgeBehavior` mora DENTRO de `models/players/A-4`, e não num plugin próprio

Um plugin separado (no molde do que o extinto modelo `missile` fazia, para não obrigar as pocs de
produção a atualizar `provides:`) não serviria aqui: `RLBridgeBehavior::genAction()` precisa de
`dynamic_cast<const xnative::FlightState*>` e construir um `xnative::FlightAction*` — tipos
**concretos** do modelo, não só o nome de fábrica. Como cada plugin compila com
`gnu_symbol_visibility: 'hidden'`, um `dynamic_cast` cruzando dois `.so` distintos para um tipo com
visibilidade oculta é frágil. Ficar no mesmo `.so` elimina esse risco; o preço é mecânico:
`RLBridgeBehavior` é mais um nome que `libflight.so` exporta, e como `provides:` é
igualdade exata de conjunto contra o que a `.so` exporta, todo cenário que carrega esse plugin
precisou de uma linha a mais (inclusive o mirror de contrato de `models/players/template`, que
precisa continuar contrato-compatível com o cenário de produção mesmo sem instanciar a classe).

## Latência de um frame

`step()` publica o `Command` **antes** de `station_->tcFrame(dt)`, mas a fase 3 desse mesmo frame
já leu o `WorldView` (via `FlightState::updateState()`, chamado antes de `genAction()`) formado
pela dinâmica que rodou na fase 0 — ou seja, pelo comando aplicado no frame **anterior**. É
comportamento padrão de qualquer malha de controle fechada, não um bug.

## `ObservationFields.hpp` — a ordem canônica, num lugar só

A forma da observação era mantida à mão em cinco lugares (`domain::WorldView`,
`xrlbridge::Observation`, a conversão campo a campo de `RLBridgeBehavior`, o `toDict()` dos
bindings e as listas de `env.py`). Enquanto a política era um processo Python do outro lado de uma
caixa de correio, divergir dava `KeyError` — alto e na hora. Com um `.onnx` (`OnnxPolicy`,
`libs/xinfer`), divergir não dá erro nenhum: o modelo recebe 28 floats na ordem errada e voa
errado, em silêncio. Por isso a ordem virou uma X-macro (`XRLBRIDGE_OBSERVATION_FIELDS`), expandida
contra `domain::WorldView` no modelo e contra `xrlbridge::Observation` aqui — **um nome de campo
que divergir entre as duas structs não compila**. `tools/train_policy.py`-style exporters e
`env.py` derivam a lista de `observationFieldNames()`/`observationBoolFields()` em vez de repeti-la.

A ordem é 23 floats numéricos, depois os 5 booleanos (`valid`, `terrainValid`, `hasContact`,
`hasAlert`, `weaponReady`) — a ordem de `env.py`, não a de declaração de `WorldView` (que intercala
os dois). **Mudar essa ordem invalida todo `.onnx` já treinado**: é quebra de contrato, não
refactor. Os três campos de texto (`contactName`, `alertSender`, `alertContactName`) ficam de fora
da macro de propósito — não são número, não entram em tensor — mas continuam em `Observation` para
log/depuração (`toDict()` os inclui à parte, em `info["raw_state"]`).

`unscaleCommand()` faz o caminho inverso para a ação: `[-1,1]` normalizado (o que um `.onnx`
exportado do SB3 emite, com `Tanh` final) → unidades físicas (`headingDeg` em `[0,360]`,
`altitudeM` em `[0,8000]`, `speedKts` em `[0,400]`, `XRLBRIDGE_ACTION_FIELDS`). O recorte para
`[-1,1]` acontece **antes** da desnormalização — uma rede fora de escala não pode virar um comando
fisicamente absurdo.

## Armadilhas confirmadas

1. **Sem chave por player id** — v1 é um único agente RL por processo (ver acima). Rodar mais de
   um `MixrFlightEnv` no mesmo processo não generaliza isso: precisa de um processo por ambiente
   (ver `src/rl/README.md`, "só pode existir uma `Station` por processo").
2. **`genAction()` publica a `Observation` mesmo quando `snap.valid == false`** (só não devolve uma
   `FlightAction` nesse caso) — para `getObservation()` nunca ficar presa num valor velho quando o
   ator momentaneamente não é um `AirVehicle` válido.
3. **`packObservation`/`unscaleCommand` degradam com ponteiro nulo, nunca abortam** — a mesma
   política de `libs/xinfer`/`libs/xjoystick` quando a dependência degrada: devolvem estrutura
   zerada em vez de derrubar o processo.

## Testes

`tests/domain/test_xrlbridge.cpp` — a camada mais isolada possível: sem `Station`, sem plugin, sem
pybind11, só a lib. Cobre a contagem/ordem/duplicata dos 28 campos, `packObservation()` campo a
campo (não só a contagem — um campo fora de ordem aqui não quebra compilação, só faz o `.onnx`
voar errado em silêncio), `unscaleCommand()` nos extremos/centro/saturação, e o round-trip de
`Command`/`Observation` sob escrita concorrente real (`std::thread` escrevendo enquanto a thread de
teste lê, verificando que nenhum struct parcialmente escrito escapa do mutex).
