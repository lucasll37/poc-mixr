#include "ubf/FlightState.hpp"

#include "xnative/AlertDatalink.hpp"

#include "mixr/models/Track.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/OnboardComputer.hpp"
#include "mixr/models/system/trackmanager/TrackManager.hpp"

#include "mixr/base/safe_ptr.hpp"

#include <string>

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(FlightState, "FlightState")
EMPTY_SLOTTABLE(FlightState)
EMPTY_DELETEDATA(FlightState)

namespace {
const int MAX_TRACKS{20};
const double RAD2DEG{57.295779513082320876798154814105};
}

FlightState::FlightState()
{
   STANDARD_CONSTRUCTOR()
}

void FlightState::copyData(const FlightState& org, const bool)
{
   BaseClass::copyData(org);
   snap = org.snap;
}

//------------------------------------------------------------------------------
// updateState() -- percepcao, agora lendo SO acessores nativos.
//
// Compare com a poc/12: la o estado consultava as NOSSAS classes
// (JsbsimFlightModel, ProximitySensor, AlertRadio). Aqui:
//
//   combustivel/mach/G/AoA  -> AirVehicle (que delega ao dynamics model)
//   contato                 -> OnboardComputer -> TrackManager -> Track
//   alerta                  -> Player::getDatalink()
//
// Nenhum updateSystemPointers() nosso: o Player ja resolve DynamicsModel,
// OnboardComputer e Datalink POR TIPO, sozinho.
//------------------------------------------------------------------------------
void FlightState::updateState(const base::Component* const actor)
{
   BaseClass::updateState(actor);

   const auto air = dynamic_cast<const models::AirVehicle*>(actor);
   if (air == nullptr) {
      snap = Snapshot{};
      return;
   }

   Snapshot s;
   s.valid = true;

   const base::Vec3d& pos{air->getPosition()};
   s.northM = pos[models::Player::INORTH];
   s.eastM = pos[models::Player::IEAST];
   s.altitudeM = air->getAltitudeM();
   s.headingDeg = air->getHeadingD();
   s.speedKts = air->getTotalVelocityKts();
   s.rollDeg = air->getRollD();
   s.pitchDeg = air->getPitchD();

   // Telemetria do 6-DOF -- tudo via AirVehicle, que repassa ao JSBSimModel
   const double fuelMax{air->getFuelWtMax()};
   s.fuelFraction = (fuelMax > 0.0) ? (air->getFuelWt() / fuelMax) : 1.0;
   s.mach = air->getMach();
   s.gLoad = air->getGload();
   s.alphaDeg = air->getAngleOfAttack() * RAD2DEG;

   // --- contato: pista do radar NATIVO (Antenna/Tws -> AirTrkMgr) ---
   const auto obc = const_cast<models::OnboardComputer*>(air->getOnboardComputer());
   if (obc != nullptr) {
      models::TrackManager* const trkMgr{obc->getTrackManagerByName("twsTrkMgr")};
      if (trkMgr != nullptr) {
         base::safe_ptr<models::Track> tracks[MAX_TRACKS];
         const int n{trkMgr->getTrackList(tracks, MAX_TRACKS)};

         const models::Track* best{};
         for (int i = 0; i < n; i++) {
            const models::Track* const trk{tracks[i]};
            if (trk == nullptr) continue;

            // O radar NATIVO nao filtra por lado (playerOfInterestTypes so
            // filtra por TIPO de player): a esquadrilha inteira aparece na
            // lista de pistas. Filtrar amigo/inimigo e decisao tatica, e o
            // lugar dela e aqui -- nao no sensor.
            const models::Player* const tgtPlayer{trk->getTarget()};
            if (tgtPlayer != nullptr && tgtPlayer->getSide() == air->getSide()) continue;
            // Desempate deterministico: menor distancia e, em empate exato,
            // menor id de pista -- para nao depender da ordem da lista.
            if (best == nullptr
                || trk->getRange() < best->getRange()
                || (trk->getRange() == best->getRange() && trk->getTrackID() < best->getTrackID())) {
               best = trk;
            }
         }

         if (best != nullptr) {
            s.hasContact = true;
            s.contactRangeM = best->getRange();
            s.contactRelBearingDeg = best->getRelAzimuthD();

            // Track::getPosition() e o vetor NED do contato RELATIVO ao
            // ownship: somando a nossa posicao sai a posicao absoluta, que
            // e o que vai no alerta para os outros avioes.
            const base::Vec3d& rel{best->getPosition()};
            s.contactDeltaAltM = -rel[models::Player::IDOWN];
            s.contactNorthM = s.northM + rel[models::Player::INORTH];
            s.contactEastM = s.eastM + rel[models::Player::IEAST];
            s.contactAltitudeM = s.altitudeM + s.contactDeltaAltM;

            const models::Player* const tgt{best->getTarget()};
            s.contactName = (tgt != nullptr && tgt->getName() != nullptr)
                             ? tgt->getName()->getString()
                             : ("trk" + std::to_string(best->getTrackID()));
         }
      }
   }

   // --- alerta recebido pelo datalink NATIVO ---
   const auto datalink = dynamic_cast<const AlertDatalink*>(air->getDatalink());
   if (datalink != nullptr) {
      const auto alert = datalink->getAlert();
      s.hasAlert = alert.valid;
      if (alert.valid) {
         s.alertSender = alert.senderName;
         s.alertContactName = alert.contactName;
         s.alertNorthM = alert.northM;
         s.alertEastM = alert.eastM;
         s.alertAltitudeM = alert.altitudeM;
         s.alertRangeM = alert.rangeM;
      }
   }

   snap = s;
}

} // namespace xnative
} // namespace mixr
