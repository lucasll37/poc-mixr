//
// python-flight
//
// O multi-thread INTEIRO, com UMA unica diferenca: as LEIS DE VOO nao estao
// compiladas em lugar nenhum -- sao quatro arquivos .py em
// configs/policy/, lidos em tempo de execucao.
//
//    multi-thread:   a arvore chama nos C++ do plugin (ReportAndEvade,
//                    Patrol, ReturnToBase, SupportAlert)
//
//    python-flight:  a arvore chama ( PyDecide ), que roda decide(obs) de um
//                    script Python DENTRO da fase 3 do frame -- mesma
//                    thread, mesmo tick, sem processo nem soquete no meio
//
// Nada disto e novo: shared/xpyembed ja embarcava o interpretador e
// bt/nodes/PyDecideAction ja era o no. O que este subprojeto acrescenta e
// uma poc COMPLETA em cima dessas duas pecas -- cenario proprio, arvore
// propria, os quatro scripts, portas proprias -- em vez do unico
// flight_tree_py.xml de exemplo que so era exercitado por um teste.
//
// COMO SE USA: edite um .py de configs/policy/, rode de novo. Nao ha
// compilacao entre as duas coisas, nem do host nem do plugin.
//
// O QUE CONTINUA EM C++, e nao por acaso: as CONDICOES da arvore (FuelLow,
// ContactDetected, AlertReceived) e a rede de seguranca de altitude
// ( AltitudeSafetyBehavior, voto 90 contra os 50 da arvore). A razao esta no
// cabecalho de configs/flight_tree_python.xml: elas dependem do 'dt' do
// frame, que a fronteira "28 floats entram, 3 saem" de shared/xpyembed nao
// atravessa -- e mante-las la e o que deixa cada script PURO em relacao ao
// estado da propria aeronave, que e a condicao do determinismo.
//
// PORTAS PROPRIAS (Tacview 1237, DIS 3004): roda ao lado das outras pocs.
//
// Opcoes de linha de comando:
//   -f <arquivo> | -threads <N> | -deterministic <N>
//
// ESTE ARQUIVO SO ORQUESTRA. Cada etapa mora no seu proprio modulo, em
// app/ -- ver o cabecalho de cada header para o "por que":
//
//    app/Options.hpp            argv -> struct
//    app/ScenarioTemplate.hpp   .epp.in -> .epp (numTcThreads)
//    app/StationBuilder.hpp     .epp -> Station de pe
//    app/Fleet.hpp              os players observados
//    app/DeterministicRun.hpp   laco de passo fixo ('-deterministic')
//    app/RealTimeRun.hpp        laco de tempo real (o modo normal)
//

#include "app/DeterministicRun.hpp"
#include "app/Fleet.hpp"
#include "app/MetaObjectReport.hpp"
#include "app/Options.hpp"
#include "app/RealTimeRun.hpp"
#include "app/ScenarioTemplate.hpp"
#include "app/Shutdown.hpp"
#include "app/StationBuilder.hpp"
#include "app/TerrainData.hpp"

#include "xlog/Log.hpp"
#include "xtacview/TacviewOutput.hpp"

#include "mixr/simulation/Station.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

const double cruiseThrottle{0.95};

// bandit1 (o intruso) e um player NETWORKED (recebido via 'networks:', ver
// scenario.epp.in), nunca declarado localmente -- por isso nao entra nesta
// lista: Fleet::collectFleet() aborta o processo se um nome dela nao
// existir entre os players locais.
const std::vector<std::string> playerNames{
   "falcon1", "falcon2", "falcon3", "falcon4"
};

// Banco de elevacao (compartilhado com a single-thread -- o tile e do
// CENARIO, que e o mesmo nas duas). Tem de bater com o 'terrain:' do .epp.
const std::string terrainDir{"./shared/data/terrain/srtm/"};
const std::string terrainTile{"S23W043"};

void printBanner(const int numTcThreads)
{
   std::cout << "=== python-flight ===" << std::endl;
   std::cout << "O multi-thread inteiro, com as LEIS DE VOO em Python: a arvore chama"
             << " ( PyDecide ), que roda decide(obs) de um .py dentro da fase 3 do frame"
             << std::endl;
   std::cout << "Politica editavel em ./src/poc/python-flight/configs/policy/"
             << " -- patrol.py, evade.py, support.py, rtb.py (edite e rode de novo,"
             << " sem recompilar)" << std::endl;
   std::cout << "Em C++ continuam so as CONDICOES da arvore e a rede de seguranca de"
             << " altitude -- ver configs/flight_tree_python.xml" << std::endl;
   std::cout << "Pool nativo de threads T/C: " << numTcThreads
             << " -- os quatro scripts decidem em paralelo, serializados pelo GIL"
             << std::endl;
   std::cout << "Tacview Real-Time Telemetry na porta 1237 -- objetos com Name=C310"
             << std::endl;
   std::cout << "Ctrl+C encerra." << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
   // Nao depende de mais nada -- pode vir antes de tudo.
   mixr::xlog::init("./src/poc/python-flight/data/logs/python-flight.log");

   app::Options defaults;
   defaults.templatePath  = "./src/poc/python-flight/configs/scenario.epp.in";
   defaults.generatedPath = "./src/poc/python-flight/configs/scenario.generated.epp";

   const app::Options opts{app::parseCommandLine(argc, argv, defaults)};

   // O log de transicao carrega o numero da thread, que depende do
   // escalonador -- fora do modo comparavel.
   if (opts.isDeterministic()) mixr::xlog::setLoggingEnabled(false);

   // O .epp nomeia o .hgt; garantir que ele existe e esta integro ANTES do
   // parse (o SrtmHgtFile nao le .gz e valida o tamanho em bytes).
   app::ensureTerrainData(terrainDir, terrainTile);

   const int numTcThreads{
      app::generateScenario(opts.templatePath, opts.generatedPath, opts.threadsOverride)};

   mixr::simulation::Station* const station{app::buildStation(opts.generatedPath)};
   mixr::xclock::ClockStation* const clockStation{app::clockStationOf(station)};
   mixr::linkage::IoHandler* const ioHandler{app::ioHandlerOf(station)};
   mixr::xtacview::TacviewOutput* const tacviewOutput{app::tacviewOutputOf(station)};

   app::primeStation(station);

   mixr::models::WorldModel* const worldModel{app::worldModelOf(station)};
   const app::Fleet fleet{app::collectFleet(worldModel, playerNames)};
   app::applyCruiseThrottle(fleet, cruiseThrottle);

   int rc{};
   if (opts.isDeterministic()) {
      rc = app::runDeterministic(station, fleet, opts.deterministicFrames, opts.parallelDecision);
      // Antes do SHUTDOWN, para o relatorio ver o regime estacionario e
      // nao o processo ja desmontado (ver app/MetaObjectReport.hpp).
      app::printMetaObjectReport();
   } else {
      printBanner(numTcThreads);
      app::runRealTime(station, fleet, worldModel, clockStation, ioHandler, tacviewOutput);
      std::cout << "=== fim ===" << std::endl;
   }

   // ORDEM importa aqui -- ver app/Shutdown.hpp. Cala a thread de tempo
   // critico nativa ANTES do SHUTDOWN_EVENT: sem isso, ela corre contra o
   // teardown da Simulation (auto-deadlock, ver xclock/ClockStation.hpp) e
   // pode seguir enfileirando registros numa fila sem teto depois que
   // ninguem mais a drena.
   app::quiesceTimeCritical(station, clockStation);
   app::shutdownStation(station);
   return rc;
}
