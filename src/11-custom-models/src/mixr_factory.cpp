#include "mixr_factory.hpp"

#include "xdrone/factory.hpp"
#include "xtacview/factory.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/recorder/factory.hpp"
#include "mixr/base/factory.hpp"

mixr::base::Object* mixrFactory(const std::string& name)
{
   mixr::base::Object* obj{};

   // 1) classes proprias desta poc (Drone, DroneDynamics, FuelSystem,
   //    ProximitySensor, BtPilot) -- vem primeiro justamente para poder
   //    SUBSTITUIR um nome do framework, se um dia for preciso
   if (obj == nullptr) obj = mixr::xdrone::factory(name);

   // 2) exportacao para o Tacview (shared/xtacview)
   if (obj == nullptr) obj = mixr::xtacview::factory(name);

   // 3) framework
   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);
   if (obj == nullptr) obj = mixr::recorder::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);

   return obj;
}
