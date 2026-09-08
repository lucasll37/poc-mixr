# `libs/xtacview` — exportação para o Tacview

Um `mixr::recorder::OutputHandler` de verdade que traduz cada `DataRecord` nativo em linhas do
protocolo ACMI/Real-Time Telemetry do Tacview — socket **e** arquivo `.acmi`, ao mesmo tempo.

## Como se usar

Declarado dentro da cadeia nativa do slot `dataRecorder:` da `Station`, como mais um
`OutputHandler` de `RecorderOutputHandler::components:`. Exemplo real, de
`src/poc/dis/flight/configs/scenario.edl.in`:

```
   dataRecorder: ( ExposedDataRecorder
      eventName: "flight"

      // 43 = REID_PLAYER_DATA   42 = REID_PLAYER_REMOVED
      enabledList: [ 43 42 ]
      outputHandler: ( RecorderOutputHandler
         components: {
            ( TacviewOutput
               port: 1234
               callsign: "poc-mixr/flight"
               fileName: "./src/poc/dis/flight/data/recordings/mission.acmi"

               modelMap: { falcon1: "A-4E"  falcon2: "A-4E"  falcon3: "A-4E"
                           falcon4: "A-4E"  bandit1: "A-4E" }
               typeMap:  { falcon1: "Air+FixedWing"  falcon2: "Air+FixedWing"
                           falcon3: "Air+FixedWing"  falcon4: "Air+FixedWing"
                           bandit1: "Air+FixedWing" }
               colorMap: { falcon1: "Blue"  falcon2: "Blue"  falcon3: "Blue"
                           falcon4: "Blue"  bandit1: "Red" }
            )
         }
      )
   )
```

`( ExposedDataRecorder )` no lugar de `( DataRecorder )` é o que deixa o host achar o
`TacviewOutput` de fora da cadeia do recorder — ver a seção própria abaixo. E **todo player que
deve aparecer no Tacview precisa de `dataLogTime:` declarado** (`falconN: ( Aircraft ...
dataLogTime: ( Seconds 0.1 ) ... )`); sem isso ele nunca emite `REID_PLAYER_DATA` e simplesmente
não aparece — ver a primeira armadilha abaixo.

Do lado do host, o `main.cpp`/`app/StationBuilder.cpp` acham o objeto assim (o slot
`outputHandler` não é alcançável por `container()` — por isso a busca desce pela árvore, nunca
sobe):

```cpp
#include "xtacview/TacviewOutput.hpp"

mixr::xtacview::TacviewOutput* tacviewOutputOf(mixr::simulation::Station* const station)
{
   const auto dataRecorder =
      dynamic_cast<mixr::xtacview::ExposedDataRecorder*>(station->getDataRecorder());
   mixr::recorder::OutputHandler* const outputHandler{
      dataRecorder != nullptr ? dataRecorder->getOutputHandler() : nullptr};
   mixr::base::Pair* const pair{
      outputHandler != nullptr ? outputHandler->findByType(typeid(mixr::xtacview::TacviewOutput)) : nullptr};
   return pair != nullptr ? dynamic_cast<mixr::xtacview::TacviewOutput*>(pair->object()) : nullptr;
}
```

Com o ponteiro em mãos, o laço de tempo real chama duas APIs que **não** passam pelo pipeline do
recorder (ver "Por que existem `publishIdentities`/`updateRadarScan`" abaixo) — nesta ordem, e
**antes** de `station->updateData(dt)`:

```cpp
if (tacviewOutput != nullptr) tacviewOutput->publishIdentities(worldModel);
// ...
for (auto* const player : discoverPlayers(worldModel)) {
   const mixr::xboard::Readout board{mixr::xboard::get(player->getID())};
   if (board.radarValid) {
      tacviewOutput->updateRadarScan(static_cast<std::uint32_t>(player->getID()), simTime,
         board.radarAzDeg, board.radarElDeg, board.radarRangeM,
         board.radarHBeamDeg, board.radarVBeamDeg);
   }
}
```

## Por que é `OutputHandler`, não um laço varrendo players

Em vez de o `main.cpp` montar as linhas ACMI na mão a cada tick, o framework **empurra** os dados:
cada registro `REID_*` que já existe no `DataRecorder` nativo vira uma linha do stream, de graça —
`REID_PLAYER_DATA` → posição/atitude, `REID_NEW_PLAYER`/`REID_PLAYER_REMOVED` → declaração/remoção,
`REID_MARKER` → evento de texto. É por isso que o cenário só precisa de um bloco EDL — nenhuma poc
deste repositório monta stream ACMI na mão.

## Por que existem `publishIdentities()`/`updateRadarScan()` — dois caminhos por fora do pipeline

O schema `DataRecord.proto` não tem campo para "para onde a antena está apontando agora"
(`PlayerState`/`TrackData`/`EmissionData` não cobrem isso), e `REID_PLAYER_DATA` — o único token
habilitado por `enabledList: [ 43 42 ]` (ver a armadilha do `REID_NEW_TRACK` abaixo) — traz um
`PlayerId` **parcial**: só `id` e `name`, sem `ac_type`/`major_type`/`side`. `resolveInfo()`
tentaria subir até a `Station` via `findContainerByType()` para achar o `Player` de verdade, mas
essa cadeia **não chega lá** — o `DataRecorder` nativo não chama `container()` no objeto do seu
próprio slot `outputHandler`. Sobra casar por nome nos mapas do EDL, o que não cobre objetos
criados em runtime (um míssil liberado ganha nome automático `"W%05d"`).

`publishIdentities()` fecha essa lacuna pelo caminho que o host já tem: varre
`Simulation::getPlayers()` e grava a identidade real de cada um (nome, `type:`, `getMajorType()`,
`getSide()`, e a **classe C++** do `Player`, que é o que distingue chaff de míssil — os dois são
`majorType == WEAPON`) no cache que `emitState()` consulta depois. Como Name/Type/Color só são
emitidos na **primeira** aparição de cada objeto no stream, chamar isto **antes** de
`station->updateData(dt)` — que é quem drena a fila e declara — é o que garante que a identidade
já esteja no lugar na única vez em que ela é escrita.

`updateRadarScan()` existe pelo mesmo motivo: o pipeline REID não tem schema para varredura de
radar, então quem já tem o `AirVehicle` nativo em mãos (o laço de tempo real do host) empurra o
dado direto — só emite se o objeto já recebeu um `T=` no stream (rastreado em `declared`), senão o
Tacview receberia `RadarAzimuth=` para um id desconhecido.

## Armadilhas confirmadas

1. **`dataLogTime` nasce zero.** É slot do `Player`, não deste handler — sem
   `dataLogTime: ( Seconds 0.1 )` a aeronave nunca emite `REID_PLAYER_DATA` e some do Tacview em
   silêncio. Parece bug deste handler; não é.
2. **`PlayerState.pos`/`.angles` são ECEF/geocêntricos, não geodésicos.** `emitState()` converte
   com `base::nav::convertEcef2Geod()` (posição) e `convertEcefAngles2GeodAngles()` (atitude) — sem
   a segunda conversão, uma aeronave nivelada em lat 37/lon -116 aparecia com `roll=-180`,
   `pitch=-53`: os ângulos mediam a posição no globo, não a atitude.
3. **`REID_NEW_TRACK` (81) nunca chega** — o `DataRecorder` nativo desta versão não tem handler
   para o token e degrada para `REID_UNHANDLED_ID_TOKEN`, apesar do `TrackManager` sinalizar a
   pista nova. E, na prática, nem `REID_TRACK_DATA` (83) chega: todo cenário deste repositório
   declara `enabledList: [ 43 42 ]`, e `isDataEnabled()` descarta qualquer id fora dessa lista
   antes de chegar aqui — os `case` de pista em `processRecordImp()` ficam como código morto,
   prontos para o dia em que o `enabledList` mudar.
4. **`REID_WEAPON_RELEASED` (61) aborta o `DataRecorder` nativo** (bug do framework, não desta
   lib) — é a razão de `enabledList` ficar restrito a `[ 43 42 ]` em vez de incluir os tokens de
   arma/pista que fariam mais sentido "por completude".
5. **`PlayerId` traz identidade parcial — quem resolve de verdade é o nome, não `ac_type`.** Sem
   `publishIdentities()` chamado (ou num cenário onde `main.cpp` não o chame), `typeMap`/
   `colorMap`/`modelMap` só casam por nome do player, nunca por `type:`, e um objeto criado em
   runtime (chaff, flare, míssil) cai sempre no default por `majorType`/`side`.
6. **"Grey" não é cor ACMI válida.** O formato aceita só Red/Orange/Yellow/Green/Cyan/Blue/Violet;
   `defaultColorForSide()` mapeia os seis `Player::Side` para essas cores reais e usa `Violet` como
   default (nunca uma string que o Tacview descarta em silêncio).
7. **Falha de rede não pode derrubar a gravação em arquivo, nem o contrário.** `initIfNeeded()`
   tenta socket e arquivo **separadamente** — uma porta ocupada por outra poc não impede o
   `.acmi` local de ser escrito, e vice-versa. `initialized` significa "há algum destino ativo".
8. **`Name=` é o MODELO, não o nome do player.** Mandar o nome do player em `Name=` (`"uav1"`) faz
   o Tacview procurar isso na base de designações ICAO/OTAN, não achar, e cair num objeto genérico
   — o nome do player pertence a `CallSign=`/`Pilot=`. Ver o comentário de `ObjectInfo` em
   `RealtimeTelemetryServer.hpp`.

## Por que é `static_library()`, não `shared_library()`

Ao contrário de `xboard`/`xlog`/`xtrack`/`xrlbridge`/`xinfer`/`xpyembed` (as seis que cruzam a
fronteira de plugin `dlopen`), `xtacview` não precisa de estado compartilhado entre host e modelo
— é consumida só pelo host, direto do slot `dataRecorder:` da `Station`. Por isso um plugin
**não pode** linkar `xtacview_dep`: ganharia cópia própria dos estáticos dela (mesma regra
documentada em `libs/xplugin/README.md`).

## `ExposedDataRecorder`

`mixr::recorder::DataRecorder` guarda `getOutputHandler()` como `protected` — sem alcance de fora
não há como o host achar o `TacviewOutput` para chamar `publishIdentities()`/`updateRadarScan()`.
`ExposedDataRecorder` é o mesmo `DataRecorder` nativo, só republicando esse getter como público
(`using recorder::DataRecorder::getOutputHandler;`); trocar `( DataRecorder )` por
`( ExposedDataRecorder )` no `.edl` roda idêntico — mesmo objeto, nenhum slot novo.

## Testes

`tests/scenario/run_tacview_identity_test.py` — prova a resolução de identidade (`typeMap`/
`colorMap`/`modelMap`, inclusive para um míssil criado em runtime) ponta a ponta contra o
binário de verdade.
