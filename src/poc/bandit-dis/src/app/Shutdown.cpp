#include "app/Shutdown.hpp"

#include "xlog/Log.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/util/system_utils.hpp"
#include "mixr/simulation/Station.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

namespace app {

bool quiesceTimeCritical(mixr::simulation::Station* const station)
{
   if (station == nullptr) return true;

   // O modo '-deterministic' nunca chama createTimeCriticalProcess(): ele roda
   // tcFrame() direto na propria thread do laco. Nada a calar. (Esta poc nao
   // tem CLI de '-deterministic', mas a guarda vale igual se algum dia ganhar.)
   if (!station->doWeHaveTheTcThread()) return true;

   // Sem ClockStation (cenario com '( Station )' pura): fallback 100%
   // nativo. Com fastForwardRate zero, o laco
   // 'for (jj=0; jj < getFastForwardRate(); jj++) tcFrame(dt)' da Station
   // roda ZERO vezes -- nenhum frame novo comeca.
   station->setFastForwardRate(0);

   const double tcRate{station->getTimeCriticalRate()};
   const double periodoMs{(tcRate > 0.0) ? (1000.0 / tcRate) : 20.0};
   mixr::base::msleep(static_cast<unsigned int>(periodoMs * 5.0) + 1);
   return false;
}

void shutdownStation(mixr::simulation::Station* const station, const double watchdogSec)
{
   if (station == nullptr) return;

   // WATCHDOG. O teardown do MIXR nao e cancelavel e nao e observavel: se algo
   // dentro dele bloquear (um destino de saida lento, um base::lock() cujo
   // dono nao progride), a main fica parada e o usuario nao recupera o shell.
   // Um _Exit() como PLANO B mantem o caminho limpo -- que e o que roda em
   // 100% dos casos normais, e e ele que fecha o .acmi e o log -- sem deixar o
   // caso ruim virar um processo pendurado.
   //
   // 'shared_ptr' e nao captura por referencia: a thread e destacada e pode
   // acordar depois de esta funcao retornar e o quadro dela sumir.
   auto terminou = std::make_shared<std::atomic<bool>>(false);
   std::thread watchdog([terminou, watchdogSec] {
      const int passoMs{50};
      const int tentativas{static_cast<int>((watchdogSec * 1000.0) / passoMs) + 1};
      for (int i = 0; i < tentativas; i++) {
         if (terminou->load()) return;
         mixr::base::msleep(passoMs);
      }
      if (terminou->load()) return;
      // Sem LOG() aqui de proposito: o log toma um mutex global que pode ser
      // justamente o que esta preso. std::fputs em stderr e imediato.
      std::fputs("[bandit-dis] encerramento travou -- saindo a forca (_Exit)\n", stderr);
      std::_Exit(0);
   });
   watchdog.detach();

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();

   terminou->store(true);
}

}
