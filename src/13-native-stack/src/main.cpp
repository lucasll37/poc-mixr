//
// poc/13-native-stack
//
// MESMO cenario da poc/12 -- 4 avioes patrulhando, 1 intruso, quem detecta
// avisa os outros pelo datalink e eles vao apoiar -- com a regra invertida:
// aqui se HERDA do MIXR tudo o que o framework ja tem pronto.
//
// O que sumiu em relacao a poc/12 (e passou a ser do framework):
//
//    xair::Airplane           -> ( Aircraft )
//    xair::JsbsimFlightModel  -> ( JSBSimModel )
//    xair::FlightDirector     -> ( Autopilot )
//    xair::ProximitySensor    -> ( Gimbal/Antenna + Tws + AirTrkMgr )
//    xair::AlertRadio         -> ( AlertDatalink : models::Datalink )
//    xair::FlightAgent        -> ( SimAgent )
//
// O que sobrou de nosso: as pecas do UBF (percepcao/decisao/atuacao, que o
// framework nao traz prontas), a arvore do BehaviorTree.CPP e a carga util
// da mensagem de datalink.
//
// DIFERENCA DE COMPORTAMENTO QUE VEM COM A ESCOLHA: o SimAgent nativo
// deriva de ubf::Agent, cujo ciclo roda em updateData() -- ou seja, a
// decisao acontece na thread de BACKGROUND (na taxa daquele laco), e nao na
// fase 3 do frame de tempo critico como na poc/12 (que precisou de um
// AgentTC proprio justamente por isso).
//
// Opcoes de linha de comando: iguais as das pocs 11/12
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
#include "app/Options.hpp"
#include "app/RealTimeRun.hpp"
#include "app/ScenarioTemplate.hpp"
#include "app/StationBuilder.hpp"

#include "xnative/Log.hpp"

#include "mixr/simulation/Station.hpp"

#include "mixr/base/Component.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

const double cruiseThrottle{0.95};

const std::vector<std::string> playerNames{
   "falcon1", "falcon2", "falcon3", "falcon4", "bandit1"
};

void printBanner(const int numTcThreads)
{
   std::cout << "=== poc/13-native-stack ===" << std::endl;
   std::cout << "Mesmo cenario da poc/12, com a pilha NATIVA: Aircraft + JSBSimModel +"
             << " Autopilot + radar (Antenna/Tws/AirTrkMgr) + Datalink + SimAgent"
             << std::endl;
   std::cout << "De proprio sobraram so as pecas do UBF, a arvore e a carga do datalink"
             << std::endl;
   std::cout << "Pool nativo de threads T/C: " << numTcThreads
             << " (a decisao, porem, roda no laco de background -- SimAgent e um ubf::Agent)"
             << std::endl;
   std::cout << "Tacview Real-Time Telemetry na porta 1234 -- objetos com Name=C310" << std::endl;
   std::cout << "Ctrl+C encerra." << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
   app::Options defaults;
   defaults.templatePath  = "./src/13-native-stack/configs/scenario.epp.in";
   defaults.generatedPath = "./src/13-native-stack/configs/scenario.generated.epp";

   const app::Options opts{app::parseCommandLine(argc, argv, defaults)};

   // O log de transicao carrega o numero da thread, que depende do
   // escalonador -- fora do modo comparavel.
   if (opts.isDeterministic()) mixr::xnative::setLoggingEnabled(false);

   const int numTcThreads{
      app::generateScenario(opts.templatePath, opts.generatedPath, opts.threadsOverride)};

   mixr::simulation::Station* const station{app::buildStation(opts.generatedPath)};
   mixr::xclock::ClockStation* const clockStation{app::clockStationOf(station)};

   app::primeStation(station);

   mixr::models::WorldModel* const worldModel{app::worldModelOf(station)};
   const app::Fleet fleet{app::collectFleet(worldModel, playerNames)};
   app::applyCruiseThrottle(fleet, cruiseThrottle);

   int rc{};
   if (opts.isDeterministic()) {
      rc = app::runDeterministic(station, fleet, opts.deterministicFrames);
   } else {
      printBanner(numTcThreads);
      app::runRealTime(station, fleet, worldModel, clockStation);
      std::cout << "=== fim ===" << std::endl;
   }

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return rc;
}
