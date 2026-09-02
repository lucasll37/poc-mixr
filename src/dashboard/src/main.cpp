//
// dashboard
//
// Quarta aplicacao do repositorio: a MESMA pilha nativa de single-thread
// (Aircraft/JSBSimModel/Autopilot/radar/SimAgent, o MESMO plugin
// libflight.so, nenhuma mudanca em models/) -- so troca o laco de tempo
// real por um painel FTXUI (estilo btop: cores, navegacao por teclado,
// redesenho responsivo) capaz de pausar/parar/acelerar/frear e carregar um
// dentre tres cenarios PROPRIOS (ver app/ScenarioCatalog.hpp), herméticos e
// com porta de Tacview/diretorio de dados proprios (1236,
// ./src/dashboard/data/) para poder rodar ao lado de single-thread/
// multi-thread sem colidir.
//
// "Carregar outro cenario"/"reiniciar"/"parar" sao um REEXEC de si mesmo
// (app/Respawn.hpp) -- nunca uma segunda Station no mesmo processo. Ver o
// cabecalho de Respawn.hpp para o "porque".
//
// Opcoes de linha de comando:
//   -scenario <chave> | -threads <N> | -deterministic <N> | -parallel-decision
//
// ESTE ARQUIVO SO ORQUESTRA -- mesma divisao de app/ das outras pocs:
//
//    app/Options.hpp             argv -> struct
//    app/ScenarioCatalog.hpp     as chaves de cenario que este poc conhece
//    app/ScenarioPickerScreen    tela de selecao (FTXUI), sem Station nenhuma
//    app/TerrainData.hpp         .hgt em disco (copia verbatim de single-thread)
//    app/ScenarioTemplate.hpp    .epp.in -> .epp (copia verbatim)
//    app/StationBuilder.hpp      .epp -> Station de pe (copia verbatim)
//    app/Fleet.hpp               os players observados (copia verbatim)
//    app/DeterministicRun.hpp    laco de passo fixo, '-deterministic' (copia verbatim)
//    app/DashboardLoop.hpp       o laco de tempo real, com FTXUI (NOVO)
//    app/Respawn.hpp             reexec de si mesmo (NOVO)
//

#include "app/BehaviorTreeView.hpp"
#include "app/DashboardLoop.hpp"
#include "app/DeterministicRun.hpp"
#include "app/Fleet.hpp"
#include "app/MetaObjectReport.hpp"
#include "app/Options.hpp"
#include "app/Respawn.hpp"
#include "app/ScenarioCatalog.hpp"
#include "app/ScenarioPickerScreen.hpp"
#include "app/ScenarioTemplate.hpp"
#include "app/StationBuilder.hpp"
#include "app/TerrainData.hpp"

#include "xlog/Log.hpp"
#include "xtacview/TacviewOutput.hpp"

#include "mixr/simulation/Station.hpp"

#include "mixr/base/Component.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

const double cruiseThrottle{0.95};

// Mesmos quatro falcons das outras pocs -- bandit1 (quando presente, cenario
// 'intercept'/'intercept_missile') e local ao .epp, nunca nesta lista (ver
// o "porque" no comentario equivalente de single-thread/src/main.cpp).
const std::vector<std::string> playerNames{
   "falcon1", "falcon2", "falcon3", "falcon4"
};

const std::string terrainDir{"./shared/data/terrain/srtm/"};
const std::string terrainTile{"S23W043"};

} // namespace

int main(int argc, char* argv[])
{
   mixr::xlog::init("./src/dashboard/data/logs/dashboard.log");

   const app::Options opts{app::parseCommandLine(argc, argv, app::Options{})};

   // Sem '-scenario': mostra a tela de selecao ANTES de tocar em Station
   // nenhuma -- nao precisa de reexec aqui, e a primeira carga do processo.
   std::string scenarioKey{opts.scenarioKey};
   if (scenarioKey.empty()) {
      scenarioKey = app::runScenarioPicker();
      if (scenarioKey.empty()) {
         std::cout << "Nenhum cenario selecionado. Ate mais." << std::endl;
         return 0;
      }
   }

   const app::ScenarioEntry* const entry{app::findScenario(scenarioKey)};
   if (entry == nullptr) {
      std::cerr << "dashboard: cenario desconhecido: '" << scenarioKey << "'" << std::endl;
      return EXIT_FAILURE;
   }

   if (opts.isDeterministic()) mixr::xlog::setLoggingEnabled(false);

   app::ensureTerrainData(terrainDir, terrainTile);

   const std::string generatedPath{"./src/dashboard/configs/" + entry->key + ".generated.epp"};
   const int numTcThreads{
      app::generateScenario(entry->templatePath, generatedPath, opts.threadsOverride)};

   mixr::simulation::Station* const station{app::buildStation(generatedPath)};
   mixr::xclock::ClockStation* const clockStation{app::clockStationOf(station)};
   mixr::xtacview::TacviewOutput* const tacviewOutput{app::tacviewOutputOf(station)};

   app::primeStation(station);

   mixr::models::WorldModel* const worldModel{app::worldModelOf(station)};
   const app::Fleet fleet{app::collectFleet(worldModel, playerNames)};
   app::applyCruiseThrottle(fleet, cruiseThrottle);

   int rc{};
   app::DashboardExit action{app::DashboardExit::Quit};

   if (opts.isDeterministic()) {
      rc = app::runDeterministic(station, fleet, opts.deterministicFrames, opts.parallelDecision);
      app::printMetaObjectReport();
   } else {
      // Best-effort: le o MESMO '.epp' ja expandido pra achar 'treeFile:'
      // e desenhar a arvore no card de detalhe -- ver app/BehaviorTreeView
      // .hpp para o "porque" disso nao ser generico a ponto de dispensar
      // saber que a tecnologia de BT e o BehaviorTree.CPP.
      const app::BtNode behaviorTree{app::loadTreeForScenario(generatedPath)};
      action = app::runDashboard(station, worldModel, clockStation, tacviewOutput,
                                 numTcThreads, entry->label, behaviorTree);
   }

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();

   if (opts.isDeterministic()) return rc;

   switch (action) {
      case app::DashboardExit::Restart:
         app::respawnSelf({"-scenario", entry->key});
         break;   // [[noreturn]], nunca chega aqui
      case app::DashboardExit::ChangeScenario:
         app::respawnSelf({});
         break;   // [[noreturn]], nunca chega aqui
      case app::DashboardExit::Quit:
      default:
         break;
   }

   return 0;
}
