//
// poc/11-custom-models
//
// Todos os players deste cenario sao classes proprias (mixr::xdrone):
// Drone (player), DroneDynamics (fisica 3-DOF escrita do zero), FuelSystem
// e ProximitySensor (subsistemas) e BtPilot (arvore de comportamento
// BehaviorTree.CPP ticada DENTRO do frame de tempo critico). Do framework
// ficam so a infraestrutura -- Station/WorldModel, o pool nativo de threads
// (numTcThreads), o DataRecorder e o TacviewOutput de shared/xtacview.
//
// Este main.cpp e deliberadamente MAGRO: ele nao comanda aeronave, nao tica
// arvore e nao monta stream ACMI. Tudo isso acontece dentro dos objetos
// declarados no .epp, nas fases certas do frame:
//
//    fase 0  DroneDynamics::dynamics()   + FuelSystem::dynamics()
//    fase 2  ProximitySensor::receive()
//    fase 3  BtPilot::process()          -> tick da arvore
//
// O laco abaixo so faz duas coisas: station->updateData(dt) (que drena a
// fila do gravador para a cadeia do Tacview) e imprimir o status --
// inclusive em QUAL thread do pool cada drone foi ticado, que e a forma
// honesta de observar o round-robin nativo (o MIXR nao expoe os handles do
// pool, mas o nosso codigo roda nessas threads e pode se identificar).
//
// Opcoes de linha de comando:
//
//   -f <arquivo>          template .epp alternativo
//   -threads <N>          forca numTcThreads (limitado a getNumProcessors()-1)
//   -deterministic <N>    roda N frames de PASSO FIXO na propria thread do
//                         main (sem createTimeCriticalProcess) e despeja o
//                         estado com 9 casas decimais, sem nada que dependa
//                         do escalonador -- e o modo usado por
//                         'make check-custom-models' para provar que o
//                         resultado nao muda com 1, 2 ou 4 threads.
//
// Por que passo fixo nao e batota: no Linux o PeriodicThread nativo tambem
// usa dt = 1/rate CONSTANTE (o flag de delta variavel e so do Windows e
// nasce desligado); o relogio de parede decide apenas QUANDO o frame roda.
// O '-deterministic' troca so o disparador do frame, nao o passo.
//

#include "mixr_factory.hpp"

#include "xdrone/BtPilot.hpp"
#include "xdrone/Drone.hpp"
#include "xdrone/DroneDynamics.hpp"
#include "xdrone/FuelSystem.hpp"
#include "xdrone/ProximitySensor.hpp"
#include "xdrone/runtime_utils.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

const int bgRate{10};              // Hz do laco de background (tempo real)
const int statusEveryNFrames{20};  // status a cada 2 s

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

// -----------------------------------------------------------------------
// @NUM_TC_THREADS@ -- unica coisa que nao da para deixar estatica no EDL:
// o teto de threads e getNumProcessors()-1, avaliado no momento em que o
// slot e preenchido (Simulation::setSlotNumTcThreads()), e um valor acima
// disso faz o slot ser REJEITADO -- a simulacao sobe monothread e so
// aparece um erro de parse. Mesmo mecanismo da poc/05.
//
// Limitamos tambem ao numero de players com pilha completa: mais threads
// do que players nao acelera nada.
// -----------------------------------------------------------------------
int generateScenario(const std::string& templatePath, const std::string& outPath,
                     const int threadsOverride)
{
   std::ifstream in(templatePath);
   if (!in.good()) {
      std::cerr << "[main] nao consegui ler " << templatePath << std::endl;
      std::exit(EXIT_FAILURE);
   }
   std::ostringstream buf;
   buf << in.rdbuf();
   std::string text{buf.str()};

   const unsigned int hwThreads{std::thread::hardware_concurrency()};
   const int maxByCpu{static_cast<int>(hwThreads > 1 ? hwThreads - 1 : 1)};
   const int wanted{(threadsOverride > 0) ? threadsOverride : std::min(maxByCpu, 4)};

   // Teto do framework: Simulation::setSlotNumTcThreads() REJEITA um valor
   // acima de getNumProcessors()-1 (e a simulacao sobe monothread, com um
   // erro de parse). Melhor limitar aqui do que gerar um .epp invalido.
   const int numTcThreads{std::max(1, std::min(wanted, maxByCpu))};

   const std::string placeholder{"@NUM_TC_THREADS@"};
   const std::string replacement{std::to_string(numTcThreads)};
   std::size_t pos{};
   while ((pos = text.find(placeholder, pos)) != std::string::npos) {
      text.replace(pos, placeholder.size(), replacement);
      pos += replacement.size();
   }

   std::ofstream out(outPath);
   out << text;

   std::cout << "[main] numTcThreads=" << numTcThreads
             << " (hardware_concurrency=" << hwThreads << ")" << std::endl;
   return numTcThreads;
}

mixr::simulation::Station* buildStation(const std::string& filename)
{
   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, mixrFactory, &num_errors)};
   if (num_errors > 0) {
      std::cerr << "File: " << filename << ", number of errors: " << num_errors << std::endl;
      std::exit(EXIT_FAILURE);
   }
   if (obj == nullptr) {
      std::cerr << "Invalid configuration file, no objects defined!" << std::endl;
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
      std::cerr << "Invalid configuration file!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   return station;
}

mixr::xdrone::Drone* findDrone(mixr::models::WorldModel* wm, const char* const name)
{
   mixr::base::PairStream* players{wm->getPlayers()};
   mixr::xdrone::Drone* result{};
   if (players != nullptr) {
      mixr::base::Pair* p{players->findByName(name)};
      if (p != nullptr) result = dynamic_cast<mixr::xdrone::Drone*>(p->object());
      players->unref();
   }
   return result;
}

void printStatus(const std::vector<mixr::xdrone::Drone*>& drones, const double elapsedSec)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(0);
   oss << "[t=" << elapsedSec << "s]" << std::endl;

   for (const auto drone : drones) {
      if (drone == nullptr) continue;

      const auto pilot = drone->getBtPilot();
      const auto fuel = drone->getFuelSystem();
      const auto sensor = drone->getProximitySensor();

      // Posicao NED (metros do ponto de referencia do cenario) -- a mesma
      // referencia que o RtbPlan usa para a base (0,0).
      const mixr::base::Vec3d pos{drone->getPosition()};
      const double northNm{pos[mixr::models::Player::INORTH] / 1852.0};
      const double eastNm{pos[mixr::models::Player::IEAST] / 1852.0};

      oss << "   " << std::setw(7) << std::left
          << (drone->getName() != nullptr ? drone->getName()->getString() : "?")
          << std::right << std::setprecision(1)
          << " pos=" << std::setw(5) << northNm << "N/" << std::setw(5) << eastNm << "E NM"
          << std::setprecision(0)
          << " alt=" << std::setw(5) << drone->getAltitudeM() << "m"
          << " hdg=" << std::setw(3) << drone->getHeadingD() << "deg"
          << " spd=" << std::setw(3) << drone->getTotalVelocityKts() << "kt";

      if (fuel != nullptr) {
         oss << " fuel=" << std::setw(3) << (fuel->getFraction() * 100.0) << "%";
      } else {
         oss << " fuel=  --";
      }

      if (pilot != nullptr) {
         oss << " bt=" << std::setw(7) << std::left << pilot->getState() << std::right
             << " ticks=" << std::setw(5) << pilot->getTickCount()
             << " thread=" << pilot->getLastThreadTag()
             << " cpu=" << std::setw(2) << pilot->getLastCpu();
      } else {
         oss << " bt=    --  (sem BtPilot: voa so pelos comandos do .epp)";
      }

      if (sensor != nullptr) {
         const auto contact = sensor->getContact();
         if (contact.valid) {
            oss << "  contato=" << contact.name
                << "@" << std::setprecision(1) << (contact.rangeM / 1852.0) << "NM"
                << "/" << std::setprecision(0) << contact.relBearingDeg << "deg";
         }
      }

      oss << std::endl;
   }

   std::cout << oss.str();
}

//------------------------------------------------------------------------------
// Dump determinístico -- usado por '-deterministic N'.
//
// Imprime SÓ estado de simulação, com precisão alta e sem nada que dependa
// do relógio de parede ou do escalonador (thread/núcleo ficam de fora de
// propósito). Duas execuções com o mesmo cenário têm que produzir bytes
// idênticos -- é assim que o determinismo desta poc é verificado, não por
// inspeção visual.
//------------------------------------------------------------------------------
void printDeterministicDump(const std::vector<mixr::xdrone::Drone*>& drones, const long frame)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(9);

   for (const auto drone : drones) {
      if (drone == nullptr) continue;

      const mixr::base::Vec3d pos{drone->getPosition()};
      const auto pilot = drone->getBtPilot();
      const auto fuel = drone->getFuelSystem();
      const auto sensor = drone->getProximitySensor();

      oss << "frame=" << frame
          << " player=" << (drone->getName() != nullptr ? drone->getName()->getString() : "?")
          << " n=" << pos[mixr::models::Player::INORTH]
          << " e=" << pos[mixr::models::Player::IEAST]
          << " alt=" << drone->getAltitudeM()
          << " hdg=" << drone->getHeadingD()
          << " roll=" << drone->getRollD()
          << " spd=" << drone->getTotalVelocity()
          << " fuel=" << (fuel != nullptr ? fuel->getFuelKg() : 0.0)
          << " bt=" << (pilot != nullptr ? pilot->getState() : std::string("--"))
          << " ticks=" << (pilot != nullptr ? pilot->getTickCount() : 0L);

      if (sensor != nullptr) {
         const auto contact = sensor->getContact();
         oss << " contact=" << (contact.valid ? contact.name : std::string("none"))
             << " contactRange=" << contact.rangeM;
      }
      oss << std::endl;
   }

   std::cout << oss.str();
}

// Roda N frames de tempo crítico com dt FIXO, na própria thread do main
// (sem createTimeCriticalProcess). O pool de numTcThreads continua sendo
// usado dentro de Simulation::updateTC, então o caminho multithread é o
// mesmo do modo normal -- o que muda é só quem dispara o frame.
int runDeterministic(mixr::simulation::Station* const station,
                     const std::vector<mixr::xdrone::Drone*>& drones,
                     const long frames)
{
   const double dt{1.0 / static_cast<double>(station->getTimeCriticalRate())};

   std::cout << "[deterministic] frames=" << frames
             << " dt=" << std::fixed << std::setprecision(9) << dt
             << " (1/tcRate, o MESMO dt que o PeriodicThread passa no modo normal)"
             << std::endl;

   for (long frame = 1; frame <= frames; ++frame) {
      station->tcFrame(dt);
      station->updateData(dt);
      if (frame % 100 == 0 || frame == frames) printDeterministicDump(drones, frame);
   }
   return 0;
}

} // namespace

int main(int argc, char* argv[])
{
   std::string templatePath = "./poc/11-custom-models/configs/scenario.epp.in";
   std::string generatedPath = "./poc/11-custom-models/configs/scenario.generated.epp";

   long deterministicFrames{};   // > 0 => modo determinístico (passo fixo, sem thread periódica)
   int threadsOverride{};        // > 0 => força numTcThreads (para comparar 1 x N threads)

   for (int i = 1; i < argc; i++) {
      const std::string arg{argv[i]};
      if (arg == "-f" && (i + 1) < argc) {
         templatePath = argv[++i];
      } else if (arg == "-deterministic" && (i + 1) < argc) {
         deterministicFrames = std::stol(argv[++i]);
      } else if (arg == "-threads" && (i + 1) < argc) {
         threadsOverride = std::stoi(argv[++i]);
      }
   }

   std::signal(SIGINT, onSigint);

   if (deterministicFrames > 0) mixr::xdrone::setLoggingEnabled(false);

   const int numTcThreads{generateScenario(templatePath, generatedPath, threadsOverride)};

   mixr::simulation::Station* station{buildStation(generatedPath)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
   if (worldModel == nullptr) {
      std::cerr << "No WorldModel found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const char* const names[6]{"uav1", "uav2", "uav3", "uav4", "bogey1", "bogey2"};
   std::vector<mixr::xdrone::Drone*> drones;
   for (const char* const name : names) {
      mixr::xdrone::Drone* const drone{findDrone(worldModel, name)};
      if (drone == nullptr) {
         std::cerr << "player '" << name << "' nao encontrado (ou nao e um Drone)!" << std::endl;
         std::exit(EXIT_FAILURE);
      }
      drones.push_back(drone);
   }

   if (deterministicFrames > 0) {
      const int rc{runDeterministic(station, drones, deterministicFrames)};
      station->event(mixr::base::Component::SHUTDOWN_EVENT);
      station->unref();
      return rc;
   }

   std::cout << "=== poc/11-custom-models ===" << std::endl;
   std::cout << "Players: 6 (4 drones com pilha completa + 2 intrusos so com dinamica)"
             << std::endl;
   std::cout << "Classes proprias: Drone / DroneDynamics / FuelSystem / ProximitySensor / BtPilot"
             << std::endl;
   std::cout << "Pool nativo de threads T/C: " << numTcThreads
             << " (o campo 'thread=' abaixo mostra em qual delas cada arvore foi ticada)"
             << std::endl;
   std::cout << "Tacview Real-Time Telemetry na porta 1234 (bind 0.0.0.0; ver CLAUDE.md p/ WSL2)"
             << std::endl;
   std::cout << "Ctrl+C encerra." << std::endl;

   // A partir daqui a simulacao roda na(s) thread(s) de tempo critico
   // criadas pelo proprio framework.
   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double wallTimeElapsed{};
   const double startTime{mixr::base::getComputerTime()};
   long frameCount{};

   while (!g_stopRequested) {

      // Unica chamada obrigatoria do laco: drena a fila do DataRecorder
      // para a cadeia de OutputHandlers (e dai para o Tacview).
      station->updateData(dt);

      frameCount += 1;
      if (frameCount % statusEveryNFrames == 0) {
         printStatus(drones, static_cast<double>(frameCount) * dt);
      }

      wallTimeElapsed += dt;
      const double elapsedTime{mixr::base::getComputerTime() - startTime};
      const int sleepTime{static_cast<int>((wallTimeElapsed - elapsedTime) * 1000.0)};
      if (sleepTime > 0) mixr::base::msleep(sleepTime);
   }

   std::cout << "=== fim ===" << std::endl;

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
