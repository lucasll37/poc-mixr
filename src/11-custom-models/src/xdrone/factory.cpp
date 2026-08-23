#include "xdrone/factory.hpp"

#include "xdrone/BtPilot.hpp"
#include "xdrone/Drone.hpp"
#include "xdrone/DroneDynamics.hpp"
#include "xdrone/FuelSystem.hpp"
#include "xdrone/ProximitySensor.hpp"

#include "mixr/base/Object.hpp"

namespace mixr {
namespace xdrone {

base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   // player proprio
   if ( name == Drone::getFactoryName() ) {
      obj = new Drone();
   }
   // dynamics model proprio
   else if ( name == DroneDynamics::getFactoryName() ) {
      obj = new DroneDynamics();
   }
   // subsistemas proprios
   else if ( name == FuelSystem::getFactoryName() ) {
      obj = new FuelSystem();
   }
   else if ( name == ProximitySensor::getFactoryName() ) {
      obj = new ProximitySensor();
   }
   else if ( name == BtPilot::getFactoryName() ) {
      obj = new BtPilot();
   }

   return obj;
}

} // namespace xdrone
} // namespace mixr
