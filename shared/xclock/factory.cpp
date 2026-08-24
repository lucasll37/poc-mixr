#include "xclock/factory.hpp"

#include "mixr/base/Object.hpp"
#include "xclock/ClockStation.hpp"

#include <string>

namespace mixr {
namespace xclock {

base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if ( name == ClockStation::getFactoryName() ) {
      obj = new ClockStation();
   }

   return obj;
}

}
}
