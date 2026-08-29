#include "app/RealTimeRun.hpp"

#include "app/StatusReport.hpp"

#include "xnative/RadarScan.hpp"

#include "xclock/ClockStation.hpp"
#include "xclock/TimeControls.hpp"

#include "xtacview/TacviewOutput.hpp"

#include "mixr/linkage/IoHandler.hpp"

#include "mixr/models/player/air/AirVehicle.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"

#include "mixr/base/util/system_utils.hpp"

#include <csignal>
#include <iostream>

namespace app {

namespace {

const int bgRate{10};
const int statusEveryNFrames{20};
const int settleMs{1000};

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

} // namespace

void runRealTime(mixr::simulation::Station* const station, const Fleet& fleet,
                 mixr::models::WorldModel* const worldModel,
                 mixr::xclock::ClockStation* const clockStation,
                 mixr::linkage::IoHandler* const ioHandler,
                 mixr::xtacview::TacviewOutput* const tacviewOutput)
{
   std::signal(SIGINT, onSigint);

   // Poe o terminal em modo raw; o destrutor devolve como estava.
   mixr::xclock::TimeControls timeControls{clockStation};
   if (timeControls.isAvailable()) {
      std::cout << mixr::xclock::TimeControls::helpText();
   } else {
      std::cout << "Sem terminal interativo (pipe/redirecionamento):"
                << " controle de tempo por teclado desligado." << std::endl;
   }

   station->createTimeCriticalProcess();
   mixr::base::msleep(settleMs);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double wallTimeElapsed{};
   const double startTime{mixr::base::getComputerTime()};
   long frameCount{};

   while (!g_stopRequested) {

      // Um read() nao bloqueante por frame -- se nao houver tecla, custa nada.
      timeControls.poll();

      // Le o joystick (se houver um configurado) e aplica no bandit1.
      if (ioHandler != nullptr) ioHandler->inputDevices(dt);

      // Drena o gravador (Tacview) -- e, se o agente do cenario for um
      // SimAgent (componente da Station), e tambem aqui que ele decide.
      // Ver o cabecalho do .hpp.
      station->updateData(dt);

      // Varredura de radar de cada player, direto pro Tacview -- FORA da
      // fila do gravador (ver TacviewOutput::updateRadarScan()). So depois
      // do updateData() acima, que ja declarou o objeto (T=) no Tacview.
      if (tacviewOutput != nullptr) {
         const double simTime{worldModel->getExecTimeSec()};
         for (auto* const air : fleet) {
            const auto scan = mixr::xnative::radarScanOf(air);
            if (scan.found) {
               tacviewOutput->updateRadarScan(static_cast<std::uint32_t>(air->getID()), simTime,
                  scan.azimuthDeg, scan.elevationDeg, scan.rangeM,
                  scan.horizontalBeamwidthDeg, scan.verticalBeamwidthDeg);
            }
         }
      }

      frameCount += 1;
      if (frameCount % statusEveryNFrames == 0) {
         printStatus(fleet, static_cast<double>(frameCount) * dt,
                     worldModel->getExecTimeSec(), timeControls.describe());
      }

      wallTimeElapsed += dt;
      const double elapsedTime{mixr::base::getComputerTime() - startTime};
      const int sleepTime{static_cast<int>((wallTimeElapsed - elapsedTime) * 1000.0)};
      if (sleepTime > 0) mixr::base::msleep(sleepTime);
   }
}

} // namespace app
