#include "domain/TerrainFloor.hpp"

#include <algorithm>

namespace domain {

double terrainFloorM(const GroundReference& ground, const double clearanceM,
                     const double absoluteFloorM)
{
   // clearanceM <= 0 desliga a camada de terreno sem desligar a rede
   // absoluta: e o que permite o controle negativo do cenario
   // (terrainClearance: ( Meters 0 ) devolve o comportamento de antes).
   if (!ground.valid || clearanceM <= 0.0) return absoluteFloorM;

   return std::max(absoluteFloorM, ground.elevationM + clearanceM);
}

double clampToTerrain(const double commandedAltM, const GroundReference& ground,
                      const double clearanceM, const double absoluteFloorM)
{
   return std::max(commandedAltM, terrainFloorM(ground, clearanceM, absoluteFloorM));
}

} // namespace domain
