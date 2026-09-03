#include "StationBuilder.hpp"

#include "mixr_factory.hpp"

#include "xplugin/PluginRegistry.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/edl_parser.hpp"

#include <stdexcept>

namespace rl {

mixr::simulation::Station* buildStation(const std::string& filename)
{
   // Mesma ordem de app::buildStation(): o registro de plugins precisa
   // conhecer a factory SEM plugin antes do parse, para recusar na carga um
   // plugin cujo nome colida com o framework.
   mixr::xplugin::setBuiltinFactory(mixrFactoryBuiltin);

   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, mixrFactory, &num_errors)};
   mixr::xplugin::seal();

   if (num_errors > 0) {
      throw std::runtime_error("File: " + filename + ", number of errors: "
                               + std::to_string(num_errors));
   }
   if (obj == nullptr) {
      throw std::runtime_error("Invalid configuration file, no objects defined!");
   }

   const auto pair = dynamic_cast<mixr::base::Pair*>(obj);
   if (pair != nullptr) {
      obj = pair->object();
      obj->ref();
      pair->unref();
   }

   const auto station = dynamic_cast<mixr::simulation::Station*>(obj);
   if (station == nullptr) {
      throw std::runtime_error("Invalid configuration file!");
   }
   return station;
}

void primeStation(mixr::simulation::Station* const station)
{
   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(1.0 / static_cast<double>(station->getTimeCriticalRate()));
}

mixr::models::WorldModel* worldModelOf(mixr::simulation::Station* const station)
{
   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
   if (worldModel == nullptr) {
      throw std::runtime_error("No WorldModel found!");
   }
   return worldModel;
}

} // namespace rl
