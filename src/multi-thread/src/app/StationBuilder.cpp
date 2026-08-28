#include "app/StationBuilder.hpp"

#include "mixr_factory.hpp"

#include "xclock/ClockStation.hpp"

#include "mixr/linkage/IoHandler.hpp"

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
   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, mixrFactory, &num_errors)};
   if (num_errors > 0) {
      std::cerr << "File: " << filename << ", number of errors: " << num_errors << std::endl;
      std::exit(EXIT_FAILURE);
   }
   if (obj == nullptr) {
      std::cerr << "Invalid configuration file, no objects defined!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   // O topo do arquivo e um Pair (nome: objeto); quem interessa e o objeto.
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

mixr::xclock::ClockStation* clockStationOf(mixr::simulation::Station* const station)
{
   const auto clockStation = dynamic_cast<mixr::xclock::ClockStation*>(station);
   if (clockStation == nullptr) {
      std::cerr << "[main] aviso: a Station do cenario nao e uma ( ClockStation );"
                << " controle de tempo desligado" << std::endl;
   }
   return clockStation;
}

mixr::linkage::IoHandler* ioHandlerOf(mixr::simulation::Station* const station)
{
   const auto ioHandler = dynamic_cast<mixr::linkage::IoHandler*>(station->getIoHandler());
   if (ioHandler == nullptr) {
      std::cerr << "[main] aviso: cenario sem ( JoystickIoHandler ) no slot 'ioHandler:';"
                << " controle por joystick desligado" << std::endl;
   }
   return ioHandler;
}

} // namespace app
