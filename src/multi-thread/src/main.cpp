//
// multi-thread
//
// O single-thread INTEIRO, com UMA unica diferenca: o agente do UBF.
//
//    single-thread:  ( SimAgent )       nativo, componente da STATION,
//                            ator por nome, decide em updateData()
//                            -> thread de BACKGROUND, na taxa daquele laco
//
//    multi-thread:   ( FlightAgentTC )  nosso, componente do PLAYER,
//                            ator = container, decide na fase 3
//                            -> thread de TEMPO CRITICO, todo frame
//
// Todo o resto e identico: mesma pilha nativa (Aircraft + JSBSimModel +
// Autopilot + Antenna/Tws/AirTrkMgr + Datalink), mesmo cenario, mesmos
// numeros, mesma arvore de comportamento, mesmas classes de UBF.
//
// O QUE ISSO MUDA NA PRATICA (ver README, secao "Determinismo"):
//
//   * a decisao passa a ser parte do frame, entao ela acontece na taxa do
//     tempo critico (50 Hz) e nao na do laco de background (10 Hz);
//   * o determinismo deixa de depender do laco de app/RealTimeRun.cpp: e o
//     frame que ordena tudo. La updateData() so drena o gravador (Tacview).
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

// Banco de elevacao (compartilhado com a single-thread -- o tile e do
// CENARIO, que e o mesmo nas duas). Tem de bater com o 'terrain:' do .epp.
const std::string terrainDir{"./shared/data/terrain/srtm/"};
const std::string terrainTile{"S23W043"};

void printBanner(const int numTcThreads)
{
   std::cout << "=== multi-thread ===" << std::endl;
   std::cout << "O single-thread inteiro, trocando ( SimAgent ) por ( FlightAgentTC ): a decisao"
             << " saiu do background e entrou na fase 3 do frame" << std::endl;
   std::cout << "Pilha NATIVA identica a da single-thread: Aircraft + JSBSimModel + Autopilot +"
             << " radar (Antenna/Tws/AirTrkMgr) + Datalink" << std::endl;
   std::cout << "Pool nativo de threads T/C: " << numTcThreads
             << " -- e a decisao roda DENTRO dele (compare 'dec' e 'thr' no status)"
             << std::endl;
   std::cout << "Tacview Real-Time Telemetry na porta 1234 -- objetos com Name=C310" << std::endl;
   std::cout << "Ctrl+C encerra." << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
   // Nao depende de mais nada -- pode vir antes de tudo.
   mixr::xlog::init("./src/multi-thread/data/logs/multi-thread.log");

   app::Options defaults;
   defaults.templatePath  = "./src/multi-thread/configs/scenario.epp.in";
   defaults.generatedPath = "./src/multi-thread/configs/scenario.generated.epp";

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
