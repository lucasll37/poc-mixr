#pragma once

namespace formations {

enum class Formation { TRAIL, WEDGE, LINE, VIC };

// Offset relativo ao lider, em metros, consumido diretamente pelos slots
// nativos do Autopilot (leadFollowingDistanceTrail/Right/DeltaAltitude).
// So dados -- nenhuma logica de voo aqui, quem converge pro slot e o
// followTheLeadMode nativo do Autopilot.
struct Offset
{
   double trailM{};   // atras (+) do lider
   double rightM{};   // direita (+) do lider
   double altOffsetM{}; // acima (+) do lider
};

// slotIndex: 0..3 (wing1..wing4)
Offset formationOffset(const Formation formation, const int slotIndex);

Formation formationFromKey(const char key); // '1'..'4' -> formacao; TRAIL se invalido
const char* formationName(const Formation formation);

// Estado compartilhado entre o KeyboardIoHandler (quem escreve, ao ler as
// teclas de formacao/RTB) e as arvores de comportamento dos 4 wingmen
// (quem le, via blackboard pai compartilhado -- ver bt/nodes). Nao e um
// tipo do MIXR; e o unico jeito pratico de propagar "formacao atual" e
// "RTB engajado" pros 4 wingmen sem repetir o parsing de teclado 4 vezes.
struct FormationState
{
   Formation current{Formation::TRAIL};
   bool rtbEngaged{};
};

} // namespace formations
