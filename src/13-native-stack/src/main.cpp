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
// decisao acontece na thread de BACKGROUND (na taxa deste laco), e nao na
// fase 3 do frame de tempo critico como na poc/12 (que precisou de um
// AgentTC proprio justamente por isso).
//
// Opcoes de linha de comando: iguais as das pocs 11/12
//   -f <arquivo> | -threads <N> | -deterministic <N>
//

#include "mixr_factory.hpp"

#include "xnative/AlertDatalink.hpp"
#include "xnative/runtime_utils.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/Autopilot.hpp"
#include "mixr/models/system/OnboardComputer.hpp"
#include "mixr/models/system/trackmanager/TrackManager.hpp"
#include "mixr/models/Track.hpp"

#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/safe_ptr.hpp"
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
const int statusEveryNFrames{20};
const int maxTracks{20};

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

mixr::models::AirVehicle* findAircraft(mixr::models::WorldModel* wm, const char* const name)
{
   mixr::base::PairStream* players{wm->getPlayers()};
   mixr::models::AirVehicle* result{};
   if (players != nullptr) {
      mixr::base::Pair* p{players->findByName(name)};
      if (p != nullptr) result = dynamic_cast<mixr::models::AirVehicle*>(p->object());
      players->unref();
   }
   return result;
}

// Contato mais proximo do TrackManager nativo (mesma consulta que o
// FlightState faz -- aqui so para o status).
bool nearestTrack(mixr::models::AirVehicle* const air, std::string* const name, double* const rangeM)
{
   mixr::models::OnboardComputer* const obc{air->getOnboardComputer()};
   if (obc == nullptr) return false;

   mixr::models::TrackManager* const trkMgr{obc->getTrackManagerByName("twsTrkMgr")};
   if (trkMgr == nullptr) return false;

   mixr::base::safe_ptr<mixr::models::Track> tracks[maxTracks];
   const int n{trkMgr->getTrackList(tracks, maxTracks)};

   const mixr::models::Track* best{};
   for (int i = 0; i < n; i++) {
      const mixr::models::Track* const trk{tracks[i]};
      if (trk == nullptr) continue;
      // Mesmo filtro que o FlightState aplica: o radar nativo nao separa
      // amigo de inimigo, quem faz isso e a camada de decisao.
      const mixr::models::Player* const tgt{trk->getTarget()};
      if (tgt != nullptr && tgt->getSide() == air->getSide()) continue;
      if (best == nullptr || trk->getRange() < best->getRange()) best = trk;
   }
   if (best == nullptr) return false;

   const mixr::models::Player* const tgt{best->getTarget()};
   *name = (tgt != nullptr && tgt->getName() != nullptr) ? tgt->getName()->getString()
                                                         : ("trk" + std::to_string(best->getTrackID()));
   *rangeM = best->getRange();
   return true;
}

void printStatus(const std::vector<mixr::models::AirVehicle*>& fleet, const double elapsedSec)
{
   std::ostringstream oss;
   oss << std::fixed;
   oss << "[t=" << std::setprecision(0) << elapsedSec << "s]" << std::endl;

   for (const auto air : fleet) {
      if (air == nullptr) continue;

      oss << "   " << std::setw(8) << std::left
          << (air->getName() != nullptr ? air->getName()->getString() : "?")
          << std::right << std::setprecision(0)
          << " alt=" << std::setw(5) << air->getAltitudeM() << "m"
          << " hdg=" << std::setw(3) << air->getHeadingD() << "deg"
          << " roll=" << std::setw(4) << air->getRollD() << "deg"
          << " spd=" << std::setw(3) << air->getTotalVelocityKts() << "kt";

      // Telemetria do 6-DOF pela interface NATIVA do AirVehicle
      const double fuelMax{air->getFuelWtMax()};
      double thrust[4]{};
      const int nEng{air->getEngThrust(thrust, 4)};
      oss << std::setprecision(0) << " thrust=" << (nEng > 0 ? (thrust[0] + thrust[1]) : 0.0) << "lb"
          << std::setprecision(2) << " mach=" << air->getMach()
          << std::setprecision(1) << " g=" << air->getGload()
          << std::setprecision(0)
          << " fuel=" << ((fuelMax > 0.0) ? (air->getFuelWt() / fuelMax * 100.0) : 0.0) << "%";

      // Rotulo do comportamento: quadro de status por id (o Aircraft
      // nativo nao tem onde guardar isso) -- ver xnative/runtime_utils.hpp
      oss << " bt=" << std::setw(8) << std::left
          << mixr::xnative::getBehaviorLabel(air->getID()) << std::right;

      const auto autopilot = dynamic_cast<const mixr::models::Autopilot*>(air->getPilot());
      if (autopilot != nullptr) {
         oss << std::setprecision(0)
             << " ap(hdg=" << autopilot->getCommandedHeadingD()
             << ",alt=" << autopilot->getCommandedAltitudeFt() << "ft"
             << ",spd=" << autopilot->getCommandedVelocityKts() << ")";
      }

      std::string trackName;
      double trackRange{};
      if (nearestTrack(air, &trackName, &trackRange)) {
         oss << " pista=" << trackName << "@" << std::setprecision(1)
             << (trackRange / 1852.0) << "NM";
      }

      const auto datalink = dynamic_cast<const mixr::xnative::AlertDatalink*>(air->getDatalink());
      if (datalink != nullptr) {
         const auto alert = datalink->getAlert();
         if (alert.valid) {
            oss << " alerta<-" << alert.senderName << "(" << alert.contactName << ")";
         }
      }

      oss << std::endl;
   }

   std::cout << oss.str();
}

void printDeterministicDump(const std::vector<mixr::models::AirVehicle*>& fleet, const long frame)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(9);

   for (const auto air : fleet) {
      if (air == nullptr) continue;

      const mixr::base::Vec3d& pos{air->getPosition()};
      const auto datalink = dynamic_cast<const mixr::xnative::AlertDatalink*>(air->getDatalink());

      std::string trackName{"none"};
      double trackRange{};
      nearestTrack(air, &trackName, &trackRange);

      oss << "frame=" << frame
          << " player=" << (air->getName() != nullptr ? air->getName()->getString() : "?")
          << " n=" << pos[mixr::models::Player::INORTH]
          << " e=" << pos[mixr::models::Player::IEAST]
          << " alt=" << air->getAltitudeM()
          << " hdg=" << air->getHeadingD()
          << " roll=" << air->getRollD()
          << " pitch=" << air->getPitchD()
          << " spd=" << air->getTotalVelocity()
          << " mach=" << air->getMach()
          << " fuel=" << air->getFuelWt()
          << " bt=" << mixr::xnative::getBehaviorLabel(air->getID())
          << " track=" << trackName
          << " trackRange=" << trackRange
          << " alert=" << ((datalink != nullptr && datalink->hasAlert())
                            ? datalink->getAlert().senderName : std::string("none"))
          << " sent=" << (datalink != nullptr ? datalink->getSentCount() : 0L)
          << " recv=" << (datalink != nullptr ? datalink->getReceivedCount() : 0L)
          << std::endl;
   }

   std::cout << oss.str();
}

int runDeterministic(mixr::simulation::Station* const station,
                     const std::vector<mixr::models::AirVehicle*>& fleet,
                     const long frames)
{
   const double dt{1.0 / static_cast<double>(station->getTimeCriticalRate())};

   std::cout << "[deterministic] frames=" << frames
             << " dt=" << std::fixed << std::setprecision(9) << dt
             << " (1/tcRate; updateData no mesmo passo, entao os SimAgents"
             << " tambem decidem uma vez por frame)" << std::endl;

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
   std::string templatePath = "./poc/13-native-stack/configs/scenario.epp.in";
   std::string generatedPath = "./poc/13-native-stack/configs/scenario.generated.epp";

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

   if (deterministicFrames > 0) mixr::xnative::setLoggingEnabled(false);

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
   std::vector<mixr::models::AirVehicle*> fleet;
   for (const char* const name : names) {
      mixr::models::AirVehicle* const air{findAircraft(worldModel, name)};
      if (air == nullptr) {
         std::cerr << "player '" << name << "' nao encontrado!" << std::endl;
         std::exit(EXIT_FAILURE);
      }
      fleet.push_back(air);
   }

   // -----------------------------------------------------------------------
   // POTENCIA DE CRUZEIRO -- ver o comentario no scenario.epp: o autopilot
   // do c310 fecha malha de RUMO e de ALTITUDE, mas nao de VELOCIDADE (o
   // c310ap.xml apenas DECLARA ap/airspeed_hold e ap/throttle-cmd-norm, sem
   // canal que os implemente). Sem manete, a aeronave perdia velocidade, o
   // altitude hold empinava o nariz para compensar e acabava em perda de
   // sustentacao -- observado rodando (40 s: 160 -> 80 kts, arfagem 15 graus).
   //
   // Correcao no nivel do CENARIO, nao uma lei de controle nossa:
   // AirVehicle::setThrottles() e do proprio framework. A velocidade passa a
   // ser RESULTADO (potencia fixa + arrasto), nao comando.
   // -----------------------------------------------------------------------
   const double cruiseThrottle{0.95};
   for (const auto air : fleet) {
      if (air != nullptr) air->setThrottles(&cruiseThrottle, 1);
   }

   if (deterministicFrames > 0) {
      const int rc{runDeterministic(station, fleet, deterministicFrames)};
      station->event(mixr::base::Component::SHUTDOWN_EVENT);
      station->unref();
      return rc;
   }

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

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double wallTimeElapsed{};
   const double startTime{mixr::base::getComputerTime()};
   long frameCount{};

   while (!g_stopRequested) {

      // Aqui updateData() faz DUAS coisas: drena o gravador (Tacview) e
      // roda os SimAgents (UBF), que sao componentes da Station.
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
