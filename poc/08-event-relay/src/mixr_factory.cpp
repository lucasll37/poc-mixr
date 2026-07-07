#include "mixr_factory.hpp"

#include "RadarContactRelay.hpp"
#include "AlertReceiver.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/interop/dis/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/base/factory.hpp"

mixr::base::Object* mixrFactory(const std::string& name)
{
   mixr::base::Object* obj{};
   if (name == "RadarContactRelay") obj = new events::RadarContactRelay();
   if (obj == nullptr && name == "AlertReceiver") obj = new events::AlertReceiver();

   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);
   if (obj == nullptr) obj = mixr::terrain::factory(name);
   if (obj == nullptr) obj = mixr::dis::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);
   return obj;
}
