#include "app/DeterministicRun.hpp"

#include "xtacview/TacviewOutput.hpp"

#include "app/DeterministicDump.hpp"

#include "mixr/simulation/Station.hpp"

#include <iomanip>
#include <atomic>
#include <thread>
#include <iostream>

namespace app {

namespace {
const long dumpEveryNFrames{100};
}

int runDeterministic(mixr::simulation::Station* const station, const Fleet& fleet,
                     const long frames, mixr::xtacview::TacviewOutput* const tacviewOutput,
                     const bool parallelDecision)
{
   const double dt{1.0 / static_cast<double>(station->getTimeCriticalRate())};

   std::cout << "[deterministic] frames=" << frames
             << " dt=" << std::fixed << std::setprecision(9) << dt
             << " (1/tcRate)"
             << (parallelDecision ? "  [DECISAO CONCORRENTE -- determinismo NAO garantido]" : "")
             << std::endl;

   if (parallelDecision) {
      // O LACO DE BACKGROUND SOLTO, na sua propria thread, sem sincronizar com
      // o frame -- que e exatamente o que o tempo real faz
      // (createTimeCriticalProcess() poe o frame numa thread e o main chama
      // updateData na outra, a taxas diferentes).
      //
      // A consequencia e a que este modo existe para mostrar: o NUMERO de
      // decisoes em N frames deixa de ser fixo. Onde a decisao mora no
      // background (( SimAgent )), cada execucao decide um numero diferente de
      // vezes, em instantes diferentes -- e os planos de voo, que integram o
      // tempo a cada decisao (PatrolPlan::advance, ThreatPolicy), divergem.
      std::atomic<bool> rodando{true};
      std::thread bg{[station, dt, &rodando] {
         while (rodando.load(std::memory_order_relaxed)) station->updateData(dt);
      }};

      for (long frame = 1; frame <= frames; ++frame) {
         station->tcFrame(dt);
         if (frame % dumpEveryNFrames == 0 || frame == frames) printDeterministicDump(fleet, frame);
      }

      rodando.store(false, std::memory_order_relaxed);
      bg.join();
      return 0;
   }

   for (long frame = 1; frame <= frames; ++frame) {
      station->tcFrame(dt);

      // Identidade real de cada player pro Tacview, ANTES do updateData que
      // declara os objetos -- ver o cabecalho do .hpp e
      // TacviewOutput::publishIdentities().
      if (tacviewOutput != nullptr) tacviewOutput->publishIdentities(station->getSimulation());

      // Sempre no mesmo passo do tcFrame -- ver o cabecalho do .hpp: e o que
      // drena o gravador e, quando o agente mora na Station, o que o faz
      // decidir uma vez por frame.
      station->updateData(dt);

      if (frame % dumpEveryNFrames == 0 || frame == frames) printDeterministicDump(fleet, frame);
   }
   return 0;
}

} // namespace app
