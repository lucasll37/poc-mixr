//
// poc/06-radar-detection
//
// Radar de busca nativo (Antenna + Tws + AirTrkMgr, dentro de um
// SensorMgr/OnboardComputer -- ver configs/scenario.epp) detectando uma
// segunda aeronave. main.cpp so orquestra: constroi a Station e, a cada
// tick, consulta o TrackManager nativo do "hunter" (Player::
// getOnboardComputer()->getTrackManagerByName()) e imprime quando uma
// pista nova aparece -- toda a deteccao em si (equacao do radar, ganho
// de antena, RCS do alvo via signature:, correlacao track-while-scan) e
// 100% nativa do framework.
//

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/models/system/OnboardComputer.hpp"
#include "mixr/models/system/trackmanager/TrackManager.hpp"
#include "mixr/models/Track.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/safe_ptr.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/util/system_utils.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/interop/dis/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/base/factory.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

namespace {

const int bgRate{10};
const double simDuration{90.0};
const unsigned int maxTracksToQuery{16};

mixr::base::Object* factory(const std::string& name)
{
   mixr::base::Object* obj{mixr::simulation::factory(name)};

   if (obj == nullptr) obj = mixr::models::factory(name);
   if (obj == nullptr) obj = mixr::terrain::factory(name);
   if (obj == nullptr) obj = mixr::dis::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);
   return obj;
}

// edl_parser (flex/bison) nao entende '#include' nativamente -- este
// cenario usa #include "gainPattern.epp" (mesmo padrao de examples/
// testRadar), entao precisa do mesmo passo de preprocessador C real
// documentado em poc/05-formation-flight/src/main.cpp / CLAUDE.md.
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

void printTelemetry(const double simTime, mixr::models::Player* hunter, mixr::models::Player* target)
{
   std::cout << std::fixed << std::setprecision(1)
             << "t=" << std::setw(5) << simTime << "s"
             << "  hunter alt=" << hunter->getAltitudeFt() << "ft hdg=" << hunter->getHeadingD()
             << "  target alt=" << target->getAltitudeFt() << "ft hdg=" << target->getHeadingD()
             << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
   std::string configFilename = "./poc/06-radar-detection/configs/scenario.epp";
   std::string preprocessedPath = "./poc/06-radar-detection/configs/scenario.preprocessed.epp";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") configFilename = argv[++i];
   }

   preprocessEdl(configFilename, preprocessedPath);
   mixr::simulation::Station* station{buildStation(preprocessedPath)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto hunter = dynamic_cast<mixr::models::Player*>(station->getOwnship());
   if (hunter == nullptr) {
      std::cerr << "No ownship (hunter) found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   mixr::models::WorldModel* worldModel{}; // usado so pra achar o target por nome
   {
      const auto sim = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
      worldModel = sim;
   }
   mixr::models::Player* target{};
   if (worldModel != nullptr) {
      mixr::base::PairStream* players{worldModel->getPlayers()};
      if (players != nullptr) {
         mixr::base::Pair* p{players->findByName("target")};
         if (p != nullptr) target = dynamic_cast<mixr::models::Player*>(p->object());
         players->unref();
      }
   }
   if (target == nullptr) {
      std::cerr << "No target player found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   mixr::models::OnboardComputer* obc{hunter->getOnboardComputer()};
   mixr::models::TrackManager* trkMgr{};
   if (obc != nullptr) trkMgr = obc->getTrackManagerByName("twsTrkMgr");
   if (trkMgr == nullptr) {
      std::cerr << "No track manager found on hunter!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double simTime{};
   const double startTime{mixr::base::getComputerTime()};
   std::set<int> knownTrackIds;

   std::cout << "=== poc/06-radar-detection ===" << std::endl;
   printTelemetry(simTime, hunter, target);

   int ticksPerSecond{};
   while (simTime < simDuration) {

      station->updateData(static_cast<double>(dt));

      mixr::base::safe_ptr<mixr::models::Track> trackList[maxTracksToQuery];
      const int numTracks{trkMgr->getTrackList(trackList, maxTracksToQuery)};
      for (int i = 0; i < numTracks; i++) {
         const mixr::models::Track* const trk{trackList[i]};
         const int id{trk->getTrackID()};
         if (knownTrackIds.insert(id).second) {
            const mixr::models::Player* const tgtPlayer{trk->getTarget()};
            std::cout << std::fixed << std::setprecision(1)
                      << ">>> [t=" << simTime << "s] RADAR: nova pista #" << id
                      << " detectada -- range=" << (trk->getRange() / 1852.0) << "NM"
                      << " bearing=" << trk->getTrueAzimuthD() << "deg"
                      << (tgtPlayer != nullptr ? (std::string(" (") + tgtPlayer->getName()->getString() + ")") : "")
                      << std::endl;
         }
      }

      simTime += dt;
      const double timeNow{mixr::base::getComputerTime()};
      const double elapsedTime{timeNow - startTime};
      const double nextFrameStart{simTime - elapsedTime};
      const int sleepTime{static_cast<int>(nextFrameStart * 1000.0)};
      if (sleepTime > 0) mixr::base::msleep(sleepTime);

      ticksPerSecond += 1;
      if (ticksPerSecond == bgRate) {
         printTelemetry(simTime, hunter, target);
         std::cout << "    tracks ativas: " << numTracks << std::endl;
         ticksPerSecond = 0;
      }
   }

   std::cout << "=== fim ===" << std::endl;

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
