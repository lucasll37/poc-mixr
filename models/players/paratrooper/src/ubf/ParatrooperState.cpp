#include "ubf/ParatrooperState.hpp"

#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {

IMPLEMENT_SUBCLASS(ParatrooperState, "ParatrooperState")
EMPTY_SLOTTABLE(ParatrooperState)
EMPTY_DELETEDATA(ParatrooperState)

ParatrooperState::ParatrooperState()
{
   STANDARD_CONSTRUCTOR()
}

// Copiada campo a campo, nao EMPTY_COPYDATA -- a leitura percebida e' estado
// de verdade, nao so cache descartavel.
void ParatrooperState::copyData(const ParatrooperState& org, const bool)
{
   BaseClass::copyData(org);
   valid = org.valid;
   aglM = org.aglM;
   altitudeM = org.altitudeM;
   terrainValid = org.terrainValid;
}

void ParatrooperState::updateState(const base::Component* const actor)
{
   const auto player = dynamic_cast<const models::Player*>(actor);
   if (player == nullptr) { valid = false; return; }

   aglM = player->getAltitudeAglM();
   altitudeM = player->getAltitudeM();
   terrainValid = player->isTerrainElevationValid();
   valid = true;
}

} // namespace xparatrooper
} // namespace models
} // namespace mixr
