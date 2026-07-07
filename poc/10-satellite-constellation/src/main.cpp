//
// poc/10-satellite-constellation
//
// Constelacao de 4 satelites LEO (SpaceVehicle, sem dynamicsModel -- ver
// comentario em configs/scenario.epp: MIXR nao tem propagador orbital
// nativo nenhum). A cada tick, este main.cpp calcula a posicao real de
// cada satelite (orbit::groundTrack(), 2-body + rotacao da Terra) e
// aplica via Player::setGeocPosition(ecef, slaved=true) -- setter nativo
// que desliga a integracao de posicao propria do Player (Player::
// positionUpdate() vira no-op quando slaved), entao a "fisica" de fato
// aplicada ao objeto e 100% a nossa.
//
// Aceleracao de velocidade ("permita que a velocidade possa ser
// acelerada"): usa o mecanismo NATIVO Station::fastForwardRate (slot +
// getFastForwardRate()/setFastForwardRate() publicos) -- documentado em
// detalhe no scenario.epp. O relogio simulado (simTime, usado tanto pela
// nossa mecanica orbital quanto pelos timestamps ACMI do Tacview) avanca
// `dtReal * fastForwardRate` a cada iteracao, i.e. tudo -- orbita e
// Tacview -- acelera junto, de forma consistente, ao mesmo multiplicador
// que controla o proprio laco T/C nativo da Station.
//
// Controle interativo: '+'/'-' no teclado ajustam o fastForwardRate em
// runtime chamando station->setFastForwardRate() diretamente (metodo
// publico comum, sem necessidade de um mixr::linkage::IoDevice dedicado
// -- ao contrario do teclado da poc/05, aqui nao ha nenhum outro canal
// nomeado/EDL que justifique esse mecanismo mais pesado). Mesmo padrao de
// termios em modo raw ja usado na poc/05 (com o mesmo fallback: sem TTY
// real neste ambiente, tcgetattr falha e o programa segue sem input).
//

#include "orbit.hpp"
#include "tacview/RealtimeTelemetryServer.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/osg/Vec3d"
#include "mixr/base/util/nav_utils.hpp"
#include "mixr/base/util/system_utils.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/base/factory.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

const int bgRate{10};                     // cadencia real (Hz) do laco principal
const unsigned int initialFastForward{60}; // espelha o valor do scenario.epp
const unsigned int minFastForward{1};
const unsigned int maxFastForward{2000};
const unsigned int fastForwardStep{10};

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

mixr::base::Object* factory(const std::string& name)
{
   mixr::base::Object* obj{mixr::simulation::factory(name)};
   if (obj == nullptr) obj = mixr::models::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);
   return obj;
}

mixr::simulation::Station* buildStation(const std::string& filename)
{
   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, factory, &num_errors)};
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

mixr::models::Player* findPlayerByName(mixr::models::WorldModel* wm, const char* const name)
{
   mixr::base::PairStream* players{wm->getPlayers()};
   mixr::models::Player* result{};
   if (players != nullptr) {
      mixr::base::Pair* p{players->findByName(name)};
      if (p != nullptr) result = dynamic_cast<mixr::models::Player*>(p->object());
      players->unref();
   }
   return result;
}

// Termios em modo raw + stdin nao-bloqueante -- mesmo padrao/fallback da
// poc/05 (sem TTY real aqui, tcgetattr falha e seguimos sem input).
struct RawKeyboard
{
   termios original{};
   bool active{};

   RawKeyboard()
   {
      if (::tcgetattr(STDIN_FILENO, &original) != 0) return;
      termios raw{original};
      raw.c_lflag &= ~(ICANON | ECHO);
      raw.c_cc[VMIN] = 0;
      raw.c_cc[VTIME] = 0;
      if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return;
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
      active = true;
   }

   ~RawKeyboard()
   {
      if (active) ::tcsetattr(STDIN_FILENO, TCSANOW, &original);
   }

   char poll() const
   {
      char c{};
      const ssize_t n{::read(STDIN_FILENO, &c, 1)};
      return (n == 1) ? c : '\0';
   }
};

struct Satellite
{
   mixr::models::Player* player{};
   orbit::CircularOrbit elements;
   std::uint32_t tacviewId{};
   tacview::ObjectInfo info;
};

} // namespace

int main(int argc, char* argv[])
{
   std::string configFilename = "./poc/10-satellite-constellation/configs/scenario.epp";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") configFilename = argv[++i];
   }

   std::signal(SIGINT, onSigint);

   mixr::simulation::Station* station{buildStation(configFilename)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
   if (worldModel == nullptr) {
      std::cerr << "No WorldModel found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   // Constelacao "walker train" de 1 plano: mesma inclinacao/RAAN, 4
   // satelites defasados 90 graus em argumento de latitude -- tem que
   // bater com os initLatitude/initLongitude do scenario.epp (t=0).
   const double altitudeM{780000.0};
   const double inclinationDeg{53.0};
   const double raanDeg{0.0};

   std::array<Satellite, 4> sats{{
      {findPlayerByName(worldModel, "sat1"), {altitudeM, inclinationDeg, raanDeg, 0.0},   0x301, {"sat1", "Misc", "Blue"}},
      {findPlayerByName(worldModel, "sat2"), {altitudeM, inclinationDeg, raanDeg, 90.0},  0x302, {"sat2", "Misc", "Blue"}},
      {findPlayerByName(worldModel, "sat3"), {altitudeM, inclinationDeg, raanDeg, 180.0}, 0x303, {"sat3", "Misc", "Blue"}},
      {findPlayerByName(worldModel, "sat4"), {altitudeM, inclinationDeg, raanDeg, 270.0}, 0x304, {"sat4", "Misc", "Blue"}},
   }};

   for (const auto& s : sats) {
      if (s.player == nullptr) {
         std::cerr << "satellite player not found in scenario!" << std::endl;
         std::exit(EXIT_FAILURE);
      }
   }

   std::cout << "=== poc/10-satellite-constellation ===" << std::endl;
   std::cout << "Periodo orbital: " << orbit::orbitalPeriodSec(sats[0].elements) << "s (~"
              << orbit::orbitalPeriodSec(sats[0].elements) / 60.0 << " min)" << std::endl;
   std::cout << "'+'/'-' aceleram/desaceleram o tempo simulado (fastForwardRate nativo da Station);"
              << " Ctrl+C encerra." << std::endl;

   tacview::RealtimeTelemetryServer tacviewServer("0.0.0.0", 1234);
   if (!tacviewServer.start()) {
      std::cerr << "Failed to start Tacview telemetry server!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   tacviewServer.startRecording("./poc/10-satellite-constellation/data/recordings/mission.acmi");

   station->setFastForwardRate(initialFastForward);
   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   RawKeyboard keyboard;

   const double dt{1.0 / static_cast<double>(bgRate)}; // cadencia real (wall clock) do laco
   double simTime{};                                     // relogio simulado, acelerado por fastForwardRate
   double wallTimeElapsed{};                              // usado so pra pacing (tempo real)
   const double startTime{mixr::base::getComputerTime()};
   int frameCount{};

   while (!g_stopRequested) {

      const char key{keyboard.poll()};
      if (key == '+' || key == '=') {
         const unsigned int rate{std::min(station->getFastForwardRate() + fastForwardStep, maxFastForward)};
         station->setFastForwardRate(rate);
         std::cout << "[t=" << simTime << "s] fastForwardRate -> " << rate << "x" << std::endl;
      } else if (key == '-' || key == '_') {
         const unsigned int current{station->getFastForwardRate()};
         const unsigned int rate{(current > minFastForward + fastForwardStep) ? current - fastForwardStep : minFastForward};
         station->setFastForwardRate(rate);
         std::cout << "[t=" << simTime << "s] fastForwardRate -> " << rate << "x" << std::endl;
      } else if (key == 'q') {
         g_stopRequested = 1;
      }

      station->updateData(dt);

      const unsigned int rate{station->getFastForwardRate()};
      const double simDt{dt * static_cast<double>(rate)};
      simTime += simDt;

      tacviewServer.acceptIfNeeded();
      tacviewServer.beginFrame(simTime);

      for (auto& s : sats) {
         double latDeg{}, lonDeg{}, altM{};
         orbit::groundTrack(s.elements, simTime, &latDeg, &lonDeg, &altM);

         double x{}, y{}, z{};
         mixr::base::nav::convertGeod2Ecef(latDeg, lonDeg, altM, &x, &y, &z);
         s.player->setGeocPosition(mixr::base::Vec3d(x, y, z), true);

         // Rumo real da orbita (nao um valor fixo): bearing entre a
         // posicao atual e a posicao um instante a frente, via
         // mixr::base::nav::fll2bd (mesma conversao geodesica nativa
         // usada no resto do projeto).
         double latAheadDeg{}, lonAheadDeg{}, altAheadM{};
         orbit::groundTrack(s.elements, simTime + 1.0, &latAheadDeg, &lonAheadDeg, &altAheadM);
         double bearingDeg{}, distNm{};
         mixr::base::nav::fll2bd(latDeg, lonDeg, latAheadDeg, lonAheadDeg, &bearingDeg, &distNm);
         s.player->setEulerAngles(0.0, 0.0, bearingDeg * (3.14159265358979323846 / 180.0));

         tacviewServer.updateObject(s.tacviewId, s.player->getLongitude(), s.player->getLatitude(),
                                     s.player->getAltitudeM(), s.player->getRollD(), s.player->getPitchD(),
                                     s.player->getHeadingD(), &s.info);
      }

      ++frameCount;
      if (frameCount % (bgRate * 5) == 0) {
         std::cout << "[t=" << static_cast<long>(simTime) << "s, " << rate << "x] "
                    << "sat1 lat/lon/alt=" << sats[0].player->getLatitude() << "/"
                    << sats[0].player->getLongitude() << "/" << sats[0].player->getAltitudeM() << "m"
                    << std::endl;
      }

      wallTimeElapsed += dt;
      const double timeNow{mixr::base::getComputerTime()};
      const double elapsedTime{timeNow - startTime};
      const double nextFrameStart{wallTimeElapsed - elapsedTime};
      const int sleepTime{static_cast<int>(nextFrameStart * 1000.0)};
      if (sleepTime > 0) mixr::base::msleep(sleepTime);
   }

   std::cout << "=== fim ===" << std::endl;

   tacviewServer.stop();
   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
