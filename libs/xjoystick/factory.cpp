#include "xjoystick/factory.hpp"

#include "mixr/base/Object.hpp"
#include "xjoystick/JoystickIoHandler.hpp"

#include <string>

namespace mixr {
namespace xjoystick {

base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if ( name == JoystickIoHandler::getFactoryName() ) {
      obj = new JoystickIoHandler();
   }

   return obj;
}

}
}
