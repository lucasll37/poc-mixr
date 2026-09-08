#include "factory.hpp"

#include "xplugin/PluginRegistry.hpp"
#include "xplugin/factory.hpp"

#include "xtacview/factory.hpp"
#include "xclock/factory.hpp"
#include "xjoystick/factory.hpp"
#include "xmsg/factory.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/interop/dis/factory.hpp"
#include "mixr/linkage/factory.hpp"
#include "mixr/recorder/factory.hpp"
#include "mixr/base/factory.hpp"

namespace node {

mixr::base::Object* factoryBuiltin(const std::string& name)
{
   mixr::base::Object* obj{};

   // 0) classes EDL da carga dinamica: ( PluginLoader ) e ( PluginModule ).
   if (obj == nullptr) obj = mixr::xplugin::factory(name);

   // 1) exportacao para o Tacview.
   if (obj == nullptr) obj = mixr::xtacview::factory(name);

   // 2) ( ClockStation ) -- antes de simulation::factory(), que tambem
   //    responde por "Station" (ver libs/xclock/factory.hpp).
   if (obj == nullptr) obj = mixr::xclock::factory(name);

   // 3) ( JoystickIoHandler )/( UsbJoystick ) -- antes de linkage::factory()
   //    (ver libs/xjoystick/factory.hpp).
   if (obj == nullptr) obj = mixr::xjoystick::factory(name);

   // 4) ( MsgFeed )/( MsgReport )/...
   if (obj == nullptr) obj = mixr::xmsg::factory(name);

   // 5) framework nativo -- Aircraft, JSBSimModel, Autopilot, Gimbal,
   //    Antenna, sensores, track managers, DataRecorder...
   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);

   // 6) banco de elevacao -- simulation::/models::factory() NAO encadeiam
   //    terrain:: sozinhos; sem esta linha, ( SrtmHgtFile ) nao constroi
   //    nada, em silencio.
   if (obj == nullptr) obj = mixr::terrain::factory(name);

   // 7) DIS nativo -- ( DisNetIO )/( DisNtm ).
   if (obj == nullptr) obj = mixr::dis::factory(name);

   // 8) ( IoHandler )/( IoData )/adaptadores nativos de E/S.
   if (obj == nullptr) obj = mixr::linkage::factory(name);

   if (obj == nullptr) obj = mixr::recorder::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);

   return obj;
}

mixr::base::Object* factory(const std::string& name)
{
   mixr::base::Object* obj{factoryBuiltin(name)};

   // Classes vindas do plugin do modelo, carregado por dlopen em tempo de
   // execucao -- POR ULTIMO, para so ACRESCENTAR nomes, nunca sombrear em
   // silencio uma classe que ja funciona.
   if (obj == nullptr) obj = mixr::xplugin::loadedFactory(name);

   if (obj == nullptr) mixr::xplugin::reportUnknownFactoryName(name); // [[noreturn]]

   return obj;
}

}
