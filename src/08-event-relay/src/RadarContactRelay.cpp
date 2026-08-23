#include "RadarContactRelay.hpp"
#include "events.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/system/OnboardComputer.hpp"
#include "mixr/models/system/trackmanager/TrackManager.hpp"
#include "mixr/models/Track.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/safe_ptr.hpp"

#include <iostream>

namespace events {

IMPLEMENT_SUBCLASS(RadarContactRelay, "RadarContactRelay")

BEGIN_SLOTTABLE(RadarContactRelay)
   "trackManagerName",
   "localComponentName",
   "relayToPlayerName",
   "relayToComponentName",
END_SLOTTABLE(RadarContactRelay)

BEGIN_SLOT_MAP(RadarContactRelay)
   ON_SLOT(1, setSlotTrackManagerName,     mixr::base::Identifier)
   ON_SLOT(2, setSlotLocalComponentName,   mixr::base::Identifier)
   ON_SLOT(3, setSlotRelayToPlayerName,    mixr::base::Identifier)
   ON_SLOT(4, setSlotRelayToComponentName, mixr::base::Identifier)
END_SLOT_MAP()

RadarContactRelay::RadarContactRelay()
{
   STANDARD_CONSTRUCTOR()
}

void RadarContactRelay::copyData(const RadarContactRelay& org, const bool)
{
   BaseClass::copyData(org);
   trackManagerName = org.trackManagerName;
   localComponentName = org.localComponentName;
   relayToPlayerName = org.relayToPlayerName;
   relayToComponentName = org.relayToComponentName;
   // trackManager/knownTrackIds/*SendData sao estado de execucao, nao
   // configuracao -- nao copiados (resolvidos/populados de novo em runtime).
}

void RadarContactRelay::deleteData()
{
}

bool RadarContactRelay::setSlotTrackManagerName(const mixr::base::Identifier* const x)
{
   trackManagerName = x;
   return true;
}

bool RadarContactRelay::setSlotLocalComponentName(const mixr::base::Identifier* const x)
{
   localComponentName = x;
   return true;
}

bool RadarContactRelay::setSlotRelayToPlayerName(const mixr::base::Identifier* const x)
{
   relayToPlayerName = x;
   return true;
}

bool RadarContactRelay::setSlotRelayToComponentName(const mixr::base::Identifier* const x)
{
   relayToComponentName = x;
   return true;
}

void RadarContactRelay::process(const double dt)
{
   BaseClass::process(dt);

   if (trackManager == nullptr) {
      mixr::models::Player* me{getOwnship()};
      if (me == nullptr || trackManagerName == nullptr) return;
      mixr::models::OnboardComputer* obc{me->getOnboardComputer()};
      if (obc == nullptr) return;
      trackManager = obc->getTrackManagerByName(trackManagerName->getString());
      if (trackManager == nullptr) return;
   }

   const unsigned int maxTracks{16};
   mixr::base::safe_ptr<mixr::models::Track> trackList[maxTracks];
   const int numTracks{trackManager->getTrackList(trackList, maxTracks)};

   for (int i = 0; i < numTracks; i++) {
      const mixr::models::Track* const trk{trackList[i]};
      const int id{trk->getTrackID()};
      if (!knownTrackIds.insert(id).second) continue; // ja conhecida

      const mixr::models::Player* const tgtPlayer{trk->getTarget()};
      const std::string targetName{tgtPlayer != nullptr ? tgtPlayer->getName()->getString() : "?"};
      const double rangeNM{trk->getRange() / 1852.0};
      const double bearingDeg{trk->getTrueAzimuthD()};
      const mixr::models::Player* const me{getOwnship()};
      const std::string myName{me != nullptr && me->getName() != nullptr ? me->getName()->getString() : "?"};

      std::cout << "[RadarContactRelay] " << myName << ": nova pista #" << id
                << " (" << targetName << ") -- disparando evento CONTACT_EVENT" << std::endl;

      // 1) hop local: um componente IRMAO (filho do hunter, nao de nos
      //    mesmos -- send() endereca pelos FILHOS de quem o chama, entao
      //    precisa ser chamado no hunter, nao no proprio RadarContactRelay)
      if (localComponentName != nullptr) {
         mixr::models::Player* const myPlayer{getOwnship()};
         if (myPlayer != nullptr) {
            const auto msg = new RadarContactMessage(id, targetName, rangeNM, bearingDeg, myName);
            myPlayer->send(localComponentName->getString(), CONTACT_EVENT,
                            static_cast<mixr::base::Object*>(msg), localSendData);
            msg->unref();
         }
      }

      // 2) hop remoto: OUTRO player, achado pelo nome na lista global do
      //    WorldModel (mesma tecnica que Datalink::sendMessage() usa) --
      //    dai o MESMO send() nativo, so que chamado no player remoto.
      if (relayToPlayerName != nullptr && relayToComponentName != nullptr) {
         mixr::models::WorldModel* wm{getWorldModel()};
         if (wm != nullptr) {
            mixr::base::PairStream* players{wm->getPlayers()};
            if (players != nullptr) {
               mixr::base::Pair* p{players->findByName(relayToPlayerName->getString())};
               if (p != nullptr) {
                  const auto remote = dynamic_cast<mixr::models::Player*>(p->object());
                  if (remote != nullptr) {
                     const auto msg = new RadarContactMessage(id, targetName, rangeNM, bearingDeg, myName);
                     remote->send(relayToComponentName->getString(), CONTACT_EVENT,
                                  static_cast<mixr::base::Object*>(msg), remoteSendData);
                     msg->unref();
                  }
               }
               players->unref();
            }
         }
      }
   }
}

} // namespace events
