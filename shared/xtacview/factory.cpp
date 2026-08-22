#include "xtacview/factory.hpp"

#include "mixr/base/Object.hpp"
#include "xtacview/TacviewOutput.hpp"

#include <string>

namespace mixr {
namespace xtacview {

base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if ( name == TacviewOutput::getFactoryName() ) {
      obj = new TacviewOutput();
   }

   return obj;
}

}
}
