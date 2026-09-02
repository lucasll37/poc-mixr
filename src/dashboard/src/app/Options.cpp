#include "app/Options.hpp"

#include <string>

namespace app {

Options parseCommandLine(const int argc, char* argv[], const Options& defaults)
{
   Options opts{defaults};

   for (int i = 1; i < argc; i++) {
      const std::string arg{argv[i]};
      if (arg == "-scenario" && (i + 1) < argc) {
         opts.scenarioKey = argv[++i];
      } else if (arg == "-deterministic" && (i + 1) < argc) {
         opts.deterministicFrames = std::stol(argv[++i]);
      } else if (arg == "-parallel-decision") {
         opts.parallelDecision = true;
      } else if (arg == "-threads" && (i + 1) < argc) {
         opts.threadsOverride = std::stoi(argv[++i]);
      }
   }

   return opts;
}

} // namespace app
