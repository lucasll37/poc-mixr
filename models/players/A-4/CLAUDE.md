# CLAUDE.md — models/players/A-4

Complementa o `CLAUDE.md` da raiz (sempre carregado) e os `.md` deste projeto (`README.md`,
`CHANGELOG.md`, `docs/ARCHITECTURE.md`, `docs/POLITICAS.md`). Só entra aqui o que não está em
nenhum dos dois — não duplique o que já está lá.

## Nomes obsoletos em comentários (a classe/arquivo não existe mais com esse nome)

Comentários de produção citam nomes renomeados e nunca atualizados — procurar por eles no código
é perda de tempo:

- `AlertRadio` / `FlightDirector` → as classes reais são `xnative::AlertDatalink` e
  `mixr::models::Autopilot` (`include/ubf/FlightAction.hpp:23-24,44`).
- `xnative/BehaviorBoard.hpp` → o arquivo real é `libs/xboard/Board.hpp`
  (`src/ubf/FlightAction.cpp:127`).

## Gotcha de unidade isolado: `Autopilot::setCommandedAltitudeFt()` é a ÚNICA chamada em pés

Todo o resto do modelo (`domain::FlightCommand`, `ubf::BtTuning`, `domain::WorldView`) trabalha em
METROS. A conversão pra pés acontece só na fronteira de atuação, `FlightAction::execute()`
(`src/ubf/FlightAction.cpp:161`): `command.altitudeM * base::distance::M2FT`. Um caminho de
atuação novo que esqueça essa conversão manda altitude ~3,3x maior do que deveria pro `Autopilot`.

## `AlertDatalink::sendMessage()` não filtra por alcance nem por lado

O slot `maxRange` existe e é gravado, mas `sendMessage()` nunca o lê — sem `radioName:` no EDL, a
entrega é BROADCAST GLOBAL (alcança até um `bandit1` vermelho, se ele tiver `datalink:`). E o
único gancho que de fato recebe a entrega LOCAL é `onDatalinkMessageEvent()` — nem
`receiveMessage()` nem sobrescrever `queueIncomingMessage()` capturam nada (medido: 0 alertas via
`receiveMessage()` em 90s com 1113 transmissões, contra 2016 chamadas de
`onDatalinkMessageEvent()`). Já documentado em detalhe no cabeçalho de
`include/xnative/AlertDatalink.hpp:15-67` — mas só lá, nenhum `.md` menciona.

## `bt/nodes/NavigateAction.cpp` limita o rumo a 3 deg/s, mais apertado que o Autopilot (6 deg/s)

Deliberado — sem esse limitador, medido rodando: a aeronave entra em espiral e colide com o
terreno em ~200s (perseguição pura divergindo). Só usado por `flight_tree_nav.xml`/
`full-systems-nav`. (`src/bt/nodes/NavigateAction.cpp:12-58`)

## Testar `FlightState`/`FlightAction`/`AltitudeSafetyBehavior` sem `Station` exige `WorldModel` + `reset()`

`Player::setPosition()`/`setAltitude()` chamam `getWorldModel()->getMaxRefRange()`/
`getEarthModel()` sem checar nulo — sem um `WorldModel` como container, segfault. E sem chamar
`air->reset()`, `useCoordSys` fica em `CS_NONE` e `setAltitude()` é um no-op silencioso. Ver o
padrão de bancada (`struct Bench`) em `tests/native/test_flight_state_action.cpp:12-20,49-77` —
qualquer teste novo na suíte `native` que manipule posição/altitude precisa replicar isso.

## `OnnxScoreCondition` trunca em 16 valores de saída, sem aviso

Buffer fixo `std::array<float, 16>` (`src/bt/nodes/OnnxScoreCondition.cpp:72`) — um `.onnx` de
score com mais de 16 saídas perde as extras em silêncio.

## `PyDecideAction` e `OnnxPolicyAction` têm contratos de saída DIFERENTES

`PyDecideAction` espera do `decide()` unidades físicas diretas (`heading_deg, altitude_m,
speed_kts`) — `include/bt/nodes/PyDecideAction.hpp:20-23`. `OnnxPolicyAction`, por padrão
(`normalized: true`), espera saída em `[-1,1]` e desnormaliza via `xrlbridge::unscaleCommand()` —
`include/bt/nodes/OnnxPolicyAction.hpp:24-29`. Quem copia um contrato pro outro precisa lembrar do
descalonamento; `docs/POLITICAS.md` documenta os dois separados, nunca lado a lado.
