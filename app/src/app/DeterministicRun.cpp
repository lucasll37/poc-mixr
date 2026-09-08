#include "app/DeterministicRun.hpp"

#include "xtacview/TacviewOutput.hpp"

#include "app/DeterministicDump.hpp"

#include "mixr/simulation/Station.hpp"

#include <iomanip>
#include <iostream>

namespace app {

namespace {
const long dumpEveryNFrames{100};
}

int runDeterministic(mixr::simulation::Station* const station, const Fleet& fleet,
                     const long frames, mixr::xtacview::TacviewOutput* const tacviewOutput)
{
   const double dt{1.0 / static_cast<double>(station->getTimeCriticalRate())};

   std::cout << "[deterministic] frames=" << frames
             << " dt=" << std::fixed << std::setprecision(9) << dt
             << " (1/tcRate)"
             << std::endl;

   for (long frame = 1; frame <= frames; ++frame) {
      station->tcFrame(dt);

      // Identidade real de cada player pro Tacview, ANTES do updateData que
      // declara os objetos -- ver o cabecalho do .hpp e
      // TacviewOutput::publishIdentities().
      if (tacviewOutput != nullptr) tacviewOutput->publishIdentities(station->getSimulation());

      // Sempre no mesmo passo do tcFrame -- ver o cabecalho do .hpp: e o que
      // drena o gravador para o Tacview. A decisao ja aconteceu na fase 3
      // do tcFrame acima.
      station->updateData(dt);

      if (frame % dumpEveryNFrames == 0 || frame == frames) printDeterministicDump(fleet, frame);
   }
   return 0;
}

} // namespace app
