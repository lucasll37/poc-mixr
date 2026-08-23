#include "ubf/FlightAction.hpp"

#include "xair/Airplane.hpp"
#include "xair/AlertRadio.hpp"
#include "xair/FlightDirector.hpp"

namespace mixr {
namespace xair {

IMPLEMENT_SUBCLASS(FlightAction, "FlightAction")
EMPTY_SLOTTABLE(FlightAction)
EMPTY_DELETEDATA(FlightAction)

FlightAction::FlightAction()
{
   STANDARD_CONSTRUCTOR()
}

void FlightAction::copyData(const FlightAction& org, const bool)
{
   BaseClass::copyData(org);

   command = org.command;
   label = org.label;
   broadcast = org.broadcast;
   alertContactName = org.alertContactName;
   alertNorthM = org.alertNorthM;
   alertEastM = org.alertEastM;
   alertAltitudeM = org.alertAltitudeM;
   alertRangeM = org.alertRangeM;
}

void FlightAction::setAlertBroadcast(const std::string& contactName,
                                     const double northM, const double eastM,
                                     const double altitudeM, const double rangeM)
{
   broadcast = true;
   alertContactName = contactName;
   alertNorthM = northM;
   alertEastM = eastM;
   alertAltitudeM = altitudeM;
   alertRangeM = rangeM;
}

bool FlightAction::execute(base::Component* actor)
{
   const auto airplane = dynamic_cast<Airplane*>(actor);
   if (airplane == nullptr) return false;

   FlightDirector* const director{airplane->getFlightDirector()};
   if (director == nullptr) return false;

   director->setCommand(command);
   airplane->setBehaviorLabel(label);

   if (broadcast) {
      AlertRadio* const radio{airplane->getAlertRadio()};
      if (radio != nullptr) {
         radio->requestBroadcast(alertContactName, alertNorthM, alertEastM,
                                 alertAltitudeM, alertRangeM);
      }
   }

   return true;
}

} // namespace xair
} // namespace mixr
