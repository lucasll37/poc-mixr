#include "xtrack/TrackQuery.hpp"

#include "mixr/models/Track.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/OnboardComputer.hpp"
#include "mixr/models/system/trackmanager/TrackManager.hpp"

#include "mixr/base/safe_ptr.hpp"

#include <string>

namespace mixr {
namespace xtrack {

namespace {
const int MAX_TRACKS{20};
const char* const TRACK_MANAGER_NAME{"twsTrkMgr"};
}

TrackInfo nearestHostileTrack(const models::AirVehicle* const air)
{
   TrackInfo info;
   if (air == nullptr) return info;

   // getTrackManagerByName() nao e const no framework, mas a consulta e de
   // leitura -- o const_cast fica confinado aqui.
   const auto obc = const_cast<models::OnboardComputer*>(air->getOnboardComputer());
   if (obc == nullptr) return info;

   models::TrackManager* const trkMgr{obc->getTrackManagerByName(TRACK_MANAGER_NAME)};
   if (trkMgr == nullptr) return info;

   base::safe_ptr<models::Track> tracks[MAX_TRACKS];
   const int n{trkMgr->getTrackList(tracks, MAX_TRACKS)};

   const models::Track* best{};
   for (int i = 0; i < n; i++) {
      const models::Track* const trk{tracks[i]};
      if (trk == nullptr) continue;

      const models::Player* const tgtPlayer{trk->getTarget()};
      if (tgtPlayer != nullptr && tgtPlayer->getSide() == air->getSide()) continue;

      if (best == nullptr
          || trk->getRange() < best->getRange()
          || (trk->getRange() == best->getRange() && trk->getTrackID() < best->getTrackID())) {
         best = trk;
      }
   }
   if (best == nullptr) return info;

   info.found = true;
   info.rangeM = best->getRange();
   info.relBearingDeg = best->getRelAzimuthD();

   const base::Vec3d& rel{best->getPosition()};
   info.relNorthM = rel[models::Player::INORTH];
   info.relEastM = rel[models::Player::IEAST];
   info.deltaAltM = -rel[models::Player::IDOWN];

   const models::Player* const tgt{best->getTarget()};
   info.name = (tgt != nullptr && tgt->getName() != nullptr)
                ? tgt->getName()->getString()
                : ("trk" + std::to_string(best->getTrackID()));
   return info;
}

} // namespace xtrack
} // namespace mixr
