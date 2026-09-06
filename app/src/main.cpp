//
// dashboard
//
// Quarta aplicacao do repositorio: a MESMA pilha nativa de single-thread
// (Aircraft/JSBSimModel/Autopilot/radar/SimAgent, o MESMO plugin
// libflight.so, nenhuma mudanca em models/) -- so troca o laco de tempo
// real por um painel FTXUI (estilo btop: cores, navegacao por teclado,
// redesenho responsivo) capaz de pausar/parar/acelerar/frear e carregar um
// dentre tres cenarios PROPRIOS (ver app/ScenarioCatalog.hpp), herméticos e
// com porta de Tacview/diretorio de dados proprios (1236, ./app/data/) para
// poder rodar ao lado de single-thread/multi-thread sem colidir. Mora fora
// de src/ (./app/ na raiz) -- e o UNICO ocupante da pasta, por isso o alvo
// se chama 'app', nao 'dashboard'.
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
//    app/ScenarioTemplate.hpp    .edl.in -> .edl (copia verbatim)
//    app/StationBuilder.hpp      .edl -> Station de pe (copia verbatim)
//    app/Fleet.hpp               os players observados (copia verbatim)
//    app/DeterministicRun.hpp    laco de passo fixo, '-deterministic' (copia verbatim)
//    app/DashboardLoop.hpp       o laco de tempo real, com FTXUI (NOVO)
//    app/Respawn.hpp             reexec de si mesmo (NOVO)
//

#include "app/BehaviorTreeView.hpp"
#include "app/DashboardLoop.hpp"
#include "app/EdlEditorState.hpp"
#include "app/DeterministicRun.hpp"
#include "app/Fleet.hpp"
#include "app/MetaObjectReport.hpp"
#include "app/Options.hpp"
#include "app/Respawn.hpp"
#include "app/ScenarioCatalog.hpp"
#include "app/ScenarioFolder.hpp"
#include "app/ScenarioPickerScreen.hpp"
#include "app/ScenarioTemplate.hpp"
#include "app/Shutdown.hpp"
#include "app/StationBuilder.hpp"
#include "app/TerrainData.hpp"

#include "xlog/Log.hpp"
#include "xtacview/TacviewOutput.hpp"

#include "mixr/simulation/Station.hpp"

#include "mixr/base/Component.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

const double cruiseThrottle{0.95};

// A FROTA nao mora mais aqui: e do CENARIO (ScenarioEntry::fleet). Foi o que
// mudou quando o ./app virou o runner unico das pocs -- 'bandit' tem UM player
// so, e uma lista fixa de falcon1..4 no main abortaria nele. Ver
// app/ScenarioCatalog.hpp.

const std::string terrainDir{"./shared/data/terrain/srtm/"};
const std::string terrainTile{"S23W043"};

} // namespace

int main(int argc, char* argv[])
{
   mixr::xlog::init("./app/data/logs/app.log");

   const app::Options opts{app::parseCommandLine(argc, argv, app::Options{})};

   // Tres modos, mutuamente exclusivos ('-folder' tem prioridade sobre os
   // outros dois -- ver app/Options.hpp): '-folder <pasta>' navega uma
   // pasta de sandbox em disco; '-f <arquivo>' e um .edl/.edl.in fora do
   // catalogo (fixtures de teste); nenhum dos dois usa o catalogo estatico
   // do '-scenario <chave>'/tela de selecao. 'chosen' e preenchido POR
   // VALOR pelo ramo que se aplicar -- mais simples que balancear ponteiros
   // pra storages de lifetimes diferentes (o design anterior).
   app::ScenarioEntry chosen{};

   if (!opts.scenarioFolder.empty()) {
      const auto entradas = app::discoverFolderScenarios(opts.scenarioFolder);
      if (entradas.empty()) {
         std::cerr << "app: nenhum cenario encontrado em '" << opts.scenarioFolder << "'" << std::endl;
         return EXIT_FAILURE;
      }

      // '-scenario', combinado com '-folder', deixa de ser uma chave do
      // catalogo e vira o NOME DA SUBPASTA -- pula a tela (ver app/Options.hpp).
      std::string nome{opts.scenarioKey};
      if (nome.empty()) {
         std::vector<app::PickerItem> items;
         items.reserve(entradas.size());
         for (const auto& e : entradas) items.push_back(app::PickerItem{e.name, e.name, e.edlPath});
         nome = app::runPickerScreen(items, "selecione um cenario -- " + opts.scenarioFolder);
         if (nome.empty()) {
            std::cout << "Nenhum cenario selecionado. Ate mais." << std::endl;
            return 0;
         }
      }

      const auto it = std::find_if(entradas.begin(), entradas.end(),
                                   [&](const app::FolderScenarioEntry& e) { return e.name == nome; });
      if (it == entradas.end()) {
         std::cerr << "app: cenario desconhecido em '" << opts.scenarioFolder << "': '" << nome << "'" << std::endl;
         return EXIT_FAILURE;
      }

      // Frota VAZIA -- sinal para main.cpp descobrir os players em runtime
      // (app::discoverFleet(), mais abaixo) em vez de assumir falcon1..4:
      // um cenario de sandbox pode ter qualquer nome de player. Ver o
      // comentario de app::discoverFleet() (app/Fleet.hpp) para o porque
      // isto NAO reabre o problema ja corrigido/revertido em adHocScenario().
      chosen = app::ScenarioEntry{it->name, it->name, "cenario de pasta: " + opts.scenarioFolder,
                                 it->edlPath, "", "", "", "", {}};

   } else if (!opts.scenarioPath.empty()) {
      chosen = app::adHocScenario(opts.scenarioPath);

   } else if (opts.scenarioKey.empty() && !opts.internalPicker) {
      // Nenhum dos tres modos foi passado -- e nao e '-internal-picker'
      // (o reexec interno de "carregar outro cenario"/"parar", ver
      // app/Options.hpp). Uso normal desta aplicacao NUNCA "adivinha" o que
      // abrir: e obrigatorio passar uma das tres opcoes explicitamente.
      std::cerr << "app: e obrigatorio passar -scenario <chave>, -f <arquivo> ou -folder <pasta>"
                << std::endl;
      return EXIT_FAILURE;

   } else {
      // '-scenario <chave>' direto, OU '-internal-picker' sem chave --
      // mostra a tela de selecao do catalogo ANTES de tocar em Station
      // nenhuma (nao precisa de reexec aqui, e a primeira carga do
      // processo desse ramo).
      std::string scenarioKey{opts.scenarioKey};
      if (scenarioKey.empty()) {
         scenarioKey = app::runScenarioPicker();
         if (scenarioKey.empty()) {
            std::cout << "Nenhum cenario selecionado. Ate mais." << std::endl;
            return 0;
         }
      }
      const app::ScenarioEntry* const found{app::findScenario(scenarioKey)};
      if (found == nullptr) {
         std::cerr << "app: cenario desconhecido: '" << scenarioKey << "'" << std::endl;
         return EXIT_FAILURE;
      }
      chosen = *found;
   }

   const app::ScenarioEntry& cenario{chosen};

   if (opts.isDeterministic()) mixr::xlog::setLoggingEnabled(false);

   app::ensureTerrainData(terrainDir, terrainTile);

   // O tile do EDL acima e OBRIGATORIO (para/sai se faltar -- ver o
   // comentario de ensureTerrainData()). Os demais '.hgt.gz' do mesmo
   // diretorio sao OPCIONAIS -- so ampliam a cobertura da vista de terreno
   // do Mapa (ver app/TerrainQuery.hpp); nenhum cenario depende deles.
   app::ensureAllTerrainTiles(terrainDir);

   const std::string generatedPath{"./app/configs/" + cenario.key + ".generated.edl"};
   const std::map<std::string, std::string> tacviewTokens{
      {"SCENARIO_ID", cenario.tacviewId},
      {"MODEL_MAP", cenario.tacviewModelMap},
      {"TYPE_MAP", cenario.tacviewTypeMap},
      {"COLOR_MAP", cenario.tacviewColorMap},
   };
   const int numTcThreads{
      app::generateScenario(cenario.templatePath, generatedPath, opts.threadsOverride, tacviewTokens)};

   mixr::simulation::Station* const station{app::buildStation(generatedPath)};
   mixr::xclock::ClockStation* const clockStation{app::clockStationOf(station)};
   mixr::xtacview::TacviewOutput* const tacviewOutput{app::tacviewOutputOf(station)};
   // So o cenario 'bandit' declara 'ioHandler:'; nos demais isto e nullptr e
   // vira um aviso, nao erro (ver StationBuilder.cpp).
   mixr::linkage::IoHandler* const ioHandler{app::ioHandlerOf(station)};

   app::primeStation(station);

   mixr::models::WorldModel* const worldModel{app::worldModelOf(station)};
   // Frota vazia == entrada sintetica de '-folder' (ver acima) -- descobre
   // os players de verdade em vez de assumir uma lista de nomes que
   // ninguem pode garantir para um cenario de sandbox.
   const app::Fleet fleet{cenario.fleet.empty()
                              ? app::discoverFleet(worldModel)
                              : app::collectFleet(worldModel, cenario.fleet)};
   app::applyCruiseThrottle(fleet, cruiseThrottle);

   int rc{};
   app::DashboardExit action{app::DashboardExit::Quit};

   if (opts.isDeterministic()) {
      rc = app::runDeterministic(station, fleet, opts.deterministicFrames, tacviewOutput,
                                 opts.parallelDecision);
      app::printMetaObjectReport();
   } else {
      // Best-effort: le o MESMO '.edl' ja expandido pra achar 'treeFile:'
      // e desenhar a arvore no card de detalhe -- ver app/BehaviorTreeView
      // .hpp para o "porque" disso nao ser generico a ponto de dispensar
      // saber que a tecnologia de BT e o BehaviorTree.CPP.
      const app::BtNode behaviorTree{app::loadTreeForScenario(generatedPath)};
      action = app::runDashboard(station, worldModel, clockStation, tacviewOutput,
                                 ioHandler, numTcThreads, cenario.label, behaviorTree,
                                 generatedPath);
   }

   // Nao e 'event(SHUTDOWN_EVENT) + unref()' cru: ver app/Shutdown.hpp para o
   // porque -- o teardown do MIXR nao e observavel e ja travou o processo aqui.
   // No caminho interativo a thread T/C nativa ja foi calada dentro de
   // runDashboard(); em '-deterministic' nao existe thread T/C nenhuma.
   app::shutdownStation(station);

   if (opts.isDeterministic()) return rc;

   switch (action) {
      case app::DashboardExit::Restart:
         // Cenario de '-folder': 'cenario.key' e o nome da SUBPASTA, nao uma
         // chave do catalogo -- reexec precisa levar '-folder' junto, ou
         // '-scenario <nome-da-subpasta>' sozinho cairia no catalogo estatico
         // e provavelmente acharia cenario nenhum.
         if (!opts.scenarioFolder.empty()) {
            app::respawnSelf({"-folder", opts.scenarioFolder, "-scenario", cenario.key});
         } else {
            app::respawnSelf({"-scenario", cenario.key});
         }
         break;   // [[noreturn]], nunca chega aqui
      case app::DashboardExit::ChangeScenario:
         // '-internal-picker', nao vazio -- ver app/Options.hpp. Sem essa
         // flag, o processo respawnado recusaria de cara (opcao obrigatoria).
         app::respawnSelf({"-internal-picker"});
         break;   // [[noreturn]], nunca chega aqui
      case app::DashboardExit::RunEdited:
         // O texto ja foi escrito em editedScenarioPath() e validado pelo
         // 'edlcheck' dentro de runDashboard() -- so falta o reexec com
         // '-f', o MESMO caminho que uma fixture de teste ja usa (ver
         // app/Options.hpp). generateScenario() roda de novo sobre ele mais
         // adiante, mas e identidade: um '.edl' ja expandido nao tem mais
         // '@include:...@'/'@NUM_TC_THREADS@' para substituir.
         app::respawnSelf({"-f", app::editedScenarioPath()});
         break;   // [[noreturn]], nunca chega aqui
      case app::DashboardExit::Quit:
      default:
         break;
   }

   return 0;
}
