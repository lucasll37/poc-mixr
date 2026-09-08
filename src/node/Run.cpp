#include "Run.hpp"

#include "xclock/ClockStation.hpp"
#include "xlog/Log.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/base/Component.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <memory>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

// Signal-safe de proposito: so um store atomico, nada de LOG()/alocacao
// aqui dentro (mesmo padrao ja documentado para o extinto RealTimeRun.cpp
// deste repositorio -- "o handler de SIGINT so marca uma flag").
void onSignal(int)
{
   g_running.store(false);
}

// Cala a thread T/C nativa ANTES de qualquer SHUTDOWN_EVENT. O framework
// nao tem API para parar essa thread -- o laco dela so testa
// getParent()->isShutdown(), que so vira true DEPOIS que
// Station::shutdownNotification() ja derrubou a Simulation (e as threads do
// pool com ela); na janela entre as duas coisas ela pode comecar um frame
// novo contra workers ja mortos e se autotravar em
// SyncThread::waitForAllCompleted(). E tambem o que impede a fila do
// gravador de crescer sem teto depois que a produtora devia ter parado.
//
// Com ( ClockStation ), ha prova POSITIVA de ociosidade
// (requestTcStop()/waitForTcQuiesced()); sem ela (cenario com uma
// ( Station ) pura), so um fallback nativo sem prova: fastForwardRate=0 faz
// o laco nativo de tcFrame() parar de rodar, e so resta esperar um pouco.
bool quiesceTimeCritical(mixr::simulation::Station* const station,
                          mixr::xclock::ClockStation* const clockStation,
                          const double timeoutSec = 2.0)
{
   if (station == nullptr || !station->doWeHaveTheTcThread()) return true;

   if (clockStation != nullptr) {
      // Volta para 1x antes de pedir a parada -- a uma escala maior, um
      // unico processTimeCriticalTasks() ainda em curso faria varios
      // tcFrame() de uma vez, e esperar por isso estouraria o timeout
      // abaixo por puro volume de trabalho, nao por travamento.
      clockStation->setTimeScale(1.0);

      clockStation->requestTcStop();
      if (clockStation->waitForTcQuiesced(timeoutSec)) return true;

      LOG(WARNING) << "encerramento: thread de tempo critico nao confirmou "
                      "ociosidade em " << timeoutSec << "s -- seguindo mesmo assim";
      return false;
   }

   station->setFastForwardRate(0);
   const double tcRate{station->getTimeCriticalRate()};
   const double periodMs{(tcRate > 0.0) ? (1000.0 / tcRate) : 20.0};
   mixr::base::msleep(static_cast<unsigned int>(periodMs * 5.0) + 1);
   return false;
}

}

namespace node {

int run(mixr::simulation::Station* const station, mixr::xclock::ClockStation* const clockStation)
{
   std::signal(SIGINT, onSignal);
   std::signal(SIGTERM, onSignal);

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000); // assentamento -- mesmo valor do laco de fundo do ./app

   const double dt{0.1}; // 10 Hz, a mesma taxa de fundo usada em toda a base
   double wallTimeElapsed{};
   const double startTime{mixr::base::getComputerTime()};

   while (g_running.load()) {
      station->updateData(dt);

      wallTimeElapsed += dt;
      const double elapsed{mixr::base::getComputerTime() - startTime};
      const int sleepMs{static_cast<int>((wallTimeElapsed - elapsed) * 1000.0)};
      if (sleepMs > 0) mixr::base::msleep(static_cast<unsigned int>(sleepMs));
   }

   quiesceTimeCritical(station, clockStation);
   station->updateData(dt); // drenagem final, produtora ja calada

   return 0;
}

void shutdownStation(mixr::simulation::Station* const station, const double watchdogSec)
{
   if (station == nullptr) return;

   // Watchdog: plano B se o teardown nativo travar (bug documentado --
   // spinlock nativo sem yield + fila do gravador sem teto). std::fputs em
   // stderr, nunca LOG(), de proposito: o mutex global do log pode ser
   // justamente o que esta preso.
   auto terminou = std::make_shared<std::atomic<bool>>(false);
   std::thread watchdog([terminou, watchdogSec] {
      const int stepMs{50};
      const int tries{static_cast<int>((watchdogSec * 1000.0) / stepMs) + 1};
      for (int i = 0; i < tries; i++) {
         if (terminou->load()) return;
         mixr::base::msleep(stepMs);
      }
      if (terminou->load()) return;
      std::fputs("[node] encerramento travou -- saindo a forca (_Exit)\n", stderr);
      std::_Exit(0);
   });
   watchdog.detach();

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();

   terminou->store(true);
}

}
