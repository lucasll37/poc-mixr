//
// sim-runner
//
// Constroi UMA Station a partir do cenario montado pelo server (prefixo
// fixo + corpo enviado pelo cliente + sufixo fixo -- ver
// app/ScenarioAssembler.hpp), roda N frames de passo fixo e imprime UMA
// linha JSON na stdout com a telemetria final. Nao ha modo tempo real: e
// sempre um processo de vida curta, um por requisicao HTTP (ver
// src/server/README.md para o porque disso ser arquitetura, nao atalho).
//
// SO ESSA LINHA vai para a stdout -- e ela que o server (Subprocess.cpp) le
// e repassa ao cliente. Por isso: xlog nunca e inicializado aqui (nada
// escreve log), TerrainData nao imprime nada no caminho de sucesso, e
// ScenarioAssembler tambem nao -- qualquer coisa que a propria MIXR
// imprima incondicionalmente (ex.: WorldModel::reset() com "Loading
// Terrain Data...", ver o CLAUDE.md) e tolerada porque o server so olha
// para a ULTIMA linha nao-vazia da stdout, nao para o corpo inteiro.
//
// Falha de build de cenario (edl_parser com erro) sai por std::exit dentro
// de app/StationBuilder.cpp, com a mensagem em stderr -- o server distingue
// isso pelo exit code, nao por parsear a stdout.
//

#include "app/RunnerOptions.hpp"
#include "app/ScenarioAssembler.hpp"
#include "app/StationBuilder.hpp"
#include "app/TerrainData.hpp"
#include "app/Fleet.hpp"
#include "app/TelemetryJson.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

#include "mixr/base/Component.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

const std::string kPrefixPath{"./src/server/configs/scenario_prefix.epp.in"};
const std::string kSuffixPath{"./src/server/configs/scenario_suffix.epp.in"};

const std::string kTerrainDir{"./shared/data/terrain/srtm/"};
const std::string kTerrainTile{"S23W043"};

// O .epp montado vai no MESMO diretorio do corpo recebido (opts.bodyPath) --
// esse diretorio ja e por-requisicao (o server cria um em data/runs/<id>/
// para cada POST /simulate, ver HttpServer.cpp), entao dois sim-runner
// concorrentes nunca escrevem no mesmo arquivo. Um caminho FIXO aqui seria
// uma corrida entre requisicoes simultaneas.
std::string generatedPathFor(const std::string& bodyPath)
{
   const std::filesystem::path dir{std::filesystem::path{bodyPath}.parent_path()};
   return (dir / "scenario.generated.epp").string();
}

// Mesma correcao de app/include/app/Fleet.hpp::applyCruiseThrottle: o
// autopilot do c310 fecha malha de rumo/altitude, nunca de velocidade --
// sem manete fixo a aeronave perde velocidade e estola. Aplicada em TODO
// AirVehicle descoberto, nao so numa lista de nomes fixa: o cliente nao
// declara throttle nenhum no corpo que envia.
constexpr double kCruiseThrottle{0.95};

void applyCruiseThrottle(const std::vector<mixr::models::Player*>& players)
{
   for (const auto player : players) {
      if (auto* const air = dynamic_cast<mixr::models::AirVehicle*>(player)) {
         air->setThrottles(&kCruiseThrottle, 1);
      }
   }
}

} // namespace

int main(int argc, char* argv[])
{
   const app::RunnerOptions opts{app::parseRunnerCommandLine(argc, argv)};

   app::ensureTerrainData(kTerrainDir, kTerrainTile);

   const std::string generatedPath{generatedPathFor(opts.bodyPath)};
   const int numTcThreads{app::assembleScenario(
      kPrefixPath, opts.bodyPath, kSuffixPath, generatedPath, opts.threadsOverride)};

   mixr::simulation::Station* const station{app::buildStation(generatedPath)};
   app::primeStation(station);

   mixr::models::WorldModel* const worldModel{app::worldModelOf(station)};
   const std::vector<mixr::models::Player*> players{app::discoverPlayers(worldModel)};
   applyCruiseThrottle(players);

   const double dt{1.0 / static_cast<double>(station->getTimeCriticalRate())};
   for (long frame = 0; frame < opts.frames; ++frame) {
      station->tcFrame(dt);
      station->updateData(dt);
   }

   const double refLat{worldModel->getRefLatitude()};
   const double refLon{worldModel->getRefLongitude()};

   nlohmann::json response;
   response["framesRequested"] = opts.frames;
   response["framesRun"] = opts.frames;
   response["threads"] = numTcThreads;
   response["simTimeSec"] = opts.frames * dt;
   response["players"] = app::fleetToJson(players, refLat, refLon);

   // dump(-1): sem indentacao -- garante UMA linha so, sem '\n' interno.
   std::cout << response.dump(-1) << std::endl;

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
