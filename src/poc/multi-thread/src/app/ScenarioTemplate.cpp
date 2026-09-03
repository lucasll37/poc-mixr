#include "app/ScenarioTemplate.hpp"

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

// Uma thread T/C por nucleo, menos a que roda o laco de background; no
// maximo 4 por padrao. O '-threads N' do usuario ainda e limitado pelo
// numero de nucleos -- pedir mais threads do que ha CPUs so acrescenta
// troca de contexto.
int resolveTcThreadCount(const int threadsOverride, unsigned int* const hwThreadsOut)
{
   const unsigned int hwThreads{std::thread::hardware_concurrency()};
   *hwThreadsOut = hwThreads;

   const int maxByCpu{static_cast<int>(hwThreads > 1 ? hwThreads - 1 : 1)};
   const int wanted{(threadsOverride > 0) ? threadsOverride : std::min(maxByCpu, 4)};
   return std::max(1, std::min(wanted, maxByCpu));
}

std::string readFileOrDie(const std::string& path)
{
   std::ifstream in(path);
   if (!in.good()) {
      std::cerr << "[main] nao consegui ler " << path << std::endl;
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

int generateScenario(const std::string& templatePath, const std::string& outPath,
                     const int threadsOverride)
{
   unsigned int hwThreads{};
   const int numTcThreads{resolveTcThreadCount(threadsOverride, &hwThreads)};

   std::string text{readFileOrDie(templatePath)};
   replaceAll(text, PLACEHOLDER, std::to_string(numTcThreads));

   std::ofstream out(outPath);
   out << text;

   std::cout << "[main] numTcThreads=" << numTcThreads
             << " (hardware_concurrency=" << hwThreads << ")" << std::endl;
   return numTcThreads;
}

} // namespace app
