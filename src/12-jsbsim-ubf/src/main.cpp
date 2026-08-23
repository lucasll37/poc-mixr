//
// poc/12-jsbsim-ubf
//
// Continuacao da poc/11. Todos os players continuam sendo classes proprias
// (mixr::xair), mas agora:
//
//   * a dinamica e 6-DOF de verdade, falando DIRETO com a JSBSim::FGFDMExec
//     (xair::JsbsimFlightModel) -- sem herdar models::DynamicsModel nem usar
//     o models::JSBSimModel do framework;
//   * a decisao passa pelo UBF NATIVO (xair::FlightAgent -> UbfArbiter ->
//     AltitudeSafetyBehavior | BtBehavior), com o BehaviorTree.CPP como
//     politica interna de um dos comportamentos;
//   * os avioes se influenciam: quem detecta o intruso transmite um alerta
//     (evento nativo, xair::AlertRadio) e os outros mudam de comportamento;
//   * o Tacview recebe Name=F-4E (modelo) + CallSign/Pilot, entao os
//     objetos aparecem como AERONAVES, nao como blocos genericos.
//
// Fases do frame de tempo critico usadas por esta poc:
//
//    fase 0  FlightDirector::dynamics()   -> deflexoes de superficie
//            JsbsimFlightModel::dynamics()-> passo do 6-DOF + escrita no Player
//    fase 1  AlertRadio::transmit()       -> alerta para os outros avioes
//    fase 2  ProximitySensor::receive()   -> varredura
//            AlertRadio::receive()        -> promove o alerta recebido
//    fase 3  FlightAgent::controller()    -> percepcao/decisao/atuacao (UBF)
//
// Opcoes de linha de comando (iguais as da poc/11):
//
//   -f <arquivo>          template .epp alternativo
//   -threads <N>          forca numTcThreads
//   -deterministic <N>    N frames de passo fixo, sem thread periodica, com
//                         dump de estado -- 'make check-jsbsim-ubf' compara
//                         execucoes com 1, 2 e 4 threads.
//

#include "mixr_factory.hpp"

#include "ubf/FlightAgent.hpp"
#include "xair/Airplane.hpp"
#include "xair/AlertRadio.hpp"
#include "xair/FlightDirector.hpp"
#include "xair/JsbsimFlightModel.hpp"
#include "xair/ProximitySensor.hpp"
#include "xair/runtime_utils.hpp"

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

const int bgRate{10};
const int statusEveryNFrames{20};   // status a cada 2 s

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

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

mixr::xair::Airplane* findAirplane(mixr::models::WorldModel* wm, const char* const name)
{
   mixr::base::PairStream* players{wm->getPlayers()};
   mixr::xair::Airplane* result{};
   if (players != nullptr) {
      mixr::base::Pair* p{players->findByName(name)};
      if (p != nullptr) result = dynamic_cast<mixr::xair::Airplane*>(p->object());
      players->unref();
   }
   return result;
}

void printStatus(const std::vector<mixr::xair::Airplane*>& fleet, const double elapsedSec)
{
   std::ostringstream oss;
   oss << std::fixed;
   oss << "[t=" << std::setprecision(0) << elapsedSec << "s]" << std::endl;

   for (const auto airplane : fleet) {
      if (airplane == nullptr) continue;

      const auto model = airplane->getFlightModel();
      const auto sensor = airplane->getProximitySensor();
      const auto radio = airplane->getAlertRadio();

      oss << "   " << std::setw(8) << std::left
          << (airplane->getName() != nullptr ? airplane->getName()->getString() : "?")
          << std::right << std::setprecision(0)
          << " alt=" << std::setw(5) << airplane->getAltitudeM() << "m"
          << " hdg=" << std::setw(3) << airplane->getHeadingD() << "deg"
          << " roll=" << std::setw(4) << airplane->getRollD() << "deg"
          << " spd=" << std::setw(3) << airplane->getTotalVelocityKts() << "kt";

      if (model != nullptr) {
         oss << std::setprecision(2)
             << " mach=" << model->getMach()
             << std::setprecision(1)
             << " g=" << model->getGLoad()
             << " aoa=" << model->getAlphaDeg() << "deg"
             << std::setprecision(0)
             << " fuel=" << (model->getFuelFraction() * 100.0) << "%";
      }

      // Sem FlightAgent nao ha decisao nenhuma: o FlightDirector so mantem
      // o estado inicial declarado no .epp (caso do intruso).
      const bool hasAgent{airplane->findByType(typeid(mixr::xair::FlightAgent)) != nullptr};
      oss << " bt=" << std::setw(11) << std::left
          << (hasAgent ? airplane->getBehaviorLabel() : std::string("(sem agente)"))
          << std::right;

      // Diagnostico da lei de controle (o que o FlightDirector pediu x o
      // que a aeronave esta fazendo) -- foi assim que os ganhos e os
      // sinais de convencao do JSBSim foram acertados.
      const auto director = airplane->getFlightDirector();
      if (director != nullptr) {
         oss << std::setprecision(0)
             << " cmd(hdg=" << director->getCommand().headingDeg
             << ",alt=" << director->getCommand().altitudeM
             << ",spd=" << director->getCommand().speedKts << ")"
             << " bank*=" << director->getCommandedBankDeg()
             << std::setprecision(2)
             << " ail=" << director->getAileron()
             << " ele=" << director->getElevator()
             << " thr=" << director->getThrottle();
      }

      if (sensor != nullptr) {
         const auto contact = sensor->getContact();
         if (contact.valid) {
            oss << " contato=" << contact.name << "@"
                << std::setprecision(1) << (contact.rangeM / 1852.0) << "NM";
         }
      }
      if (radio != nullptr) {
         const auto alert = radio->getAlert();
         if (alert.valid) {
            oss << " alerta<-" << alert.senderName << "(" << alert.contactName << ")";
         }
      }

      oss << std::endl;
   }

   std::cout << oss.str();
}

//------------------------------------------------------------------------------
// Dump deterministico -- so estado de simulacao, sem nada que dependa do
// escalonador (ver a poc/11: mesma tecnica, mesmo alvo de Makefile).
//------------------------------------------------------------------------------
void printDeterministicDump(const std::vector<mixr::xair::Airplane*>& fleet, const long frame)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(9);

   for (const auto airplane : fleet) {
      if (airplane == nullptr) continue;

      const mixr::base::Vec3d pos{airplane->getPosition()};
      const auto model = airplane->getFlightModel();
      const auto sensor = airplane->getProximitySensor();
      const auto radio = airplane->getAlertRadio();

      oss << "frame=" << frame
          << " player=" << (airplane->getName() != nullptr ? airplane->getName()->getString() : "?")
          << " n=" << pos[mixr::models::Player::INORTH]
          << " e=" << pos[mixr::models::Player::IEAST]
          << " alt=" << airplane->getAltitudeM()
          << " hdg=" << airplane->getHeadingD()
          << " roll=" << airplane->getRollD()
          << " pitch=" << airplane->getPitchD()
          << " spd=" << airplane->getTotalVelocity()
          << " mach=" << (model != nullptr ? model->getMach() : 0.0)
          << " fuel=" << (model != nullptr ? model->getFuelLbs() : 0.0)
          << " bt=" << airplane->getBehaviorLabel()
          << " contact=" << ((sensor != nullptr && sensor->hasContact())
                              ? sensor->getContact().name : std::string("none"))
          << " alert=" << ((radio != nullptr && radio->hasAlert())
                            ? radio->getAlert().senderName : std::string("none"))
          << " sent=" << (radio != nullptr ? radio->getSentCount() : 0L)
          << " recv=" << (radio != nullptr ? radio->getReceivedCount() : 0L)
          << std::endl;
   }

   std::cout << oss.str();
}

int runDeterministic(mixr::simulation::Station* const station,
                     const std::vector<mixr::xair::Airplane*>& fleet,
                     const long frames)
{
   const double dt{1.0 / static_cast<double>(station->getTimeCriticalRate())};

   std::cout << "[deterministic] frames=" << frames
             << " dt=" << std::fixed << std::setprecision(9) << dt
             << " (1/tcRate, o MESMO dt do PeriodicThread no modo normal)" << std::endl;

   for (long frame = 1; frame <= frames; ++frame) {
      station->tcFrame(dt);
      station->updateData(dt);
      if (frame % 100 == 0 || frame == frames) printDeterministicDump(fleet, frame);
   }
   return 0;
}

} // namespace

int main(int argc, char* argv[])
{
   std::string templatePath = "./poc/12-jsbsim-ubf/configs/scenario.epp.in";
   std::string generatedPath = "./poc/12-jsbsim-ubf/configs/scenario.generated.epp";

   long deterministicFrames{};
   int threadsOverride{};

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

   if (deterministicFrames > 0) mixr::xair::setLoggingEnabled(false);

   const int numTcThreads{generateScenario(templatePath, generatedPath, threadsOverride)};

   mixr::simulation::Station* station{buildStation(generatedPath)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
   if (worldModel == nullptr) {
      std::cerr << "No WorldModel found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const char* const names[5]{"falcon1", "falcon2", "falcon3", "falcon4", "bandit1"};
   std::vector<mixr::xair::Airplane*> fleet;
   for (const char* const name : names) {
      mixr::xair::Airplane* const airplane{findAirplane(worldModel, name)};
      if (airplane == nullptr) {
         std::cerr << "player '" << name << "' nao encontrado (ou nao e um Airplane)!" << std::endl;
         std::exit(EXIT_FAILURE);
      }
      fleet.push_back(airplane);
   }

   if (deterministicFrames > 0) {
      const int rc{runDeterministic(station, fleet, deterministicFrames)};
      station->event(mixr::base::Component::SHUTDOWN_EVENT);
      station->unref();
      return rc;
   }

   std::cout << "=== poc/12-jsbsim-ubf ===" << std::endl;
   std::cout << "5 cacas F-4 (JSBSim 6-DOF direto), 4 azuis com UBF+BehaviorTree e 1 intruso"
             << std::endl;
   std::cout << "Quem detectar o intruso transmite um alerta; os outros vao apoiar (bt=SUPPORT)"
             << std::endl;
   std::cout << "Pool nativo de threads T/C: " << numTcThreads << std::endl;
   std::cout << "Tacview Real-Time Telemetry na porta 1234 -- objetos com Name=F-4E" << std::endl;
   std::cout << "Ctrl+C encerra." << std::endl;

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double wallTimeElapsed{};
   const double startTime{mixr::base::getComputerTime()};
   long frameCount{};

   while (!g_stopRequested) {

      station->updateData(dt);

      frameCount += 1;
      if (frameCount % statusEveryNFrames == 0) {
         printStatus(fleet, static_cast<double>(frameCount) * dt);
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
