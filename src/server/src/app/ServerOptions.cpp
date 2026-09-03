#include "app/ServerOptions.hpp"

#include <string>

namespace app {

ServerOptions parseServerCommandLine(const int argc, char* argv[])
{
   ServerOptions opts;

   for (int i = 1; i < argc; i++) {
      const std::string arg{argv[i]};
      if (arg == "-port" && (i + 1) < argc) {
         opts.port = std::stoi(argv[++i]);
      } else if (arg == "-bind" && (i + 1) < argc) {
         opts.bindAddress = argv[++i];
      } else if (arg == "-runner" && (i + 1) < argc) {
         opts.runnerPath = argv[++i];
      } else if (arg == "-max-concurrent" && (i + 1) < argc) {
         opts.maxConcurrentSims = std::stoi(argv[++i]);
      } else if (arg == "-timeout" && (i + 1) < argc) {
         opts.subprocessTimeoutSec = std::stoi(argv[++i]);
      }
   }

   return opts;
}

} // namespace app
