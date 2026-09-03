#include "app/StationBuilder.hpp"

#include "mixr_factory.hpp"

#include "xplugin/PluginRegistry.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/edl_parser.hpp"

#include <cstdlib>
#include <iostream>

namespace app {

mixr::simulation::Station* buildStation(const std::string& filename)
{
   // Ver o comentario identico em
   // src/poc/single-thread/src/app/StationBuilder.cpp: o registro de
   // plugins precisa saber o que a aplicacao ja constroi sem plugin nenhum,
   // para recusar na CARGA um plugin cujo nome de fabrica colida.
   mixr::xplugin::setBuiltinFactory(mixrFactoryBuiltin);

   int num_errors{};

   // A carga do plugin acontece AQUI DENTRO, durante o parse -- ver o
   // comentario identico em StationBuilder.cpp do single-thread para a
   // prova de ordem (o bloco 'plugins:' tem de vir antes de qualquer uso).
   //
   // Mensagens de erro do parser vao para stderr (nunca stdout) -- e assim
   // que o server (Subprocess.cpp) distingue "a ultima linha da stdout e o
   // JSON de telemetria" de "isto e um erro de build de cenario".
   mixr::base::Object* obj{mixr::base::edl_parser(filename, mixrFactory, &num_errors)};

   mixr::xplugin::seal();

   if (num_errors > 0) {
      std::cerr << "File: " << filename << ", number of errors: " << num_errors << std::endl;
      std::exit(EXIT_FAILURE);
   }
   if (obj == nullptr) {
      std::cerr << "Invalid configuration file, no objects defined!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const auto pair = dynamic_cast<mixr::base::Pair*>(obj);
   if (pair != nullptr) {
      obj = pair->object();
      obj->ref();
      pair->unref();
   }

   const auto station = dynamic_cast<mixr::simulation::Station*>(obj);
   if (station == nullptr) {
      std::cerr << "Invalid configuration file!" << std::endl;
      std::exit(EXIT_FAILURE);
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
      std::cerr << "No WorldModel found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   return worldModel;
}

} // namespace app
