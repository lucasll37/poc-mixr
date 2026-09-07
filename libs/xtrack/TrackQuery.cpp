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

int selectNearestHostileIndex(const std::vector<TrackCandidate>& candidates,
                              const models::Player::Side ownSide)
{
   int bestIndex{-1};
   for (int i = 0; i < static_cast<int>(candidates.size()); i++) {
      const TrackCandidate& c{candidates[static_cast<std::size_t>(i)]};
      if (c.hasResolvedTarget && c.side == ownSide) continue;

      if (bestIndex < 0
          || c.rangeM < candidates[static_cast<std::size_t>(bestIndex)].rangeM
          || (c.rangeM == candidates[static_cast<std::size_t>(bestIndex)].rangeM
              && c.trackId < candidates[static_cast<std::size_t>(bestIndex)].trackId)) {
         bestIndex = i;
      }
   }
   return bestIndex;
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

   // Monta os candidatos (so os 3 campos que a regra de selecao le) e
   // delega a decisao a selectNearestHostileIndex() -- ver o "porque" da
   // separacao no header. 'origIndex' preserva o indice ORIGINAL em
   // 'tracks[]' (pistas nulas sao puladas, entao os indices dos dois
   // arrays divergem sem este mapeamento de volta).
   std::vector<TrackCandidate> candidates;
   std::vector<int> origIndex;
   candidates.reserve(static_cast<std::size_t>(n));
   origIndex.reserve(static_cast<std::size_t>(n));
   for (int i = 0; i < n; i++) {
      if (tracks[i] == nullptr) continue;
      const models::Player* const tgtPlayer{tracks[i]->getTarget()};
      candidates.push_back(TrackCandidate{
         tracks[i]->getTrackID(), tracks[i]->getRange(),
         tgtPlayer != nullptr, tgtPlayer != nullptr ? tgtPlayer->getSide() : models::Player::GRAY,
      });
      origIndex.push_back(i);
   }

   const int bestIdx{selectNearestHostileIndex(candidates, air->getSide())};
   if (bestIdx < 0) return info;

   const models::Track* const best{tracks[origIndex[static_cast<std::size_t>(bestIdx)]]};
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
