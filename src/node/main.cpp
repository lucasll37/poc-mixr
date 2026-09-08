#include "Run.hpp"
#include "factory.hpp"

#include "xplugin/PluginRegistry.hpp"
#include "xclock/ClockStation.hpp"
#include "xlog/Log.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/base/Component.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <cstdlib>
#include <ctime>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

namespace fs = std::filesystem;

// Carimbo AAAAMMDD-HHMMSS-<pid> -- so pra nomear o arquivo de log desta
// execucao sem colidir com a de outro processo no mesmo segundo.
std::string runIdNow()
{
   const std::time_t t{std::time(nullptr)};
   std::tm tmBuf{};
   ::localtime_r(&t, &tmBuf);
   char buf[24];
   std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tmBuf);
   return std::string(buf) + "-" + std::to_string(::getpid());
}

// "src/poc/dis/flight/configs/scenario.edl.in" -> "scenario" + "flight" (a
// pasta duas acima de configs/x.edl.in) -> "flight-scenario". A pasta entra
// na chave porque o nome do ARQUIVO sozinho nao e unico entre pocs --
// flight/bandit/python-flight/onnx-policy usam todos "scenario.edl[.in]" --
// e a chave vira o caminho de build/generated-scenarios/<chave>.generated.edl
// e do arquivo de log, os dois GLOBAIS ao processo (nao aninhados por poc).
// Sem a pasta, dois processos `node` apontando pra pocs DIFERENTES mas com o
// MESMO nome de arquivo (o caso canonico do grupo DIS, `flight`+`bandit`
// rodando juntos) colidem no mesmo cenario gerado -- confirmado rodando os
// dois ao mesmo tempo antes desta correcao: o processo do `bandit` carregava
// e executava o cenario inteiro do `flight`, em silencio (so a porta Tacview
// duplicada acusava algo errado, e por um sintoma indireto).
std::string deriveScenarioKey(const std::string& path)
{
   const fs::path p{path};
   std::string stem{p.stem().string()}; // remove UMA extensao: "x.edl.in" -> "x.edl"
   if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, ".edl") == 0) {
      stem.resize(stem.size() - 4); // "x.edl" -> "x"
   }
   if (stem.empty()) stem = "ad-hoc";

   // Mesma pasta que 'scenarioDir' calcula em main() (parent_path() duas
   // vezes) -- duplicado aqui de proposito para 'deriveScenarioKey()'
   // continuar uma funcao pura, testavel isolada de 'main()'.
   const fs::path pocDir{p.parent_path().parent_path()};
   const std::string pocName{pocDir.empty() ? std::string{} : pocDir.filename().string()};
   return pocName.empty() ? stem : (pocName + "-" + stem);
}

// Mesmo tile fixo que o resto do repositorio ja vendoriza
// (shared/data/terrain/srtm/), incondicional para qualquer cenario --
// harmless se o cenario nao usar terreno nenhum, e evita a armadilha
// documentada de Player::updateElevation() lendo 0.0 em silencio sem o
// arquivo no lugar.
void ensureTerrainTile(const std::string& dir, const std::string& baseName)
{
   const std::string hgt{dir + baseName + ".hgt"};
   if (mixr::base::doesFileExist(hgt.c_str())) return;

   const std::string gz{hgt + ".gz"};
   if (!mixr::base::doesFileExist(gz.c_str())) {
      std::cerr << "node: terreno ausente: " << gz << "\n";
      std::exit(EXIT_FAILURE);
   }

   const std::string cmd{"gunzip -kf \"" + gz + "\""};
   std::system(cmd.c_str());

   if (!mixr::base::doesFileExist(hgt.c_str())) {
      std::cerr << "node: falha ao descomprimir " << gz << "\n";
      std::exit(EXIT_FAILURE);
   }
}

// hardware_concurrency()-1, teto 8 -- mesma formula ja usada no restante do
// repositorio para o numero automatico de threads de tempo critico.
unsigned int resolveTcThreadCount()
{
   const unsigned int hw{std::thread::hardware_concurrency()};
   unsigned int n{(hw > 1) ? (hw - 1) : 1};
   if (n > 8) n = 8;
   return n;
}

// .edl.in -> .edl. Confirmado por varredura de todo src/poc/**: o UNICO
// token usado pelos cenarios de producao e @NUM_TC_THREADS@ -- por isso so
// uma troca de string, sem a maquina generica de @include:@/tokens
// arbitrarios (nenhum cenario que este binario roda usa isso).
std::string generateScenario(const std::string& templatePath, const std::string& outPath)
{
   std::ifstream in{templatePath};
   if (!in) {
      std::cerr << "node: nao foi possivel abrir " << templatePath << "\n";
      std::exit(EXIT_FAILURE);
   }
   std::ostringstream ss;
   ss << in.rdbuf();
   std::string content{ss.str()};

   const std::string token{"@NUM_TC_THREADS@"};
   const std::string value{std::to_string(resolveTcThreadCount())};
   std::string::size_type pos{0};
   while ((pos = content.find(token, pos)) != std::string::npos) {
      content.replace(pos, token.size(), value);
      pos += value.size();
   }

   std::ofstream out{outPath};
   out << content;
   return outPath;
}

// edl_parser + desembrulho do Pair de topo -- mesma sequencia confirmada em
// src/rl/bindings/StationBuilder.cpp.
mixr::simulation::Station* buildStation(const std::string& filename)
{
   mixr::xplugin::setBuiltinFactory(node::factoryBuiltin);

   int numErrors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, node::factory, &numErrors)};
   mixr::xplugin::seal();

   if (numErrors > 0 || obj == nullptr) {
      std::cerr << "node: falha ao interpretar " << filename << "\n";
      std::exit(EXIT_FAILURE);
   }

   const auto pair = dynamic_cast<mixr::base::Pair*>(obj);
   if (pair != nullptr) {
      obj = pair->object();
      obj->ref();
      pair->unref();
   }

   const auto station = dynamic_cast<mixr::simulation::Station*>(obj);
   if (station == nullptr) {
      std::cerr << "node: " << filename << " nao produz uma ( Station )\n";
      std::exit(EXIT_FAILURE);
   }
   return station;
}

// RESET_EVENT + um frame de partida (resolve ponteiros entre subsistemas
// antes do primeiro uso de verdade).
void primeStation(mixr::simulation::Station* const station)
{
   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(1.0 / station->getTimeCriticalRate());
}

}

int main(int argc, char* argv[])
{
   if (argc != 2) {
      std::cerr << "uso: node <arquivo.edl|.edl.in>\n";
      return EXIT_FAILURE;
   }

   const std::string scenarioPath{argv[1]};
   const std::string runId{runIdNow()};
   const std::string key{deriveScenarioKey(scenarioPath)};

   fs::path scenarioDir{fs::path(scenarioPath).parent_path().parent_path()};
   if (scenarioDir.empty()) scenarioDir = "./src/node";
   mixr::xlog::init((scenarioDir / "data" / "logs" / (key + "_" + runId + ".log")).string());

   ensureTerrainTile("./shared/data/terrain/srtm/", "S23W043");

   fs::create_directories("./build/generated-scenarios");
   const std::string generatedPath{"./build/generated-scenarios/" + key + ".generated.edl"};
   generateScenario(scenarioPath, generatedPath);

   mixr::simulation::Station* const station{buildStation(generatedPath)};

   auto* const clockStation{dynamic_cast<mixr::xclock::ClockStation*>(station)};
   if (clockStation == nullptr) {
      std::cerr << "node: aviso: a Station do cenario nao e uma ( ClockStation );"
                   " encerramento sem prova positiva de ociosidade\n";
   }

   primeStation(station);

   const int rc{node::run(station, clockStation)};
   node::shutdownStation(station);

   return rc;
}
