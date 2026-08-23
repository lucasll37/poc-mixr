#include "xair/factory.hpp"

#include "ubf/AltitudeSafetyBehavior.hpp"
#include "ubf/BtBehavior.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightAgent.hpp"
#include "ubf/FlightState.hpp"
#include "xair/Airplane.hpp"
#include "xair/AlertRadio.hpp"
#include "xair/FlightDirector.hpp"
#include "xair/JsbsimFlightModel.hpp"
#include "xair/ProximitySensor.hpp"
#include "xair/TacticalAlert.hpp"

#include "mixr/base/Object.hpp"

namespace mixr {
namespace xair {

base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   // player e subsistemas
   if ( name == Airplane::getFactoryName() )                 obj = new Airplane();
   else if ( name == JsbsimFlightModel::getFactoryName() )   obj = new JsbsimFlightModel();
   else if ( name == FlightDirector::getFactoryName() )      obj = new FlightDirector();
   else if ( name == ProximitySensor::getFactoryName() )     obj = new ProximitySensor();
   else if ( name == AlertRadio::getFactoryName() )          obj = new AlertRadio();
   else if ( name == TacticalAlert::getFactoryName() )       obj = new TacticalAlert();

   // pecas do UBF (o Arbiter e o do framework: 'UbfArbiter')
   else if ( name == FlightAgent::getFactoryName() )         obj = new FlightAgent();
   else if ( name == FlightState::getFactoryName() )         obj = new FlightState();
   else if ( name == BtBehavior::getFactoryName() )          obj = new BtBehavior();
   else if ( name == AltitudeSafetyBehavior::getFactoryName() ) obj = new AltitudeSafetyBehavior();
   else if ( name == FlightAction::getFactoryName() )        obj = new FlightAction();

   return obj;
}

} // namespace xair
} // namespace mixr
