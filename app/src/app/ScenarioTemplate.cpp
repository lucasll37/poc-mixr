#include "app/ScenarioTemplate.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace app {

namespace {

const char* const PLACEHOLDER{"@NUM_TC_THREADS@"};

// Onde os fragmentos EDL compartilhados (ver app/configs/fragments/) moram
// -- caminho relativo, como todo o resto do projeto: os binarios rodam a
// partir da RAIZ do repositorio (convencao documentada no CLAUDE.md).
const char* const kFragmentsDir{"./app/configs/fragments"};

// Uma thread T/C por nucleo, menos a que roda o laco de background; no
// maximo 4 por padrao. O '-threads N' do usuario ainda e limitado pelo
// numero de nucleos -- pedir mais threads do que ha CPUs so acrescenta
// troca de contexto.
int resolveTcThreadCount(const int threadsOverride, unsigned int* const hwThreadsOut)
{
   const unsigned int hwThreads{std::thread::hardware_concurrency()};
   *hwThreadsOut = hwThreads;

   const int maxByCpu{static_cast<int>(hwThreads > 1 ? hwThreads - 1 : 1)};
   const int wanted{(threadsOverride > 0) ? threadsOverride : std::min(maxByCpu, 8)};
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

// Acha '@include:NOME@' e troca pelo conteudo de 'kFragmentsDir/NOME' --
// roda ANTES da substituicao de token (PLACEHOLDER/extraTokens), assim um
// fragmento pode conter tokens que so fazem sentido resolvidos depois do
// include (ex.: '@SCENARIO_ID@' dentro de tacview_recorder.epp.frag). O
// parser EDL nunca ve nem '@include:...@' nem '@TOKEN@' -- a mesma garantia
// que ja vale para '@NUM_TC_THREADS@' desde sempre: a resolucao acontece
// aqui, no .epp.in -> .epp, nao no '.epp' final que o edl_parser le.
//
// Deliberadamente RASO -- um nivel so, sem include recursivo -- para nao
// reintroduzir a complexidade que a auxsencia do preprocessador C evitou
// (ver a armadilha do acento em comentario, documentada no CLAUDE.md).
std::string expandIncludes(std::string text, const std::string& fragmentsDir)
{
   const std::string marker{"@include:"};
   std::size_t pos{};
   while ((pos = text.find(marker, pos)) != std::string::npos) {
      const std::size_t nameStart{pos + marker.size()};
      const std::size_t nameEnd{text.find('@', nameStart)};
      if (nameEnd == std::string::npos) break;   // marcador malformado -- deixa como esta
      const std::string fragName{text.substr(nameStart, nameEnd - nameStart)};
      const std::string fragText{readFileOrDie(fragmentsDir + "/" + fragName)};
      text.replace(pos, (nameEnd + 1) - pos, fragText);
      pos += fragText.size();
   }
   return text;
}

} // namespace

int generateScenario(const std::string& templatePath, const std::string& outPath,
                     const int threadsOverride, const std::map<std::string, std::string>& extraTokens)
{
   unsigned int hwThreads{};
   const int numTcThreads{resolveTcThreadCount(threadsOverride, &hwThreads)};

   std::string text{readFileOrDie(templatePath)};
   text = expandIncludes(std::move(text), kFragmentsDir);
   replaceAll(text, PLACEHOLDER, std::to_string(numTcThreads));
   for (const auto& [name, value] : extraTokens) replaceAll(text, "@" + name + "@", value);

   std::ofstream out(outPath);
   out << text;

   std::cout << "[main] numTcThreads=" << numTcThreads
             << " (hardware_concurrency=" << hwThreads << ")" << std::endl;
   return numTcThreads;
}

} // namespace app
