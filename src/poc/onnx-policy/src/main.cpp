//
// onnx-policy
//
// O multi-thread INTEIRO, com UMA unica diferenca: QUEM decide.
//
//    multi-thread:  ( BtBehavior treeFile: flight_tree.xml )
//                       -> a arvore de regras escritas a mao (Patrol, Evade,
//                          Support, RTB) -- C++, em domain/
//
//    onnx-policy:   ( BtBehavior treeFile: configs/flight_tree_onnx.xml )
//                       -> uma REDE NEURAL, carregada de um .onnx e inferida
//                          DENTRO da fase 3 do frame pelo no OnnxPolicy
//                          (models/flight/src/bt/nodes/OnnxPolicyAction.cpp,
//                          motor em shared/xinfer)
//
// Todo o resto e identico: mesma pilha nativa (Aircraft + JSBSimModel +
// Autopilot + Antenna/Tws/AirTrkMgr + Datalink), mesmo agente
// ( FlightAgentTC ) decidindo na fase 3, mesmo cenario, mesmo plugin
// (libflight_tc.so). Trocar as regras pela rede foi trocar o 'treeFile:' --
// nao ha uma linha de C++ nova nesta poc.
//
// O QUE ISSO DEMONSTRA, e que nenhuma outra poc demonstra:
//
//   * a inferencia acontece NO FRAME, nao num processo Python do outro lado
//     de uma caixa de correio (esse caminho e o src/rl, e tem um frame de
//     latencia): a rede le o WorldView deste frame e comanda neste frame;
//   * ela nao quebra o determinismo -- 4 aeronaves inferindo em paralelo no
//     pool de tempo critico, compartilhando UMA sessao do ONNX Runtime,
//     dao dumps byte-identicos com 1, 2 e 4 threads (make check-onnx-policy);
//   * a rede NAO tem a ultima palavra: o ( AltitudeSafetyBehavior vote: 90 )
//     do mesmo UbfArbiter sobrepoe o ( BtBehavior vote: 50 ) quando o piso
//     anti-CFIT e furado. Uma politica ruim nao poe a aeronave no chao.
//
// A rede e a politica vendorizada em configs/policy_barrier.onnx -- ver
// README.md e tools/train_policy.py.
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

// bandit1 (o intruso) e um player NETWORKED (recebido via 'networks:', ver
// scenario.epp.in), nunca declarado localmente -- por isso nao entra nesta
// lista: Fleet::collectFleet() aborta o processo se um nome dela nao
// existir entre os players locais.
const std::vector<std::string> playerNames{
   "falcon1", "falcon2", "falcon3", "falcon4"
};

// Banco de elevacao (compartilhado com as outras pocs -- o tile e do
// CENARIO, que e o mesmo nas tres). Tem de bater com o 'terrain:' do .epp.
const std::string terrainDir{"./shared/data/terrain/srtm/"};
const std::string terrainTile{"S23W043"};

void printBanner(const int numTcThreads)
{
   std::cout << "=== onnx-policy ===" << std::endl;
   std::cout << "O multi-thread inteiro, trocando a arvore de regras por uma REDE NEURAL:"
             << " quem comanda os falcons e o .onnx de configs/policy_barrier.onnx" << std::endl;
   std::cout << "A inferencia roda DENTRO da fase 3 do frame (no OnnxPolicy + shared/xinfer),"
             << " sem Python no processo" << std::endl;
   std::cout << "Pool nativo de threads T/C: " << numTcThreads
             << " -- as 4 aeronaves inferem em paralelo (compare 'dec' e 'thr' no status)"
             << std::endl;
   std::cout << "O ( AltitudeSafetyBehavior vote: 90 ) continua acima da rede (voto 50):"
             << " o piso anti-CFIT nao depende da politica" << std::endl;
   std::cout << "Tacview Real-Time Telemetry na porta 1238 -- objetos com Name=C310" << std::endl;
   std::cout << "Ctrl+C encerra." << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
   // Nao depende de mais nada -- pode vir antes de tudo.
   mixr::xlog::init("./src/poc/onnx-policy/data/logs/onnx-policy.log");

   app::Options defaults;
   defaults.templatePath  = "./src/poc/onnx-policy/configs/scenario.epp.in";
   defaults.generatedPath = "./src/poc/onnx-policy/configs/scenario.generated.epp";

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

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return rc;
}
