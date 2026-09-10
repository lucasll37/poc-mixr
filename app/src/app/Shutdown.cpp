#include "app/Shutdown.hpp"

#include "xclock/ClockStation.hpp"
#include "xlog/Log.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/util/system_utils.hpp"
#include "mixr/simulation/Station.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

namespace app {

namespace {

// Teto da espera pela saida da thread T/C. Ela acorda a cada 1/tcRate (20 ms a
// 50 Hz), entao isto e folga larguissima para maquina carregada -- nao uma
// expectativa. Fica ABAIXO do watchdog de shutdownStation() de proposito: o
// caminho de degradacao correto e o LOG(WARNING) daqui, nao o _Exit() de la.
const double kTimeoutTcSaidaSec{3.0};

// Espera o contador de referencias da Station cair ate 'alvo'. 'true' se caiu,
// 'false' se o teto venceu (nunca trava o chamador).
bool esperaRefCount(const mixr::simulation::Station* const station, const int alvo,
                    const double timeoutSec)
{
   const int passoMs{2};
   const int tentativas{std::max(1, static_cast<int>((timeoutSec * 1000.0) / passoMs))};
   for (int i = 0; i < tentativas; i++) {
      if (station->getRefCount() <= alvo) return true;
      mixr::base::msleep(passoMs);
   }
   return station->getRefCount() <= alvo;
}

}

bool quiesceTimeCritical(mixr::simulation::Station* const station,
                         mixr::xclock::ClockStation* const clockStation,
                         const double timeoutSec)
{
   if (station == nullptr) return true;

   // O modo '-deterministic' nunca chama createTimeCriticalProcess(): ele roda
   // tcFrame() direto na propria thread do laco. Nada a calar.
   if (!station->doWeHaveTheTcThread()) return true;

   if (clockStation != nullptr) {
      // Volta para 1x ANTES de pedir a parada. A 64x um unico
      // processTimeCriticalTasks() faz 64 tcFrame() (o laco nativo de
      // Station.cpp:506-511), e esperar por isso estouraria o teto abaixo --
      // nao por deadlock, so por volume de trabalho ja em curso.
      clockStation->setTimeScale(1.0);

      clockStation->requestTcStop();
      if (clockStation->waitForTcQuiesced(timeoutSec)) return true;

      LOG(WARNING) << "encerramento: a thread de tempo critico nao confirmou "
                      "ociosidade em " << timeoutSec << "s -- seguindo mesmo assim";
      return false;
   }

   // Fallback sem ClockStation, 100% nativo: com fastForwardRate zero, o laco
   // 'for (jj=0; jj < getFastForwardRate(); jj++) tcFrame(dt)' da Station roda
   // ZERO vezes, entao nenhum frame novo comeca. Nao ha prova positiva aqui --
   // a Station nativa nao tem por onde nos avisar --, so a espera de alguns
   // periodos T/C, que ja e melhor que o nada de antes.
   station->setFastForwardRate(0);

   const double tcRate{station->getTimeCriticalRate()};
   const double periodoMs{(tcRate > 0.0) ? (1000.0 / tcRate) : 20.0};
   mixr::base::msleep(static_cast<unsigned int>(periodoMs * 5.0) + 1);
   return false;
}

void shutdownStation(mixr::simulation::Station* const station, const double watchdogSec)
{
   if (station == nullptr) return;

   // A THREAD T/C NATIVA E DONA DE UMA REFERENCIA DA STATION -- e e por isso
   // que o unref() la embaixo, sozinho, nao destroi nada.
   //
   // ARMADILHA MEDIDA, NAO REDESCOBRIR. Station::createTimeCriticalProcess()
   // cria 'new StationTcPeriodicThread(this, ...)' passando a PROPRIA Station
   // como parent (Station.cpp:446), e a funcao de partida da thread faz
   // 'parent->ref()' (AbstractThread_linux.cpp:28). Logo a Station chega aqui
   // com refCount 2, e 'station->unref()' so leva 2 -> 1 e retorna em
   // microssegundos. Quem de fato executa ~Station e a thread T/C, ate um
   // periodo T/C depois (20 ms a 50 Hz), quando o laco
   // 'while (!getParent()->isShutdown())' (PeriodicThread_linux.cpp:53) enfim
   // ve a marcacao do SHUTDOWN_EVENT, sai, e chama 'parent->unref()'.
   //
   // Isso poe a destruicao do grafo INTEIRO (players, JSBSimModel,
   // DataRecorder/TacviewOutput fechando o .acmi, e os objetos do PLUGIN) em
   // paralelo com o que a main faz em seguida: o exit() e os destrutores
   // estaticos de todo .so carregado -- 'mixr::xlog' (g_mutex/g_sink/
   // g_entries), o 'static Ort::Env' de libs/xinfer, e o _IO_cleanup() da
   // glibc liberando os FILE* enquanto o gravador ainda escreve. RTLD_NODELETE
   // mantem o .so MAPEADO, mas nao impede que os destrutores registrados por
   // __cxa_atexit dele rodem.
   //
   // A saida e tomar uma referencia EXTRA aqui: com ela, o unref() da thread
   // T/C nunca pode chegar a zero, e a destruicao acontece deterministicamente
   // NESTA thread, abaixo, com a thread T/C ja fora do laco.
   const bool haviaTcThread{station->doWeHaveTheTcThread()};
   if (haviaTcThread) station->ref();

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
      std::fputs("[app] encerramento travou -- saindo a forca (_Exit)\n", stderr);
      std::_Exit(0);
   });
   watchdog.detach();

   // Marca isShutdown() -- e o que faz o laco da thread T/C terminar -- e ja
   // drena o gravador e fecha o .acmi. Todo o trabalho OBSERVAVEL do
   // encerramento acontece aqui; o unref() abaixo e so liberacao de memoria.
   station->event(mixr::base::Component::SHUTDOWN_EVENT);

   if (haviaTcThread) {
      // PROVA DIRETA de que a thread T/C soltou a referencia dela, em vez de
      // um sleep esperancoso: esperamos exatamente UMA liberacao a partir da
      // contagem de agora (a nossa extra + a da aplicacao + a dela).
      // Referenced::getRefCount() (Referenced.hpp:25) e publico. A leitura nao
      // toma o semaforo do objeto, o que aqui e aceitavel de proposito: o
      // valor so DECRESCE nesta fase, e um valor velho apenas nos faz esperar
      // mais uma volta -- nunca liberar cedo demais.
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
