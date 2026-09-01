#include "xplugin/factory.hpp"

#include "mixr/base/Object.hpp"
#include "xplugin/PluginLoader.hpp"
#include "xplugin/PluginModule.hpp"

#include <string>

namespace mixr {
namespace xplugin {

base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if ( name == PluginLoader::getFactoryName() ) {
      obj = new PluginLoader();
   } else if ( name == PluginModule::getFactoryName() ) {
      obj = new PluginModule();
   }

   return obj;
}

}
}
