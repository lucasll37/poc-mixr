#include "app/Options.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace app {

namespace {

// std::stol/std::stoi lancam std::invalid_argument (token nao numerico) ou
// std::out_of_range (estoura o tipo) -- sem isto a excecao escapava ate o topo
// e virava um abort() sem nenhuma mensagem apontando QUAL argumento estava
// errado. Toda outra entrada invalida desta aplicacao (arquivo de cenario,
// player ausente, banco de elevacao corrompido -- ver TerrainData/
// StationBuilder/Fleet) sai por std::exit com uma mensagem clara; um
// '-threads abc' devia se comportar da mesma forma, nao travar com um sinal.
long parseLongOrDie(const std::string& flag, const std::string& token)
{
   try {
      return std::stol(token);
   } catch (const std::exception&) {
      std::cerr << "[app] " << flag << " espera um numero inteiro, recebi '"
                << token << "'" << std::endl;
      std::exit(EXIT_FAILURE);
   }
}

int parseIntOrDie(const std::string& flag, const std::string& token)
{
   try {
      return std::stoi(token);
   } catch (const std::exception&) {
      std::cerr << "[app] " << flag << " espera um numero inteiro, recebi '"
                << token << "'" << std::endl;
      std::exit(EXIT_FAILURE);
   }
}

} // namespace

Options parseCommandLine(const int argc, char* argv[], const Options& defaults)
{
   Options opts{defaults};

   for (int i = 1; i < argc; i++) {
      const std::string arg{argv[i]};
      if (arg == "-scenario" && (i + 1) < argc) {
         opts.scenarioKey = argv[++i];
      } else if (arg == "-f" && (i + 1) < argc) {
         opts.scenarioPath = argv[++i];
      } else if (arg == "-deterministic" && (i + 1) < argc) {
         opts.deterministicFrames = parseLongOrDie(arg, argv[++i]);
      } else if (arg == "-parallel-decision") {
         opts.parallelDecision = true;
      } else if (arg == "-threads" && (i + 1) < argc) {
         opts.threadsOverride = parseIntOrDie(arg, argv[++i]);
      }
   }

   return opts;
}

} // namespace app
