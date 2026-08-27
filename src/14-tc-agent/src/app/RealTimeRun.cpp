#include "app/RealTimeRun.hpp"

#include "app/StatusReport.hpp"

#include "xclock/ClockStation.hpp"
#include "xclock/TimeControls.hpp"

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
                 mixr::xclock::ClockStation* const clockStation)
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

      // Drena o gravador (Tacview) -- e, se o agente do cenario for um
      // SimAgent (componente da Station), e tambem aqui que ele decide.
      // Ver o cabecalho do .hpp.
      station->updateData(dt);

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
