//
// app
//
// O RUNNER UNICO deste repositorio -- nenhuma poc sob src/poc/ tem mais
// executavel proprio (ver "Estrutura de um subprojeto" no CLAUDE.md). Um
// painel FTXUI (estilo btop: cores, navegacao por teclado, redesenho
// responsivo) capaz de pausar/acelerar/frear, sobre DUAS fontes de cenario
// possiveis (mutuamente exclusivas, ver app/Options.hpp): '-f <arquivo>'
// (fixtures de teste, ou qualquer cenario apontado direto -- assume a frota
// falcon1..4), '-folder <pasta>' navegando cenarios de sandbox em disco
// (frota descoberta em runtime -- e o caminho para carregar as pocs de
// src/poc/**, ex.: '-f src/poc/dis/flight/configs/scenario.edl.in'). Mora
// fora de src/ (./app/ na raiz) -- e o UNICO ocupante da pasta, por isso o
// alvo se chama 'app', nao 'dashboard' (nome interno das classes, que nao
// mudou).
//
// "Reiniciar" e um REEXEC de si mesmo (app/Respawn.hpp) -- nunca uma
// segunda Station no mesmo processo. Ver o cabecalho de Respawn.hpp para o
// "porque".
//
// Opcoes de linha de comando (lista completa em app/Options.hpp):
//   -f <arquivo> | -folder <pasta>   (um dos dois)
//   -threads <N> | -deterministic <N>
//
// ESTE ARQUIVO SO ORQUESTRA -- mesma divisao de app/ das outras pocs:
//
//    app/Options.hpp             argv -> struct
//    app/AdHocScenario.hpp       ScenarioEntry, e a entrada de '-f <arquivo>'
//    app/ScenarioFolder.hpp      descoberta de '-folder <pasta>' em disco
//    app/ScenarioPickerScreen    tela de selecao (FTXUI), sem Station nenhuma
//    app/TerrainData.hpp         .hgt em disco (copia verbatim das pocs)
//    app/ScenarioTemplate.hpp    .edl.in -> .edl (copia verbatim)
//    app/StationBuilder.hpp      .edl -> Station de pe (copia verbatim)
//    app/Fleet.hpp               os players observados (copia verbatim)
//    app/DeterministicRun.hpp    laco de passo fixo, '-deterministic' (copia verbatim)
//    app/DashboardLoop.hpp       o laco de tempo real, com FTXUI (NOVO)
//    app/Respawn.hpp             reexec de si mesmo (NOVO)
//

#include "app/AdHocScenario.hpp"
#include "app/BehaviorTreeView.hpp"
#include "app/DashboardLoop.hpp"
#include "app/EdlEditorState.hpp"
#include "app/DeterministicRun.hpp"
#include "app/Fleet.hpp"
#include "app/GrootMonitorCheck.hpp"
#include "app/MetaObjectReport.hpp"
#include "app/Options.hpp"
#include "app/Respawn.hpp"
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
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <unistd.h>

namespace {

const double cruiseThrottle{0.95};

// A FROTA nao mora mais aqui: e do CENARIO (ScenarioEntry::fleet). Foi o que
// mudou quando o ./app virou o runner unico das pocs -- 'bandit' tem UM player
// so, e uma lista fixa de falcon1..4 no main abortaria nele. Ver
// app/AdHocScenario.hpp.

const std::string terrainDir{"./shared/data/terrain/srtm/"};
const std::string terrainTile{"S23W043"};

// Um carimbo por PROCESSO, usado em dois lugares: o nome do log do cenario
// escolhido (montado mais abaixo, depois que 'cenario' e conhecido -- ver
// o comentario la) e o token '@RUN_ID@' (ver generateScenario()/
// scenarioTokens mais adiante), que um '.edl.in' pode usar para nomear
// mission_<RUN_ID>.jsonl/.acmi. Nenhum dos tres gravadores em jogo protege
// sozinho contra sobrescrita entre EXECUCOES separadas: MsgFileSink::
// open() e RealtimeTelemetryServer::startRecording() abrem com
// std::ios::trunc, sem nenhum versionamento (ao contrario do
// recorder::PrintHandler nativo, que tenta '_v01'.._v99' antes de
// desistir); e o proprio xlog::init() APAGA o log anterior de proposito,
// para nao disparar esse versionamento nativo a cada respawn (ver o
// comentario dentro de xlog::init()).
//
// O PID entra alem do timestamp -- achado rodando (stress-sweep desta
// sessao): so o segundo NAO basta. Duas instancias do MESMO cenario de
// sandbox lancadas com menos de 1s de diferenca caiam no MESMO RUN_ID e
// escreviam no MESMO inode (confirmado com lsof: dois PIDs, um fd cada,
// mesmo NODE) -- perda silenciosa de dado de uma das duas gravacoes. Dois
// processos SEMPRE tem PIDs diferentes, entao isto fecha a classe inteira
// de colisao por concorrencia, nao so o caso raro do mesmo segundo.
std::string runIdNow()
{
   const std::time_t t{std::time(nullptr)};
   std::tm tm{};
   localtime_r(&t, &tm);
   std::ostringstream oss;
   oss << std::put_time(&tm, "%Y%m%d-%H%M%S") << '-' << ::getpid();
   return oss.str();
}

} // namespace

int main(int argc, char* argv[])
{
   const std::string runId{runIdNow()};

   const app::Options opts{app::parseCommandLine(argc, argv, app::Options{})};

   // Dois modos, mutuamente exclusivos ('-folder' tem prioridade sobre
   // '-f' -- ver app/Options.hpp): '-folder <pasta>' navega uma pasta de
   // sandbox em disco; '-f <arquivo>' e um .edl/.edl.in qualquer (fixtures
   // de teste inclusive). 'chosen' e preenchido POR VALOR pelo ramo que se
   // aplicar -- mais simples que balancear ponteiros pra storages de
   // lifetimes diferentes (o design anterior).
   app::ScenarioEntry chosen{};

   if (!opts.scenarioFolder.empty()) {
      const auto entradas = app::discoverFolderScenarios(opts.scenarioFolder);
      if (entradas.empty()) {
         std::cerr << "app: nenhum cenario encontrado em '" << opts.scenarioFolder << "'" << std::endl;
         return EXIT_FAILURE;
      }

      // '-scenario', combinado com '-folder', e o NOME DA SUBPASTA -- pula
      // a tela (ver app/Options.hpp).
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

   } else {
      // Nenhum dos dois modos foi passado. Uso normal desta aplicacao NUNCA
      // "adivinha" o que abrir: e obrigatorio passar uma das duas opcoes
      // explicitamente.
      std::cerr << "app: e obrigatorio passar -f <arquivo> ou -folder <pasta>"
                << std::endl;
      return EXIT_FAILURE;
   }

   const app::ScenarioEntry& cenario{chosen};

   // O log vai para 'data/logs/' DENTRO da pasta do proprio cenario -- o
   // mesmo lugar onde 'dataRecorder:'/'msgFeed:' do .edl ja escrevem
   // recordings/mensagens (ver o comentario de runIdNow()) -- nunca um
   // caminho global fixo. 'templatePath' e sempre '<pasta-do-cenario>/
   // configs/<arquivo>' (poc via '-f', ou sandbox de '-folder'); subir dois
   // niveis acha essa pasta sem o chamador ter de saber de qual familia o
   // cenario veio.
   std::filesystem::path scenarioDir{
      std::filesystem::path(cenario.templatePath).parent_path().parent_path()};
   // Achado rodando (stress-sweep desta sessao): um '-f' apontando pra um
   // arquivo com MENOS de dois niveis de diretorio acima (ex.: '-f
   // ./x.edl' na propria raiz, ou um caminho absoluto de um componente so)
   // faz os dois 'parent_path()' encalharem em "" ou na RAIZ do sistema de
   // arquivos -- e o log tentaria abrir '/data/logs/...'. Nenhum cenario de
   // poc/sandbox cai aqui (todos tem a forma '<pasta>/configs/<arquivo>');
   // so um '-f' avulso foge do padrao. Fallback: o mesmo lugar que TODO log
   // deste app usava antes desta mudanca.
   if (scenarioDir.empty() || scenarioDir == scenarioDir.root_path()) scenarioDir = "./app";
   mixr::xlog::init((scenarioDir / "data" / "logs" / (cenario.key + "_" + runId + ".log")).string());

   if (opts.isDeterministic()) mixr::xlog::setLoggingEnabled(false);

   app::ensureTerrainData(terrainDir, terrainTile);

   // O tile do EDL acima e OBRIGATORIO (para/sai se faltar -- ver o
   // comentario de ensureTerrainData()). Os demais '.hgt.gz' do mesmo
   // diretorio sao OPCIONAIS -- so ampliam a cobertura da vista de terreno
   // do Mapa (ver app/TerrainQuery.hpp); nenhum cenario depende deles.
   app::ensureAllTerrainTiles(terrainDir);

   // O '.generated.edl' e artefato de RUNTIME, nunca fonte -- vai para
   // 'build/', decoplado de proposito de QUALQUER 'configs/' rastreado no
   // git (o de 'app/' inclusive), mesmo raciocinio ja aplicado a
   // 'build/tests-fixtures'/'build/tests-determinism' (ver a armadilha 9 da
   // secao "Testes automatizados" do CLAUDE.md). Antes disto, TODO cenario
   // rodado por este binario -- os proprios ou os de qualquer poc/fixture
   // passados por '-f'/'-folder' -- escrevia em './app/configs/<key>.
   // generated.edl', poluindo a pasta de configuracao do app com o gerado de
   // pocs inteiramente alheias. Chave-por-'cenario.key' e mantida (nao um
   // nome fixo) para nao reabrir a colisao entre cenarios concorrentes que
   // a chave ja evitava.
   const std::filesystem::path generatedDir{"./build/generated-scenarios"};
   std::error_code ecGeneratedDir;
   std::filesystem::create_directories(generatedDir, ecGeneratedDir);
   const std::string generatedPath{(generatedDir / (cenario.key + ".generated.edl")).string()};
   // 'RUN_ID' entra para TODO cenario, nao so os do proprio ./app -- ver o
   // comentario de runIdNow() acima. Um '.edl.in' que nao usa '@RUN_ID@'
   // simplesmente nao tem esse token pra substituir; a troca so acontece
   // onde o arquivo pede.
   const std::map<std::string, std::string> scenarioTokens{
      {"SCENARIO_ID", cenario.tacviewId},
      {"MODEL_MAP", cenario.tacviewModelMap},
      {"TYPE_MAP", cenario.tacviewTypeMap},
      {"COLOR_MAP", cenario.tacviewColorMap},
      {"RUN_ID", runId},
   };
   const int numTcThreads{
      app::generateScenario(cenario.templatePath, generatedPath, opts.threadsOverride, scenarioTokens)};

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

   // Aqui, e nao dentro do modelo: so' o host tem a lista inteira de players,
   // entao so' ele consegue afirmar "MIXR_GROOT_MONITOR nao casa com ninguem"
   // (ver app/GrootMonitorCheck.hpp). Sem a variavel definida, e' no-op.
   app::checkGrootMonitorTarget(worldModel);

   int rc{};
   app::DashboardExit action{app::DashboardExit::Quit};

   if (opts.isDeterministic()) {
      rc = app::runDeterministic(station, fleet, opts.deterministicFrames, tacviewOutput);
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
      case app::DashboardExit::Restart: {
         // Cenario de '-folder': 'cenario.key' e o nome da SUBPASTA -- reexec
         // precisa levar '-folder' junto. Senao, o cenario veio de '-f'
         // ('cenario.templatePath' e o mesmo caminho que 'opts.scenarioPath'
         // ja tinha).
         std::vector<std::string> args{!opts.scenarioFolder.empty()
            ? std::vector<std::string>{"-folder", opts.scenarioFolder, "-scenario", cenario.key}
            : std::vector<std::string>{"-f", cenario.templatePath}};
         // '-threads' e escolha explicita do usuario (opts.threadsOverride==0
         // significa "sem override" -- ver resolveTcThreadCount()); sem
         // repassar, o reexec recalcularia pelo default de hardware.
         if (opts.threadsOverride > 0) {
            args.push_back("-threads");
            args.push_back(std::to_string(opts.threadsOverride));
         }
         app::respawnSelf(args);
         break;   // [[noreturn]], nunca chega aqui
      }
      case app::DashboardExit::RunEdited: {
         // O texto ja foi escrito em editedScenarioPath() e validado pelo
         // 'edlcheck' dentro de runDashboard() -- so falta o reexec com
         // '-f', o MESMO caminho que uma fixture de teste ja usa (ver
         // app/Options.hpp). generateScenario() roda de novo sobre ele mais
         // adiante, mas e identidade: um '.edl' ja expandido nao tem mais
         // '@include:...@'/'@NUM_TC_THREADS@' para substituir.
         std::vector<std::string> args{"-f", app::editedScenarioPath()};
         if (opts.threadsOverride > 0) {
            args.push_back("-threads");
            args.push_back(std::to_string(opts.threadsOverride));
         }
         app::respawnSelf(args);
         break;   // [[noreturn]], nunca chega aqui
      }
      case app::DashboardExit::Quit:
      default:
         break;
   }

   return 0;
}
