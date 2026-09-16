#include "xterrain/factory.hpp"

#include "mixr/base/Object.hpp"
#include "xterrain/MultiTileTerrain.hpp"

#include <string>

namespace mixr {
namespace xterrain {

base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if ( name == MultiTileTerrain::getFactoryName() ) {
      obj = new MultiTileTerrain();
   }

   return obj;
}

}
}
