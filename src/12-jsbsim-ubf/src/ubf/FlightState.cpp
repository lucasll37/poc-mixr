#include "ubf/FlightState.hpp"

#include "xair/Airplane.hpp"
#include "xair/AlertRadio.hpp"
#include "xair/JsbsimFlightModel.hpp"
#include "xair/ProximitySensor.hpp"

#include "domain/geometry.hpp"

#include <cmath>

namespace mixr {
namespace xair {

IMPLEMENT_SUBCLASS(FlightState, "FlightState")
EMPTY_SLOTTABLE(FlightState)
EMPTY_DELETEDATA(FlightState)

FlightState::FlightState()
{
   STANDARD_CONSTRUCTOR()
}

void FlightState::copyData(const FlightState& org, const bool)
{
   BaseClass::copyData(org);
   snap = org.snap;
}

void FlightState::updateState(const base::Component* const actor)
{
   BaseClass::updateState(actor);

   const auto airplane = dynamic_cast<const Airplane*>(actor);
   if (airplane == nullptr) {
      snap = Snapshot{};
      return;
   }

   Snapshot s;
   s.valid = true;

   const base::Vec3d pos{airplane->getPosition()};
   s.northM = pos[models::Player::INORTH];
   s.eastM = pos[models::Player::IEAST];
   s.altitudeM = airplane->getAltitudeM();
   s.headingDeg = airplane->getHeadingD();
   s.speedKts = airplane->getTotalVelocityKts();
   s.rollDeg = airplane->getRollD();
   s.pitchDeg = airplane->getPitchD();

   const xair::JsbsimFlightModel* const model{airplane->getFlightModel()};
   if (model != nullptr) {
      s.fuelFraction = model->getFuelFraction();
      s.mach = model->getMach();
      s.gLoad = model->getGLoad();
      s.alphaDeg = model->getAlphaDeg();
   }

   const xair::ProximitySensor* const sensor{airplane->getProximitySensor()};
   if (sensor != nullptr) {
      const auto contact = sensor->getContact();
      s.hasContact = contact.valid;
      if (contact.valid) {
         s.contactName = contact.name;
         s.contactRangeM = contact.rangeM;
         s.contactRelBearingDeg = contact.relBearingDeg;
         s.contactDeltaAltM = contact.deltaAltM;

         // Posicao absoluta do contato: e ISSO que vai no alerta para os
         // outros avioes (marcacao relativa so faz sentido para quem viu).
         const double bearingDeg{domain::wrap360(s.headingDeg + contact.relBearingDeg)};
         const double bearingRad{bearingDeg * 0.017453292519943295};
         s.contactNorthM = s.northM + contact.rangeM * std::cos(bearingRad);
         s.contactEastM = s.eastM + contact.rangeM * std::sin(bearingRad);
         s.contactAltitudeM = s.altitudeM + contact.deltaAltM;
      }
   }

   const xair::AlertRadio* const radio{airplane->getAlertRadio()};
   if (radio != nullptr) {
      const auto alert = radio->getAlert();
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

} // namespace xair
} // namespace mixr
