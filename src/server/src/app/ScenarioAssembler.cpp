#include "app/ScenarioAssembler.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace app {

namespace {

const char* const PLACEHOLDER{"@NUM_TC_THREADS@"};

// Mesma regra de src/poc/*/app/ScenarioTemplate.cpp: uma thread T/C por
// nucleo, menos a que roda o laco de background, no maximo 8 por padrao.
int resolveTcThreadCount(const int threadsOverride)
{
   const unsigned int hwThreads{std::thread::hardware_concurrency()};
   const int maxByCpu{static_cast<int>(hwThreads > 1 ? hwThreads - 1 : 1)};
   const int wanted{(threadsOverride > 0) ? threadsOverride : std::min(maxByCpu, 8)};
   return std::max(1, std::min(wanted, maxByCpu));
}

std::string readFileOrDie(const std::string& path)
{
   std::ifstream in(path);
   if (!in.good()) {
      std::cerr << "[sim-runner] nao consegui ler " << path << std::endl;
      std::exit(EXIT_FAILURE);
   }
   std::ostringstream buf;
   buf << in.rdbuf();
   return buf.str();
}

void replaceAll(std::string& text, const std::string& from, const std::string& to)
{
   std::size_t pos{};
   while ((pos = text.find(from, pos)) != std::string::npos) {
      text.replace(pos, from.size(), to);
      pos += to.size();
   }
}

} // namespace

int assembleScenario(const std::string& prefixPath, const std::string& bodyPath,
                     const std::string& suffixPath, const std::string& outPath,
                     const int threadsOverride)
{
   const int numTcThreads{resolveTcThreadCount(threadsOverride)};

   std::string text{readFileOrDie(prefixPath)};
   text += readFileOrDie(bodyPath);
   text += readFileOrDie(suffixPath);

   replaceAll(text, PLACEHOLDER, std::to_string(numTcThreads));

   std::ofstream out(outPath);
   if (!out.good()) {
      std::cerr << "[sim-runner] nao consegui escrever " << outPath << std::endl;
      std::exit(EXIT_FAILURE);
   }
   out << text;

   return numTcThreads;
}

} // namespace app
