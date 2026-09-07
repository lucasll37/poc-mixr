# `libs/xtrack` — qual é o contato hostil mais próximo

Uma única pergunta — "qual é o contato hostil mais próximo?" — respondida num só lugar, para que
a percepção do UBF e o `track=`/`trackRange=` do dump da aplicação nunca digam coisas diferentes.

## Como se usar

Duas funções, sem estado, chamadas dos dois lados da fronteira de plugin.

**Do lado do modelo** (`models/player/A4/src/ubf/FlightState.cpp`, a percepção que alimenta a
árvore de comportamento inteira):

```cpp
#include "xtrack/TrackQuery.hpp"

// --- contato: pista do radar NATIVO (Antenna/Tws -> AirTrkMgr) ---
const xtrack::TrackInfo track{xtrack::nearestHostileTrack(air)};
if (track.found) {
   s.hasContact = true;
   s.contactName = track.name;
   s.contactRangeM = track.rangeM;
   s.contactRelBearingDeg = track.relBearingDeg;
   s.contactDeltaAltM = track.deltaAltM;
   // ...
}
```

**Do lado do host** (`app/src/app/DeterministicDump.cpp`, o dump `frame=` que os `check-*`
comparam):

```cpp
#include "xtrack/TrackQuery.hpp"

const mixr::xtrack::TrackInfo track{mixr::xtrack::nearestHostileTrack(air)};
oss << " track=" << (track.found ? track.name : std::string("none"))
    << " trackRange=" << track.rangeM;
```

`air` é um `const models::AirVehicle*` — qualquer um, não só `falcon1..4`. Não há nada para
declarar em `.edl`: `nearestHostileTrack()` não é uma classe MIXR registrada em factory, é só uma
função que lê o `TrackManager` já vivo (`twsTrkMgr`, ver `configs/scenario.edl.in` de qualquer poc)
— o cenário só precisa ter o radar/`OnboardComputer` configurados como sempre.

`TrackInfo` devolvido:

```cpp
struct TrackInfo {
   bool found{};
   std::string name;       // nome do player alvo, ou "trk<id>" se anonimo
   double rangeM{};
   double relBearingDeg{};
   double relNorthM{};      // vetor NED do contato RELATIVO ao ownship
   double relEastM{};
   double deltaAltM{};      // positivo = contato acima
};
```

## Duas regras que não são do sensor, e por isso moram aqui

O caminho é sempre o mesmo do framework: `AirVehicle -> OnboardComputer ->
TrackManager("twsTrkMgr") -> Track`. Duas decisões, deliberadamente fora do radar nativo:

1. **O radar nativo não filtra por lado.** `playerOfInterestTypes` filtra por *tipo* de player,
   não por `side` — a esquadrilha inteira entra na lista de pistas do `TrackManager`. Separar
   amigo de inimigo é decisão **tática**, não de sensor, e é o que `selectNearestHostileIndex()`
   faz.
2. **Desempate determinístico**: menor `rangeM` e, em empate exato, menor `trackId`. A lista de
   pistas do `TrackManager` não tem ordem garantida entre execuções com números diferentes de
   threads T/C — sem essa regra o resultado dependeria de quem chegou primeiro na lista, e o dump
   deixaria de ser byte-idêntico entre 1/2/4 threads.

`selectNearestHostileIndex(candidates, ownSide)` é a regra pura, separada da travessia que a
alimenta — testável sem `Station`/`Player`/`Track` ao vivo, só os três campos que ela de fato lê
(`TrackCandidate{trackId, rangeM, hasResolvedTarget, side}`). Uma pista sem alvo resolvido
(`trk->getTarget() == nullptr`, comum em pista ainda não correlacionada) **nunca** é filtrada por
lado — `side` só é consultado quando `hasResolvedTarget == true` — e compete normalmente por
alcance contra qualquer hostil.

**Não confundir com `RadarScan`** (`models/player/A4/include/xnative/RadarScan.hpp`, do modelo, não
desta lib): `RadarScan` lê para onde a antena está *apontando agora* (`Gimbal::getAzimuthD()`
etc.), para alimentar a varredura no Tacview — é o *apontamento*. `TrackQuery` lê o *contato já
detectado* — é a pista.

## Por que é `shared_library()`, e não estática

Mesmo argumento de [`xboard`](../xboard/Board.hpp): é a única peça disputada pelos dois lados da
fronteira de `dlopen`. O host precisa dela para o `track=`/`trackRange=` do dump (e do painel do
`./app`); o modelo precisa dela para a percepção (`ubf::FlightState`). A alternativa seria
compilar o mesmo `.cpp` dos dois lados — funciona (as funções não têm estado), mas depois que o
fonte do modelo saiu para `models/player/A4/`, isso viraria **duas cópias do arquivo em duas
árvores**, que divergem em silêncio. Uma `.so` a mais no SDK é mais barata que essa divergência.

## Armadilhas confirmadas

1. **`getTrackManagerByName()` não é `const` no framework**, embora a consulta seja de leitura —
   o `const_cast` fica confinado dentro de `nearestHostileTrack()`, não vaza para quem chama.
2. **Pistas nulas em `tracks[]` quebram a correspondência de índice.** `TrackManager::getTrackList()`
   preenche um array de `base::safe_ptr<Track>` que pode ter buracos; o `.cpp` mantém `origIndex[]`
   à parte para mapear de volta do índice em `candidates` (denso, sem buracos) para o índice
   original em `tracks[]` (com buracos).
3. **`deltaAltM` é o negativo da componente `IDOWN`** do vetor de posição relativa (`NED`) — o
   sinal já sai invertido do `.cpp` para que "positivo" signifique "contato acima", que é a
   convenção que o resto da aplicação espera.

## Testes

`tests/domain/test_track_selection.cpp` (alvo registrado na suíte `domain`, ver `tests/meson.build`)
cobre `selectNearestHostileIndex()` isolada — sem `Station`, sem `Player`/`Track` ao vivo: lista
vazia, só-amigos, hostil mais próximo ignorando amigo mais perto, empate de alcance resolvido pelo
menor `trackId`, independência da ordem da lista, e as duas variações de pista sem alvo resolvido
(sozinha, e competindo na mesma lista com amigo e hostil resolvidos). A travessia
`AirVehicle -> OnboardComputer -> TrackManager -> Track` em si só é exercitada com `Station` viva,
pelos testes de cenário que passam pelo `track=` do dump (`tests/scenario/`).
