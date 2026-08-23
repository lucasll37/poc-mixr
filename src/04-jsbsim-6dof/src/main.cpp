//
// poc/04-jsbsim-6dof
//
// Subprojeto com aerodinamica 6-DOF completa (JSBSimModel, aeronave F4N).
//
// A telemetria para o Tacview NAO e montada aqui: quem exporta e o
// TacviewOutput (shared/xtacview), um mixr::recorder::OutputHandler ligado
// na cadeia do 'dataRecorder' da Station -- ver configs/scenario.epp. O
// framework empurra cada REID_* para ele; este main.cpp so pilota a
// aeronave e roda o laco.
//
// F4N nao tem um sistema de autopilot proprio no JSBSim (ap/heading_hold
// etc. -- ver systems/ do aircraft), entao os comandos de
// DynamicsModel::setCommandedHeadingD/Altitude/VelocityKts (que dependiam
// disso no RacModel dos pocs anteriores) nao teriam efeito aqui. Em vez
// disso, este main.cpp aplica diretamente entradas de controle
// (stick/throttle), para demonstrar a resposta real do modelo 6-DOF.
//

#include "mixr_factory.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/dynamics/DynamicsModel.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <algorithm>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

namespace {

const int bgRate{10};

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

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

// Perfil de controle simples e conservador: neutro -> rolamento -> nivela
// -> cabrar levemente -> depois disso, nivela as asas e o pitch sozinho
// (proporcional simples) para nao divergir numa sessao longa de Tacview.
// So para provar que a resposta 6-DOF do JSBSim reage de forma coerente
// aos comandos (nao e um autopilot real).
void applyControlProfile(const double t, mixr::models::Player* ac, mixr::models::DynamicsModel* dm)
{
   double roll{};
   double pitch{};

   if (t >= 15.0 && t < 18.0) {
      roll = 0.03;                                 // inicia uma curva suave
   } else if (t >= 28.0 && t < 31.0) {
      pitch = 0.03;                                // cabra levemente (subida)
   } else if (t >= 31.0) {
      // autonivelamento simples: comanda de volta na direcao contraria ao
      // angulo atual, proporcional ao erro (para nao ficar divergindo em
      // bank/pitch indefinidamente numa sessao longa)
      const double kP{0.01};
      const double maxInput{0.2};
      roll = std::max(-maxInput, std::min(maxInput, -kP * ac->getRollD()));
      pitch = std::max(-maxInput, std::min(maxInput, -kP * ac->getPitchD()));
   }

   dm->setControlStick(roll, pitch);
}

void printTelemetry(const double simTime, mixr::models::Player* ac)
{
   std::cout << std::fixed << std::setprecision(1)
             << "t=" << std::setw(5) << simTime << "s"
             << "  alt=" << std::setw(7) << ac->getAltitudeFt() << " ft"
             << "  hdg=" << std::setw(5) << ac->getHeadingD() << " deg"
             << "  roll=" << std::setw(5) << ac->getRollD() << " deg"
             << "  pitch=" << std::setw(5) << ac->getPitchD() << " deg"
             << "  spd=" << std::setw(5) << ac->getTotalVelocityKts() << " kts"
             << std::endl;
}

}

int main(int argc, char* argv[])
{
   std::string configFilename = "./poc/04-jsbsim-6dof/configs/scenario.epp";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") configFilename = argv[++i];
   }

   std::signal(SIGINT, onSigint);

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

   const int numEngines{dynamicsModel->getNumberOfEngines()};
   const std::vector<double> milPower(static_cast<std::size_t>(numEngines), 1.0);

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double simTime{};
   const double startTime{mixr::base::getComputerTime()};

   std::cout << "=== poc/04-jsbsim-6dof ===" << std::endl;
   std::cout << "(Ctrl+C para encerrar; ate la, roda indefinidamente)" << std::endl;
   printTelemetry(simTime, ownship);

   int ticksPerSecond{};
   while (!g_stopRequested) {

      // updateData() drena a fila do recorder para a cadeia de
      // OutputHandlers -- e assim que o TacviewOutput recebe os dados.
      station->updateData(static_cast<double>(dt));

      dynamicsModel->setThrottles(milPower.data(), numEngines);
      applyControlProfile(simTime, ownship, dynamicsModel);

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
