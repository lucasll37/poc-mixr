#include "app/RunnerOptions.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace app {

RunnerOptions parseRunnerCommandLine(const int argc, char* argv[])
{
   RunnerOptions opts;

   for (int i = 1; i < argc; i++) {
      const std::string arg{argv[i]};
      if (arg == "-f" && (i + 1) < argc) {
         opts.bodyPath = argv[++i];
      } else if (arg == "-frames" && (i + 1) < argc) {
         opts.frames = std::stol(argv[++i]);
      } else if (arg == "-threads" && (i + 1) < argc) {
         opts.threadsOverride = std::stoi(argv[++i]);
      }
   }

   if (opts.bodyPath.empty()) {
      std::cerr << "[sim-runner] uso: sim-runner -f <corpo.epp> -frames <N> [-threads <N>]" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   if (opts.frames <= 0) {
      std::cerr << "[sim-runner] -frames tem de ser positivo" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   return opts;
}

} // namespace app
