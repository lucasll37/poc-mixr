#include "formations.hpp"

namespace formations {

namespace {
// Separacao vertical de seguranca entre wingmen (evita sobreposicao exata
// de altitude independente da formacao horizontal escolhida).
double altStagger(const int slotIndex)
{
   return -20.0 * static_cast<double>(slotIndex + 1);
}
}

Offset formationOffset(const Formation formation, const int slotIndex)
{
   const double alt{altStagger(slotIndex)};
   const bool rightSide{(slotIndex % 2) == 0}; // wing1,wing3 -> direita; wing2,wing4 -> esquerda
   const double side{rightSide ? 1.0 : -1.0};
   const int pairIndex{slotIndex / 2}; // 0 para o par mais proximo, 1 para o mais distante

   switch (formation) {
      case Formation::TRAIL:
         // fila unica atras do lider, sem afastamento lateral
         return Offset{200.0 * (slotIndex + 1), 0.0, alt};

      case Formation::LINE:
         // lado a lado com o lider, sem afastamento longitudinal
         return Offset{0.0, side * 200.0 * (pairIndex + 1), alt};

      case Formation::WEDGE:
         // "V" aberto para tras, mais largo
         return Offset{200.0 * (pairIndex + 1), side * 200.0 * (pairIndex + 1), alt};

      case Formation::VIC:
         // "V" classico, mais fechado que o wedge
         return Offset{100.0 * (pairIndex + 1), side * 150.0 * (pairIndex + 1), alt};
   }
   return Offset{200.0, 0.0, alt};
}

Formation formationFromKey(const char key)
{
   switch (key) {
      case '1': return Formation::TRAIL;
      case '2': return Formation::WEDGE;
      case '3': return Formation::LINE;
      case '4': return Formation::VIC;
      default:  return Formation::TRAIL;
   }
}

const char* formationName(const Formation formation)
{
   switch (formation) {
      case Formation::TRAIL: return "TRAIL";
      case Formation::WEDGE: return "WEDGE";
      case Formation::LINE:  return "LINE";
      case Formation::VIC:   return "VIC";
   }
   return "?";
}

} // namespace formations
