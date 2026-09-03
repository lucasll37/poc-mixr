#include "app/Fleet.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"

#include <cstdlib>
#include <iostream>

namespace app {

mixr::models::AirVehicle* findAircraft(mixr::models::WorldModel* const wm, const std::string& name)
{
   mixr::base::PairStream* players{wm->getPlayers()};
   if (players == nullptr) return nullptr;

   mixr::models::AirVehicle* result{};
   mixr::base::Pair* const p{players->findByName(name.c_str())};
   if (p != nullptr) result = dynamic_cast<mixr::models::AirVehicle*>(p->object());
   players->unref();
   return result;
}

Fleet collectFleet(mixr::models::WorldModel* const wm, const std::vector<std::string>& names)
{
   Fleet fleet;
   fleet.reserve(names.size());

   for (const std::string& name : names) {
      mixr::models::AirVehicle* const air{findAircraft(wm, name)};
      if (air == nullptr) {
         std::cerr << "player '" << name << "' nao encontrado!" << std::endl;
         std::exit(EXIT_FAILURE);
      }
      fleet.push_back(air);
   }
   return fleet;
}

void applyCruiseThrottle(const Fleet& fleet, const double throttle)
{
   for (const auto air : fleet) {
      if (air != nullptr) air->setThrottles(&throttle, 1);
   }
}

} // namespace app
