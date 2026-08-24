#include "xclock/ClockStation.hpp"

#include "mixr/simulation/Simulation.hpp"

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
