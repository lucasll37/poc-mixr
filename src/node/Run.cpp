#include "Run.hpp"

#include "xclock/ClockStation.hpp"
#include "xlog/Log.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/base/Component.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <algorithm>
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

// Teto da espera pela saida da thread T/C ao soltar a referencia da Station
// (ver o comentario grande de shutdownStation() abaixo). Ela acorda a cada
// 1/tcRate (20 ms a 50 Hz), entao isto e folga largissima para maquina
// carregada -- nao uma expectativa. Fica ABAIXO do watchdog de proposito: o
// caminho de degradacao correto e o LOG(WARNING) aqui, nao o _Exit() do
// watchdog.
const double kTimeoutTcSaidaSec{3.0};

// Espera o contador de referencias da Station cair ate 'alvo'. 'true' se
// caiu, 'false' se o teto venceu (nunca trava o chamador). Mesma logica de
// app/src/app/Shutdown.cpp -- duplicada aqui de proposito (node nao
// reaproveita NADA de app/).
bool esperaRefCount(const mixr::simulation::Station* const station, const int alvo,
                    const double timeoutSec)
{
   const int stepMs{2};
   const int tries{std::max(1, static_cast<int>((timeoutSec * 1000.0) / stepMs))};
   for (int i = 0; i < tries; i++) {
      if (station->getRefCount() <= alvo) return true;
      mixr::base::msleep(stepMs);
   }
   return station->getRefCount() <= alvo;
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

   // A THREAD T/C NATIVA E DONA DE UMA REFERENCIA DA STATION -- ACHADO POR
   // AUDITORIA (revisao completa do repositorio): esta funcao fazia so
   // 'station->event(SHUTDOWN_EVENT); station->unref();', sem a correcao que
   // app/src/app/Shutdown.cpp ja tem desde 2026-09-10 (esta funcao foi criada
   // antes, copiando o padrao velho, e nunca foi atualizada -- CLAUDE.md
   // chegou a afirmar que as duas eram "a MESMA logica", o que nao era mais
   // verdade so para shutdownStation()).
   //
   // ARMADILHA MEDIDA (no ./app, mesma causa vale aqui -- node sempre cria a
   // thread T/C, nunca tem '-deterministic'): Station::
   // createTimeCriticalProcess() cria 'new StationTcPeriodicThread(this,
   // ...)' passando a PROPRIA Station como parent, e a funcao de partida da
   // thread faz 'parent->ref()'. Logo a Station chega aqui com refCount 2, e
   // 'station->unref()' sozinho so leva 2 -> 1 e retorna em microssegundos.
   // Quem de fato executa ~Station e a thread T/C, ate um periodo T/C depois
   // (20 ms a 50 Hz), quando o laco 'while (!getParent()->isShutdown())' enfim
   // ve a marcacao do SHUTDOWN_EVENT, sai, e chama 'parent->unref()'.
   //
   // Isso poe a destruicao do grafo INTEIRO (players, JSBSimModel,
   // DataRecorder/TacviewOutput fechando o .acmi, e os objetos do PLUGIN) em
   // paralelo com o que a main faz em seguida: o 'return 0' de main() e os
   // destrutores estaticos de todo .so carregado -- mixr::xlog, o 'static
   // Ort::Env' de libs/xinfer, o _IO_cleanup() da glibc liberando os FILE*
   // enquanto o gravador ainda escreve.
   //
   // A saida e tomar uma referencia EXTRA aqui: com ela, o unref() da thread
   // T/C nunca pode chegar a zero, e a destruicao acontece
   // deterministicamente NESTA thread, abaixo, com a thread T/C ja fora do
   // laco.
   const bool haviaTcThread{station->doWeHaveTheTcThread()};
   if (haviaTcThread) station->ref();

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

   // Marca isShutdown() -- o que faz o laco da thread T/C terminar -- e ja
   // drena o gravador e fecha o .acmi. Todo o trabalho OBSERVAVEL do
   // encerramento acontece aqui; o unref() abaixo e so liberacao de memoria.
   station->event(mixr::base::Component::SHUTDOWN_EVENT);

   if (haviaTcThread) {
      // PROVA DIRETA de que a thread T/C soltou a referencia dela, em vez de
      // um sleep esperancoso: esperamos exatamente UMA liberacao a partir da
      // contagem de agora (a nossa extra + a da aplicacao + a dela).
      const int alvo{station->getRefCount() - 1};
      const bool saiu{esperaRefCount(station, alvo, kTimeoutTcSaidaSec)};

      if (!saiu) {
         // NAO destruir e estritamente mais seguro do que destruir sob
         // corrida: o processo esta encerrando e o SHUTDOWN_EVENT acima ja
         // fez todo o trabalho observavel. Deixamos a Station viva de
         // proposito (as duas referencias ficam) e saimos.
         LOG(WARNING) << "encerramento: a thread de tempo critico nao soltou a "
                         "referencia da Station em " << kTimeoutTcSaidaSec
                      << "s -- nao destruindo o grafo para nao correr com o exit()";
         terminou->store(true);
         return;
      }
      station->unref();   // solta a EXTRA; sobra so a da aplicacao
   }

   station->unref();      // ultima referencia -> ~Station aqui, na thread main

   terminou->store(true);
}

}
