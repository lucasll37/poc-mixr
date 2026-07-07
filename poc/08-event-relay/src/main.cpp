//
// poc/08-event-relay
//
// Mesmas features das pocs 04/06/07 (6-DOF via JSBSimModel/F4N, radar
// nativo, Tacview) -- a diferenca e arquitetural: a deteccao do radar
// chega aos seus consumidores via EVENTO nativo (Component::event()/
// send(), ver RadarContactRelay/AlertReceiver), nao por chamada C++
// direta orquestrada por este main.cpp.
//
// Por isso main.cpp fica ainda mais fino que nas pocs anteriores: NENHUMA
// logica de deteccao ou de alerta mora aqui. station->updateData(dt) por
// si so ja faz a arvore de componentes cascatear ate
// RadarContactRelay::process() (fase 3, nativa) disparar os eventos e
// AlertReceiver::event() (tambem nativo) trata-los -- tudo isso acontece
// SOZINHO dentro do updateData(). Este main.cpp so constroi a Station,
// sustenta o voo do hunter (mesmo gotcha de trim do F4N ja documentado) e
// encaminha pro Tacview o que os AlertReceivers ja processaram
// (consumePendingAlert() -- so pra log/replay, a decisao "recebi, tratei"
// ja aconteceu dentro do event handler nativo).
//

#include "AlertReceiver.hpp"
#include "mixr_factory.hpp"
#include "tacview/RealtimeTelemetryServer.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/dynamics/DynamicsModel.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

const int bgRate{10};

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

// edl_parser nao entende '#include' nativamente (gainPattern.epp) -- mesmo
// passo de preprocessador C das pocs 05/06/07 (ver CLAUDE.md).
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

events::AlertReceiver* findAlertReceiver(mixr::models::Player* player, const char* const name)
{
   mixr::base::Pair* p{player->findByName(name)};
   if (p == nullptr) return nullptr;
   return dynamic_cast<events::AlertReceiver*>(p->object());
}

} // namespace

int main(int argc, char* argv[])
{
   std::string configFilename = "./poc/08-event-relay/configs/scenario.epp";
   std::string preprocessedPath = "./poc/08-event-relay/configs/scenario.preprocessed.epp";

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

   const auto controller = findPlayerByName(worldModel, "controller");
   const auto target = findPlayerByName(worldModel, "target");
   if (controller == nullptr || target == nullptr) {
      std::cerr << "controller/target player not found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const auto localAlert = findAlertReceiver(hunter, "localAlert");
   const auto remoteAlert = findAlertReceiver(controller, "alertReceiver");
   if (localAlert == nullptr || remoteAlert == nullptr) {
      std::cerr << "AlertReceiver components not found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   // hunter e F4N (JSBSimModel): sem hold-modes nativos do Autopilot (mesmo
   // gotcha da poc/04/07) -- so sustenta potencia + autonivelamento simples
   // de pitch, sem manobra, pra manter a geometria do radar estavel.
   const auto dynamicsModel = hunter->getDynamicsModel();
   if (dynamicsModel == nullptr) {
      std::cerr << "Hunter has no dynamics model!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   const int numEngines{dynamicsModel->getNumberOfEngines()};
   const std::vector<double> milPower(static_cast<std::size_t>(numEngines), 1.0);

   tacview::RealtimeTelemetryServer tacviewServer("0.0.0.0", 1234);
   if (!tacviewServer.start()) {
      std::cerr << "Failed to start Tacview telemetry server!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   tacviewServer.startRecording("./poc/08-event-relay/data/recordings/mission.acmi");

   const std::uint32_t hunterId{0x101};
   const std::uint32_t controllerId{0x102};
   const std::vector<std::pair<std::uint32_t, tacview::ObjectInfo>> objectInfos{
      {hunterId,     {"hunter",     "Air+FixedWing", "Blue"}},
      {controllerId, {"controller", "Air+FixedWing", "Blue"}},
      {0x103,        {"target",     "Air+FixedWing", "Red"}},
   };
   const std::vector<mixr::models::Player*> allPlayers{hunter, controller, target};

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double simTime{};
   const double startTime{mixr::base::getComputerTime()};

   std::cout << "=== poc/08-event-relay ===" << std::endl;
   std::cout << "(Ctrl+C para encerrar; roda indefinidamente)" << std::endl;

   while (!g_stopRequested) {

      // Toda a deteccao (Tws/AirTrkMgr) e o relay de eventos
      // (RadarContactRelay -> AlertReceiver, local e remoto) acontecem
      // SOZINHOS aqui dentro, cascateando pela arvore de componentes --
      // nada disso e chamado explicitamente por este main.cpp.
      station->updateData(static_cast<double>(dt));

      dynamicsModel->setThrottles(milPower.data(), numEngines);
      {
         const double kP{0.01};
         const double maxInput{0.2};
         const double pitchCorrection{
            std::max(-maxInput, std::min(maxInput, -kP * hunter->getPitchD()))};
         dynamicsModel->setControlStick(0.0, pitchCorrection);
      }

      tacviewServer.acceptIfNeeded();
      tacviewServer.beginFrame(simTime);
      for (std::size_t i = 0; i < allPlayers.size(); i++) {
         const auto p = allPlayers[i];
         tacviewServer.updateObject(objectInfos[i].first,
                                    p->getLongitude(), p->getLatitude(), p->getAltitudeM(),
                                    p->getRollD(), p->getPitchD(), p->getHeadingD(),
                                    &objectInfos[i].second);
      }

      // Aqui so consumimos o que os AlertReceivers JA processaram, pra
      // log/Tacview -- nao ha nenhuma decisao de negocio nesta parte.
      const std::string localAlertText{localAlert->consumePendingAlert()};
      if (!localAlertText.empty()) tacviewServer.logEvent(hunterId, "[local] " + localAlertText);

      const std::string remoteAlertText{remoteAlert->consumePendingAlert()};
      if (!remoteAlertText.empty()) tacviewServer.logEvent(controllerId, "[remote] " + remoteAlertText);

      simTime += dt;
      const double timeNow{mixr::base::getComputerTime()};
      const double elapsedTime{timeNow - startTime};
      const double nextFrameStart{simTime - elapsedTime};
      const int sleepTime{static_cast<int>(nextFrameStart * 1000.0)};
      if (sleepTime > 0) mixr::base::msleep(sleepTime);
   }

   std::cout << "=== fim ===" << std::endl;

   tacviewServer.stop();
   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
