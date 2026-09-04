//
// src/poc/bandit-dis
//
// O bandit1 num processo proprio: pilotado por joystick fisico (ou pelo
// Autopilot scripted, sem hardware conectado) e emitido via DIS nativo do
// MIXR para quem quiser recebe-lo -- hoje, as pocs single-thread/
// multi-thread, que perderam o bandit1 LOCAL e passaram a receber este
// aqui so pela rede. Ver o cabecalho de configs/scenario.epp e a secao
// 'src/poc/bandit-dis' do CLAUDE.md para o desenho completo.
//
// Sem -f/-threads/-deterministic: essa poc nao tem CLI. O cenario e fixo
// (configs/scenario.epp), e joystick/rede nao sao deterministicos por
// natureza -- nao ha o que comparar num modo '-deterministic' aqui.
//
// ESTE ARQUIVO SO ORQUESTRA -- mesmo espirito de single-thread/multi-thread,
// so que mais curto: nao ha Fleet (um player so), nem ClockStation.
//

#include "app/RealTimeRun.hpp"
#include "app/Shutdown.hpp"
#include "app/StationBuilder.hpp"
#include "app/TerrainData.hpp"

#include "xlog/Log.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/Simulation.hpp"

#include "mixr/models/player/air/AirVehicle.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"

#include <iostream>
#include <string>

namespace {

const std::string scenarioPath{"./src/poc/bandit-dis/configs/scenario.epp"};

// Mesma correcao ja documentada em app/Fleet.hpp das pocs gemeas: o
// autopilot do c310 fecha malha de RUMO e ALTITUDE, mas nao de VELOCIDADE
// (o c310ap.xml so DECLARA ap/airspeed_hold, sem canal que a implemente).
// Sem manete fixo o bandit1 perderia velocidade e estolaria -- mesma
// aeronave, mesmo problema, tem de levar a mesma correcao aqui.
const double cruiseThrottle{0.95};

const std::string terrainDir{"./shared/data/terrain/srtm/"};
const std::string terrainTile{"S23W043"};

void applyCruiseThrottle(mixr::simulation::Station* const station)
{
   mixr::simulation::Simulation* const sim{station->getSimulation()};
   if (sim == nullptr) return;

   mixr::base::PairStream* const players{sim->getPlayers()};
   if (players == nullptr) return;

   mixr::base::Pair* const p{players->findByName("bandit1")};
   const auto air = (p != nullptr) ? dynamic_cast<mixr::models::AirVehicle*>(p->object()) : nullptr;
   if (air != nullptr) air->setThrottles(&cruiseThrottle, 1);

   players->unref();
}

void printBanner()
{
   std::cout << "=== bandit-dis ===" << std::endl;
   std::cout << "bandit1 num processo proprio -- joystick fisico (ou Autopilot scripted, sem"
             << " hardware) + emissao DIS nativa (mixr::dis)" << std::endl;
   std::cout << "Tacview Real-Time Telemetry na porta 1235 -- objetos com Name=F-16C" << std::endl;
   std::cout << "Ctrl+C encerra." << std::endl;
}

} // namespace

int main()
{
   mixr::xlog::init("./src/poc/bandit-dis/data/logs/bandit-dis.log");

   app::ensureTerrainData(terrainDir, terrainTile);

   mixr::simulation::Station* const station{app::buildStation(scenarioPath)};
   mixr::linkage::IoHandler* const ioHandler{app::ioHandlerOf(station)};

   app::primeStation(station);
   applyCruiseThrottle(station);

   printBanner();
   app::runRealTime(station, ioHandler);
   std::cout << "=== fim ===" << std::endl;

   // ORDEM importa aqui -- ver app/Shutdown.hpp. Cala a thread de tempo
   // critico nativa ANTES do SHUTDOWN_EVENT (mesmo risco documentado em
   // xclock/ClockStation.hpp, medido nas gemeas single-thread/multi-thread).
   app::quiesceTimeCritical(station);
   app::shutdownStation(station);
   return 0;
}
