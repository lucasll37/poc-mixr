#ifndef __xplugin_factory_H__
#define __xplugin_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; }
namespace xplugin {

// Factory das classes EDL desta biblioteca -- ( PluginLoader ) e
// ( PluginModule ). Encadeie no mixrFactory() de cada poc junto com as
// demais factories de shared/x<nome>.
//
// ATENCAO: esta factory NAO constroi as classes VINDAS de plugin -- para
// isso existe mixr::xplugin::loadedFactory(), que entra por ULTIMO na
// cadeia (ver PluginRegistry.hpp).
base::Object* factory(const std::string&);

}
}

#endif
