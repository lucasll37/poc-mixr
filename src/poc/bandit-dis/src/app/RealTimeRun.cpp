#include "app/RealTimeRun.hpp"

#include "mixr/linkage/IoHandler.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/Simulation.hpp"

#include "mixr/models/player/air/AirVehicle.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace app {

namespace {

const int bgRate{10};
const int statusEveryNFrames{20};
const int settleMs{1000};

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

// Achado fresco a cada chamada -- o bandit1 pode nao existir ainda no
// primeiro instante apos o parse, e nao ha necessidade de guardar o
// ponteiro entre frames so para uma linha de status a cada 2s.
void printBanditStatus(mixr::simulation::Station* const station, const double elapsedSec)
{
   mixr::simulation::Simulation* const sim{station->getSimulation()};
   if (sim == nullptr) return;

   mixr::base::PairStream* const players{sim->getPlayers()};
   if (players == nullptr) return;

   const mixr::base::Pair* const p{players->findByName("bandit1")};
   const auto air = (p != nullptr) ? dynamic_cast<const mixr::models::AirVehicle*>(p->object()) : nullptr;

   if (air != nullptr) {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(0);
      oss << "[t=" << elapsedSec << "s] bandit1"
          << " alt=" << air->getAltitudeM() << "m"
          << " agl=" << air->getAltitudeAglM() << "m"
          << " hdg=" << air->getHeadingD() << "deg"
          << " roll=" << air->getRollD() << "deg"
          << " spd=" << air->getTotalVelocityKts() << "kt";
      std::cout << oss.str() << std::endl;
   }

   players->unref();
}

} // namespace

void runRealTime(mixr::simulation::Station* const station, mixr::linkage::IoHandler* const ioHandler)
{
   std::signal(SIGINT, onSigint);

   station->createTimeCriticalProcess();
   mixr::base::msleep(settleMs);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double wallTimeElapsed{};
   const double startTime{mixr::base::getComputerTime()};
   long frameCount{};

   while (!g_stopRequested) {

      // Le o joystick (se houver um configurado) e aplica no bandit1 --
      // com fallback pro Autopilot se nao houver hardware conectado.
      if (ioHandler != nullptr) ioHandler->inputDevices(dt);

      // Drena o gravador (Tacview) e processa 'networks:' (envia os PDUs
      // DIS do bandit1) -- as duas coisas acontecem dentro de updateData().
      station->updateData(dt);

      frameCount += 1;
      if (frameCount % statusEveryNFrames == 0) {
         printBanditStatus(station, static_cast<double>(frameCount) * dt);
      }

      wallTimeElapsed += dt;
      const double elapsedTime{mixr::base::getComputerTime() - startTime};
      const int sleepTime{static_cast<int>((wallTimeElapsed - elapsedTime) * 1000.0)};
      if (sleepTime > 0) mixr::base::msleep(sleepTime);
   }
}

} // namespace app
