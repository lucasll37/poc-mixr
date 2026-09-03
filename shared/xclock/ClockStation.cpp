#include "xclock/ClockStation.hpp"

#include "mixr/simulation/Simulation.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <algorithm>
#include <cmath>

namespace mixr {
namespace xclock {

IMPLEMENT_SUBCLASS(ClockStation, "ClockStation")
EMPTY_SLOTTABLE(ClockStation)
EMPTY_DELETEDATA(ClockStation)

namespace {
// Abaixo de 0.05x um segundo de parede vale menos de um centesimo de
// segundo simulado -- na pratica ja e uma pausa, com o custo de rodar a
// simulacao inteira. Acima de 1000x o laco T/C nao fecha o periodo neste
// hardware e o "x" pedido deixa de ser o "x" entregue.
const double kMinTimeScale{0.05};
const double kMaxTimeScale{1000.0};
}

ClockStation::ClockStation()
{
   STANDARD_CONSTRUCTOR()
}

void ClockStation::copyData(const ClockStation& org, const bool)
{
   BaseClass::copyData(org);
   slowFactor = org.slowFactor;
   // 'tcStopRequested_'/'tcIdleTicks_' NAO sao copiados de proposito: sao
   // estado de CICLO DE VIDA desta instancia (a thread T/C que aponta para
   // ela), nao configuracao do cenario. Uma copia nasce sem pedido de parada.
}

double ClockStation::getMinTimeScale() { return kMinTimeScale; }
double ClockStation::getMaxTimeScale() { return kMaxTimeScale; }

double ClockStation::getTimeScale() const
{
   // Fonte unica de verdade: abaixo de 1x quem manda e o slowFactor, de 1x
   // para cima quem manda e o contador NATIVO da Station.
   if (slowFactor < 1.0) return slowFactor;
   return static_cast<double>(getFastForwardRate());
}

bool ClockStation::setTimeScale(const double scale)
{
   if (scale < kMinTimeScale || scale > kMaxTimeScale) return false;

   if (scale >= 1.0) {
      slowFactor = 1.0;
      // O slot nativo e inteiro, entao 1.5x nao existe -- arredonda.
      return setFastForwardRate(static_cast<unsigned int>(scale + 0.5));
   }

   slowFactor = scale;
   return setFastForwardRate(1);
}

bool ClockStation::isPaused() const
{
   const simulation::Simulation* const sim{getSimulation()};
   return (sim != nullptr) && sim->isFrozen();
}

bool ClockStation::setPaused(const bool flag)
{
   simulation::Simulation* const sim{getSimulation()};
   if (sim == nullptr) return false;
   sim->freeze(flag);
   return true;
}

bool ClockStation::togglePaused()
{
   return setPaused(!isPaused());
}

//------------------------------------------------------------------------------
// requestTcStop() / waitForTcQuiesced() -- ver o "porque" longo no .hpp.
//------------------------------------------------------------------------------
void ClockStation::requestTcStop()
{
   tcStopRequested_.store(true, std::memory_order_release);
}

bool ClockStation::waitForTcQuiesced(const double timeoutSec)
{
   // Sem thread T/C nativa (o modo '-deterministic' chama tcFrame() direto,
   // na propria thread do laco) nao ha nada para esperar.
   if (!doWeHaveTheTcThread()) return true;

   // Um tick JA prova o que interessa: a thread T/C e uma so e sequencial,
   // entao ve-la entrar em processTimeCriticalTasks() depois da marcacao quer
   // dizer que o tcFrame() anterior retornou. O segundo tick e folga barata.
   const unsigned long alvo{2};

   const int passoMs{2};
   const int tentativas{std::max(1, static_cast<int>((timeoutSec * 1000.0) / passoMs))};
   for (int i = 0; i < tentativas; i++) {
      if (tcIdleTicks_.load(std::memory_order_acquire) >= alvo) return true;
      base::msleep(passoMs);
   }
   return tcIdleTicks_.load(std::memory_order_acquire) >= alvo;
}

//------------------------------------------------------------------------------
// processTimeCriticalTasks() -- unico ponto em que esta classe se mete no
// caminho do framework.
//------------------------------------------------------------------------------
void ClockStation::processTimeCriticalTasks(const double dt)
{
   // PAUSA: nao avanca frame nenhum.
   //
   // So marcar o freeze NAO bastaria, e o motivo e uma ordem infeliz dentro
   // de Simulation::updateTC(): 'execTime += dt' acontece na linha 462, ANTES
   // do 'if (isFrozen()) dt0 = 0.0' da linha 498 -- e usa o dt cru, nao o
   // dt0. Ou seja, com a simulacao congelada o mundo para, mas
   // getExecTimeSec() continua correndo. Isso vazaria para o Tacview, que
   // data cada linha ACMI justamente com exec_time (TacviewOutput.cpp:373):
   // o replay avancaria com as aeronaves paradas. Medido rodando antes desta
   // correcao: com a simulacao congelada, 'sim=' ainda subia.
   //
   // Nao chamar tcFrame() resolve os dois de uma vez -- o relogio de execucao
   // para junto com o mundo, e ainda deixa de queimar CPU integrando um
   // estado que nao muda.
   //
   // O flag de freeze CONTINUA valendo (setPaused o mantem): ele e o que
   // congela o outro caminho, o de background -- Simulation::updateData()
   // tem o mesmo teste (linha 625), e esse laco nao passa por aqui.

   // PARADA pedida: nenhum frame novo comeca, e registramos a passagem para
   // que waitForTcQuiesced() tenha prova de que esta thread saiu de
   // tcFrame(). Vem ANTES do isPaused() de proposito -- a prova tem de valer
   // tambem com a simulacao pausada, que e um estado perfeitamente possivel
   // no momento em que o usuario decide sair.
   if (tcStopRequested_.load(std::memory_order_acquire)) {
      tcIdleTicks_.fetch_add(1, std::memory_order_release);
      return;
   }

   if (isPaused()) return;

   if (slowFactor >= 1.0) {
      // Tempo real ou acelerado: caminho NATIVO, intocado. A base repete
      // tcFrame(dt) tantas vezes quanto disser o fastForwardRate.
      BaseClass::processTimeCriticalTasks(dt);
      return;
   }

   // Camara lenta: um unico frame, com o dt encurtado na proporcao pedida.
   tcFrame(dt * slowFactor);
}

}
}
