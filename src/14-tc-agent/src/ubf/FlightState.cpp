#include "ubf/FlightState.hpp"

#include "xnative/AlertDatalink.hpp"
#include "xnative/TrackQuery.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(FlightState, "FlightState")
EMPTY_SLOTTABLE(FlightState)
EMPTY_DELETEDATA(FlightState)

namespace {
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
//   contato                 -> xnative::nearestHostileTrack() (radar nativo)
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
   const TrackInfo track{nearestHostileTrack(air)};
   if (track.found) {
      s.hasContact = true;
      s.contactName = track.name;
      s.contactRangeM = track.rangeM;
      s.contactRelBearingDeg = track.relBearingDeg;
      s.contactDeltaAltM = track.deltaAltM;

      // A pista vem RELATIVA ao ownship; somando a nossa posicao sai a
      // posicao absoluta, que e o que vai no alerta para os outros avioes.
      s.contactNorthM = s.northM + track.relNorthM;
      s.contactEastM = s.eastM + track.relEastM;
      s.contactAltitudeM = s.altitudeM + track.deltaAltM;
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
