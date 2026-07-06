//
// poc/03-bt-autopilot
//
// Subprojeto mais elaborado: usa MIXR (Aircraft + RacModel) E
// BehaviorTree.CPP juntos -- a arvore de comportamento decide, a cada
// tick, o que comandar no dynamics model da aeronave, com base em regras
// de negocio isoladas em domain::Mission (patrulha ciclica entre
// waypoints + retorno a base por combustivel baixo).
//
// Separacao de responsabilidades (ver README/CLAUDE.md):
//   include/domain, src/domain   -- regras de negocio puras (sem MIXR/BT)
//   include/bt, src/bt           -- nos da arvore (adaptadores finos) e a
//                                   factory de registro do BehaviorTreeFactory
//   mixr_factory.hpp/.cpp        -- factory de objetos MIXR (Station/Aircraft/...)
//   main.cpp                     -- so orquestra: constroi Station + Tree,
//                                   injeta Mission/DynamicsModel no blackboard
//                                   e roda o loop
//

#include "mixr_factory.hpp"
#include "bt/bt_factory.hpp"
#include "domain/Mission.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/dynamics/DynamicsModel.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/util/system_utils.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <cstdlib>

namespace {

const int bgRate{10};
const double simDuration{50.0};

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

void printTelemetry(const double simTime, const domain::Mission& mission, mixr::models::Player* ac)
{
   std::cout << std::fixed << std::setprecision(1)
             << "t=" << std::setw(5) << simTime << "s"
             << "  " << (mission.isInRtb() ? "RTB    " : "wp=" + std::to_string(mission.currentWaypointIndex()) + "   ")
             << "  fuel=" << std::setw(5) << mission.getFuel()
             << "  alt=" << std::setw(7) << ac->getAltitudeFt() << " ft"
             << "  hdg=" << std::setw(5) << ac->getHeadingD() << " deg"
             << "  spd=" << std::setw(5) << ac->getTotalVelocityKts() << " kts"
             << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
   std::string configFilename = "./poc/03-bt-autopilot/configs/scenario.epp";
   std::string treeFilename = "./poc/03-bt-autopilot/configs/mission.xml";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") configFilename = argv[++i];
      else if (std::string(argv[i]) == "-t") treeFilename = argv[++i];
   }

   // --- MIXR: Station + Aircraft ---
   mixr::simulation::Station* station{buildStation(configFilename)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto ownship = dynamic_cast<mixr::models::Player*>(station->getOwnship());
   if (ownship == nullptr) {
      std::cerr << "No ownship player found in scenario!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const auto dynamicsModel = ownship->getDynamicsModel();
   if (dynamicsModel == nullptr) {
      std::cerr << "Ownship has no dynamics model!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   // --- Dominio + BehaviorTree ---
   domain::Mission mission;

   BT::BehaviorTreeFactory btFactory;
   bt_nodes::registerNodes(btFactory);

   auto blackboard = BT::Blackboard::create();
   blackboard->set<domain::Mission*>("mission", &mission);
   blackboard->set<mixr::models::DynamicsModel*>("dynamicsModel", dynamicsModel);

   BT::Tree tree = btFactory.createTreeFromFile(treeFilename, blackboard);

   // --- Loop de simulacao ---
   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double simTime{};
   const double startTime{mixr::base::getComputerTime()};

   std::cout << "=== poc/03-bt-autopilot ===" << std::endl;
   printTelemetry(simTime, mission, ownship);

   int ticksPerSecond{};
   while (simTime < simDuration) {

      station->updateData(static_cast<double>(dt));
      mission.tick(dt);
      tree.tickRoot();

      simTime += dt;
      const double timeNow{mixr::base::getComputerTime()};
      const double elapsedTime{timeNow - startTime};
      const double nextFrameStart{simTime - elapsedTime};
      const int sleepTime{static_cast<int>(nextFrameStart * 1000.0)};
      if (sleepTime > 0)
         mixr::base::msleep(sleepTime);

      ticksPerSecond += 1;
      if (ticksPerSecond == bgRate) {
         printTelemetry(simTime, mission, ownship);
         ticksPerSecond = 0;
      }
   }

   std::cout << "=== fim ===" << std::endl;

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
