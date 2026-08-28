#include "app/DeterministicRun.hpp"

#include "app/DeterministicDump.hpp"

#include "mixr/simulation/Station.hpp"

#include <iomanip>
#include <iostream>

namespace app {

namespace {
const long dumpEveryNFrames{100};
}

int runDeterministic(mixr::simulation::Station* const station, const Fleet& fleet,
                     const long frames)
{
   const double dt{1.0 / static_cast<double>(station->getTimeCriticalRate())};

   std::cout << "[deterministic] frames=" << frames
             << " dt=" << std::fixed << std::setprecision(9) << dt
             << " (1/tcRate)" << std::endl;

   for (long frame = 1; frame <= frames; ++frame) {
      station->tcFrame(dt);

      // Sempre no mesmo passo do tcFrame -- ver o cabecalho do .hpp: e o que
      // drena o gravador e, quando o agente mora na Station, o que o faz
      // decidir uma vez por frame.
      station->updateData(dt);

      if (frame % dumpEveryNFrames == 0 || frame == frames) printDeterministicDump(fleet, frame);
   }
   return 0;
}

} // namespace app
