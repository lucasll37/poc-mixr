#include "ubf/AaaState.hpp"

#include "xtrack/TrackQuery.hpp"

#include "mixr/models/player/ground/SamVehicle.hpp"
#include "mixr/models/system/StoresMgr.hpp"

namespace mixr {
namespace models {
namespace xaaa {

IMPLEMENT_SUBCLASS(AaaState, "AaaState")
EMPTY_SLOTTABLE(AaaState)
EMPTY_DELETEDATA(AaaState)

AaaState::AaaState()
{
   STANDARD_CONSTRUCTOR()
}

// Copiada campo a campo, nao EMPTY_COPYDATA -- e' a ULTIMA leitura (estado
// de verdade), nao so' cache descartavel; mesmo raciocinio de
// models/players/A-4/src/ubf/FlightState.cpp.
void AaaState::copyData(const AaaState& org, const bool)
{
   BaseClass::copyData(org);
   hasTarget_ = org.hasTarget_;
   targetName_ = org.targetName_;
   targetRangeM_ = org.targetRangeM_;
   weaponReady_ = org.weaponReady_;
   dome_ = org.dome_;
}

void AaaState::updateState(const base::Component* const actor)
{
   // SamVehicle, nao Player: e' de onde vem getMinLaunchRange()/
   // getMaxLaunchRange() (o domo). xnative::AaaSite e' subclasse direta de
   // SamVehicle -- este cast enxerga qualquer antiaerea montada assim, sem
   // este arquivo (ubf/) precisar incluir xnative/AaaSite.hpp e criar uma
   // dependencia de camada que o resto do modelo nao tem.
   const auto site = dynamic_cast<const models::SamVehicle*>(actor);
   if (site == nullptr) return;

   dome_.minRangeM = site->getMinLaunchRange();
   dome_.maxRangeM = site->getMaxLaunchRange();

   const models::StoresMgr* const storesMgr{site->getStoresManagement()};
   weaponReady_ = (storesMgr != nullptr) && (storesMgr->available() > 0);

   const xtrack::TrackInfo track{xtrack::nearestHostileTrack(site, "aaaTrkMgr")};
   hasTarget_ = track.found;
   targetName_ = track.name;
   targetRangeM_ = track.rangeM;
}

} // namespace xaaa
} // namespace models
} // namespace mixr
