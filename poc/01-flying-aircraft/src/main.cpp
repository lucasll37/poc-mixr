//
// poc/01-flying-aircraft
//
// Subprojeto minimo e funcional: uma Station com um unico Aircraft
// (RacModel dynamics) que recebe comandos de heading/altitude/velocidade
// diferentes do seu estado inicial. O loop de simulacao roda por uma
// duracao fixa, imprimindo telemetria a cada segundo simulado para
// demonstrar que a aeronave de fato manobra (voa), e encerra sozinho.
//

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/util/system_utils.hpp"

// factories
#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/interop/dis/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/base/factory.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>

// background frame rate
const int bgRate{10};

// how long the demo runs (simulated seconds)
const double simDuration{30.0};

mixr::base::Object* factory(const std::string& name)
{
   mixr::base::Object* obj{mixr::simulation::factory(name)};

   if (obj == nullptr) obj = mixr::models::factory(name);
   if (obj == nullptr) obj = mixr::terrain::factory(name);
   if (obj == nullptr) obj = mixr::dis::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);
   return obj;
}

// station builder
mixr::simulation::Station* builder(const std::string& filename)
{
   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, factory, &num_errors)};
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

void printTelemetry(const double simTime, mixr::models::Player* ac)
{
   std::cout << std::fixed << std::setprecision(1)
             << "t=" << std::setw(5) << simTime << "s"
             << "  alt=" << std::setw(7) << ac->getAltitudeFt() << " ft"
             << "  hdg=" << std::setw(5) << ac->getHeadingD() << " deg"
             << "  spd=" << std::setw(5) << ac->getTotalVelocityKts() << " kts"
             << "  lat=" << std::setprecision(4) << ac->getLatitude()
             << "  lon=" << ac->getLongitude()
             << std::setprecision(1)
             << std::endl;
}

int main(int argc, char* argv[])
{
   std::string configFilename = "./poc/01-flying-aircraft/configs/scenario.epp";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") {
         configFilename = argv[++i];
      }
   }

   mixr::simulation::Station* station{builder(configFilename)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto ownship = dynamic_cast<mixr::models::Player*>(station->getOwnship());
   if (ownship == nullptr) {
      std::cerr << "No ownship player found in scenario!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};

   double simTime{};
   const double startTime{mixr::base::getComputerTime()};

   std::cout << "=== poc/01-flying-aircraft ===" << std::endl;
   printTelemetry(simTime, ownship);

   int ticksPerSecond{};
   while (simTime < simDuration) {

      station->updateData(static_cast<double>(dt));

      simTime += dt;
      const double timeNow{mixr::base::getComputerTime()};
      const double elapsedTime{timeNow - startTime};
      const double nextFrameStart{simTime - elapsedTime};
      const int sleepTime{static_cast<int>(nextFrameStart * 1000.0)};
      if (sleepTime > 0)
         mixr::base::msleep(sleepTime);

      ticksPerSecond += 1;
      if (ticksPerSecond == bgRate) {
         printTelemetry(simTime, ownship);
         ticksPerSecond = 0;
      }
   }

   std::cout << "=== fim ===" << std::endl;

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
