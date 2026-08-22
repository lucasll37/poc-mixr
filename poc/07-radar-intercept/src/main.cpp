//
// poc/07-radar-intercept
//
// Integra tres subprojetos anteriores num unico cenario:
//   - 6-DOF real (poc/04): "hunter" usa JSBSimModel/F4N (mesmos dados)
//   - radar nativo (poc/06): Antenna/Gimbal + Tws + AirTrkMgr detectando
//     3 aeronaves "target" (RacModel) com RCS diferentes
//   - Tacview (shared/xtacview): as 4 aeronaves E as deteccoes do radar
//     exportadas pela cadeia nativa do 'dataRecorder' -- REID_PLAYER_DATA e
//     REID_NEW_TRACK viram linhas ACMI sem nenhuma chamada deste main.cpp
//
// main.cpp so orquestra: nao ha logica de deteccao/RF em C++ (100% nativa,
// via Tws/AirTrkMgr), nem de aerodinamica (JSBSim/RacModel), nem de
// exportacao (TacviewOutput).
//

#include "mixr_factory.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/dynamics/DynamicsModel.hpp"
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
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

const int bgRate{10};
const int logEveryNFrames{16};
const unsigned int maxTracksToQuery{16};

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

// edl_parser nao entende '#include' nativamente (gainPattern.epp) -- mesmo
// passo de preprocessador C das pocs 05/06 (ver CLAUDE.md).
std::string preprocessEdl(const std::string& inPath, const std::string& outPath)
{
   const std::string cmd{"g++ -E -x c -P -undef -nostdinc \"" + inPath + "\" -o \"" + outPath + "\""};
   const int rc{std::system(cmd.c_str())};
   if (rc != 0) {
      std::cerr << "[main] falha ao pre-processar " << inPath << " (rc=" << rc << ")" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   return outPath;
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

void printStatus(const double simTime, const int numTracks,
                  const std::vector<mixr::models::Player*>& players)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(0)
       << "[status] t=" << std::setprecision(1) << simTime << "s tracks=" << numTracks << "  ";
   for (std::size_t i = 0; i < players.size(); i++) {
      if (i > 0) oss << ", ";
      oss << players[i]->getName()->getString() << "@alt=" << std::setprecision(0)
          << players[i]->getAltitudeFt() << "ft/hdg=" << players[i]->getHeadingD();
   }
   std::cout << oss.str() << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
   std::string configFilename = "./poc/07-radar-intercept/configs/scenario.epp";
   std::string preprocessedPath = "./poc/07-radar-intercept/configs/scenario.preprocessed.epp";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") configFilename = argv[++i];
   }

   std::signal(SIGINT, onSigint);

   preprocessEdl(configFilename, preprocessedPath);
   mixr::simulation::Station* station{buildStation(preprocessedPath)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto hunter = dynamic_cast<mixr::models::Player*>(station->getOwnship());
   if (hunter == nullptr) {
      std::cerr << "No ownship (hunter) found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
   if (worldModel == nullptr) {
      std::cerr << "No WorldModel found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const char* const targetNames[3]{"target1", "target2", "target3"};
   std::vector<mixr::models::Player*> targets(3);
   for (int i = 0; i < 3; i++) {
      targets[static_cast<std::size_t>(i)] = findPlayerByName(worldModel, targetNames[i]);
      if (targets[static_cast<std::size_t>(i)] == nullptr) {
         std::cerr << targetNames[i] << " not found!" << std::endl;
         std::exit(EXIT_FAILURE);
      }
   }

   // hunter e F4N (JSBSimModel): sem hold-modes nativos do Autopilot (mesmo
   // gotcha da poc/04) -- so sustenta potencia pra manter voo reto e
   // nivelado, geometria estavel pro radar, sem manobra.
   const auto dynamicsModel = hunter->getDynamicsModel();
   if (dynamicsModel == nullptr) {
      std::cerr << "Hunter has no dynamics model!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   const int numEngines{dynamicsModel->getNumberOfEngines()};
   const std::vector<double> milPower(static_cast<std::size_t>(numEngines), 1.0);

   mixr::models::OnboardComputer* obc{hunter->getOnboardComputer()};
   mixr::models::TrackManager* trkMgr{};
   if (obc != nullptr) trkMgr = obc->getTrackManagerByName("twsTrkMgr");
   if (trkMgr == nullptr) {
      std::cerr << "No track manager found on hunter!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   std::vector<mixr::models::Player*> allPlayers{hunter, targets[0], targets[1], targets[2]};

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double simTime{};
   const double startTime{mixr::base::getComputerTime()};
   std::set<int> knownTrackIds;

   std::cout << "=== poc/07-radar-intercept ===" << std::endl;
   std::cout << "(Ctrl+C para encerrar; roda indefinidamente)" << std::endl;

   long frameCount{};
   while (!g_stopRequested) {

      station->updateData(static_cast<double>(dt));
      dynamicsModel->setThrottles(milPower.data(), numEngines);

      // F4N/JSBSim comeca destrimado (gotcha ja documentado na poc/04) --
      // sem nenhuma correcao ele perde altitude continuamente numa sessao
      // longa. Autonivelamento simples (proporcional ao pitch atual, sem
      // tocar no roll) so pra manter a geometria do radar razoavel por
      // mais tempo, nao e um piloto automatico de verdade.
      {
         const double kP{0.01};
         const double maxInput{0.2};
         const double pitchCorrection{
            std::max(-maxInput, std::min(maxInput, -kP * hunter->getPitchD()))};
         dynamicsModel->setControlStick(0.0, pitchCorrection);
      }

      // As 4 aeronaves E as deteccoes de radar chegam ao Tacview pela cadeia
      // nativa do 'dataRecorder' (REID_PLAYER_DATA / REID_NEW_TRACK) -- ver
      // configs/scenario.epp. A consulta ao TrackManager abaixo existe so
      // para o log legivel no console, que e o output didatico desta poc.
      mixr::base::safe_ptr<mixr::models::Track> trackList[maxTracksToQuery];
      const int numTracks{trkMgr->getTrackList(trackList, maxTracksToQuery)};
      for (int i = 0; i < numTracks; i++) {
         const mixr::models::Track* const trk{trackList[i]};
         const int id{trk->getTrackID()};
         if (knownTrackIds.insert(id).second) {
            const mixr::models::Player* const tgtPlayer{trk->getTarget()};
            const std::string tgtName{tgtPlayer != nullptr ? tgtPlayer->getName()->getString() : "?"};
            std::ostringstream ev;
            ev << std::fixed << std::setprecision(1)
               << "Radar contact #" << id << ": " << tgtName
               << " range=" << (trk->getRange() / 1852.0) << "NM"
               << " bearing=" << trk->getTrueAzimuthD() << "deg";
            std::cout << ">>> [t=" << simTime << "s] " << ev.str() << std::endl;
         }
      }

      simTime += dt;
      const double timeNow{mixr::base::getComputerTime()};
      const double elapsedTime{timeNow - startTime};
      const double nextFrameStart{simTime - elapsedTime};
      const int sleepTime{static_cast<int>(nextFrameStart * 1000.0)};
      if (sleepTime > 0) mixr::base::msleep(sleepTime);

      frameCount += 1;
      if (frameCount % logEveryNFrames == 0) {
         printStatus(simTime, numTracks, allPlayers);
      }
   }

   std::cout << "=== fim ===" << std::endl;

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
