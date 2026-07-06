#include "mixr_factory.hpp"

#include "linkage/KeyboardDevice.hpp"
#include "linkage/KeyboardIoHandler.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/interop/dis/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/recorder/factory.hpp"
#include "mixr/linkage/factory.hpp"
#include "mixr/base/factory.hpp"

mixr::base::Object* mixrFactory(const std::string& name)
{
   // classe propria desta PoC: tentar primeiro, cair pras do framework
   mixr::base::Object* obj{};
   if (name == "KeyboardDevice") obj = new keyboard::KeyboardDevice();
   if (obj == nullptr && name == "KeyboardIoHandler") obj = new keyboard::KeyboardIoHandler();

   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);
   if (obj == nullptr) obj = mixr::terrain::factory(name);
   if (obj == nullptr) obj = mixr::dis::factory(name);
   if (obj == nullptr) obj = mixr::recorder::factory(name);
   if (obj == nullptr) obj = mixr::linkage::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);
   return obj;
}
