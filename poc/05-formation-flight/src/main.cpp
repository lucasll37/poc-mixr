//
// poc/05-formation-flight
//
// Esquadrilha de 5 aeronaves: "lead" (JSBSimModel/F4N, pilotado por
// teclado via mixr::linkage) + 4 wingmen autonomos (RacModel, seguindo o
// lead via Autopilot.followTheLeadMode nativo, decidido por uma arvore de
// comportamento cada, com blackboard compartilhado). Ver
// /home/lima/.claude/plans/starry-tickling-cosmos.md para o desenho
// completo e as descobertas sobre o que o MIXR realmente oferece nativo.
//
// Este main.cpp e so orquestracao: gera o .epp final (unica excecao a
// "tudo via EDL" -- numTcThreads depende do numero de CPUs da maquina,
// que um arquivo EDL estatico nao pode saber; setSlotNumTcThreads() e
// privado, entao a unica forma honesta e um template substituido ANTES
// do parse, nao um patch no objeto depois), builda a Station, monta as 4
// arvores de comportamento dos wingmen, liga a telemetria Tacview e o
// gravador nativo, e roda o loop ate Ctrl+C.
//

#include "mixr_factory.hpp"
#include "formations.hpp"
#include "recorder_events.hpp"
#include "linkage/KeyboardIoHandler.hpp"
#include "bt/bt_factory.hpp"
#include "tacview/RealtimeTelemetryServer.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/simulation/AbstractDataRecorder.hpp"
#include "mixr/simulation/dataRecorderTokens.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/system/Autopilot.hpp"
#include "mixr/terrain/Terrain.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/util/system_utils.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <sched.h>
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
const int logEveryNFrames{16};

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

// -----------------------------------------------------------------------
// @NUM_TC_THREADS@ -- unica excecao a "tudo via EDL": numero de threads do
// pool nativo da Simulation depende da maquina, nao do cenario. Gera um
// .epp concreto a partir do template antes do parse (nenhum objeto MIXR e
// tocado fora do edl_parser).
// -----------------------------------------------------------------------
std::string generateScenario(const std::string& templatePath, const std::string& outPath)
{
   std::ifstream in(templatePath);
   std::ostringstream buf;
   buf << in.rdbuf();
   std::string text{buf.str()};

   const unsigned int hwThreads{std::thread::hardware_concurrency()};
   const int numTcThreads{static_cast<int>(hwThreads > 1 ? hwThreads - 1 : 1)};

   // substitui TODAS as ocorrencias (o template tambem menciona o
   // placeholder em comentarios de documentacao, nao so no slot real)
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
             << " (hardware_concurrency=" << hwThreads << " - 1)" << std::endl;
   return outPath;
}

// edl_parser (flex/bison) NAO entende '#include'/'#define' nativamente --
// diferente dos pocs anteriores (scenario.epp sem nenhum #include), esta
// poc usa #include de verdade (linkage.epp/recorder.epp/rtb_route.epp,
// mesmo padrao dos .epp de exemplo do proprio MIXR, que por sua vez
// dependem de um passo externo de preprocessador C -- ver
// examples/README.md, "use 'make edl' pra gerar os *.edl"). Reproduzimos
// esse passo aqui chamando g++ -E diretamente, em vez de exigir um
// Makefile target/ferramenta externa.
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

// Descomprime o tile SRTM na primeira execucao (o .gz e o que fica
// versionado no git; o .hgt descomprimido e artefato gerado, gitignored).
void ensureTerrainDecompressed(const std::string& dir, const std::string& baseName)
{
   const std::string hgt{dir + baseName + ".hgt"};
   const std::string gz{dir + baseName + ".hgt.gz"};

   std::ifstream test(hgt, std::ios::binary);
   if (test.good()) return;

   std::cout << "[main] descomprimindo " << gz << " ..." << std::endl;
   const std::string cmd{"gunzip -k \"" + gz + "\""};
   std::system(cmd.c_str());
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

mixr::models::Autopilot* getAutopilot(mixr::models::Player* p)
{
   mixr::models::Autopilot* ap{};
   mixr::base::Pair* pair{p->getPilotByType(typeid(mixr::models::Autopilot))};
   if (pair != nullptr) ap = static_cast<mixr::models::Autopilot*>(pair->object());
   return ap;
}

struct Wingman
{
   mixr::models::Player* player{};
   mixr::models::Autopilot* autopilot{};
   BT::Tree tree;
};

void printStatusLine(const int numTcThreads, const long cycleIndex,
                      const std::vector<mixr::models::Player*>& players)
{
   std::ostringstream oss;
   oss << "[status] ciclo=" << cycleIndex
       << " tc_threads(nativo,round-robin)=" << numTcThreads
       << " main_core(sched_getcpu)=" << ::sched_getcpu()
       << " players=";
   for (std::size_t i = 0; i < players.size(); ++i) {
      if (i > 0) oss << ",";
      oss << players[i]->getName()->getString() << "@alt=" << std::fixed << std::setprecision(0)
          << players[i]->getAltitudeFt() << "ft";
   }
   // Nota: o MIXR nao expoe os handles internos das threads do pool
   // (tcThreads e privado em mixr::simulation::Simulation, sem pinning de
   // CPU em lugar nenhum do framework -- ver CLAUDE.md), entao a afinidade
   // "por thread" pedida so e honestamente reportavel pra thread principal
   // deste processo. O tempo por fase (dynamics/emit/receive/logica) tambem
   // nao e instrumentavel de fora: as 4 fases rodam dentro de
   // Simulation::updateTC(), chamado pela thread TC nativa, nao pelo
   // nosso loop.
   std::cout << oss.str() << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
   std::string templatePath = "./poc/05-formation-flight/configs/scenario.epp.in";
   std::string generatedPath = "./poc/05-formation-flight/configs/scenario.generated.epp";
   std::string preprocessedPath = "./poc/05-formation-flight/configs/scenario.preprocessed.epp";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") templatePath = argv[++i];
   }

   std::signal(SIGINT, onSigint);

   ensureTerrainDecompressed("./poc/05-formation-flight/data/terrain/srtm/", "S23W043");
   generateScenario(templatePath, generatedPath);
   preprocessEdl(generatedPath, preprocessedPath);

   mixr::simulation::Station* station{buildStation(preprocessedPath)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
   if (worldModel == nullptr) {
      std::cerr << "No WorldModel found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   // getTerrain() so tem overload publico na versao const (a nao-const e
   // protected); so consultamos elevacao (metodo const), nunca mutamos.
   const mixr::models::WorldModel* const worldModelConst{worldModel};

   const auto lead = dynamic_cast<mixr::models::Player*>(station->getOwnship());
   if (lead == nullptr) {
      std::cerr << "No ownship (lead) found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   std::vector<Wingman> wingmen(4);
   const char* const wingmanNames[4]{"wing1", "wing2", "wing3", "wing4"};
   for (int i = 0; i < 4; i++) {
      wingmen[static_cast<std::size_t>(i)].player = findPlayerByName(worldModel, wingmanNames[i]);
      if (wingmen[static_cast<std::size_t>(i)].player == nullptr) {
         std::cerr << "Wingman " << wingmanNames[i] << " not found!" << std::endl;
         std::exit(EXIT_FAILURE);
      }
      wingmen[static_cast<std::size_t>(i)].autopilot = getAutopilot(wingmen[static_cast<std::size_t>(i)].player);
   }

   // --- teclado: injeta o estado de formacao compartilhado + terrain ---
   formations::FormationState formationState;

   const auto keyboardHandler = dynamic_cast<keyboard::KeyboardIoHandler*>(station->getIoHandler());
   if (keyboardHandler == nullptr) {
      std::cerr << "No KeyboardIoHandler found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   keyboardHandler->setFormationState(&formationState);
   keyboardHandler->setTerrain(worldModelConst->getTerrain());

   // --- BT: uma arvore por wingman, blackboard pai compartilhado ---
   BT::BehaviorTreeFactory btFactory;
   bt_nodes::registerNodes(btFactory);

   // Nota: BT::Blackboard::create(parent) so encaminha automaticamente
   // pro pai as chaves com "port remapping" explicito (subtree ports do
   // XML) -- get<T>/set<T> direto NAO sobe a cadeia de pais sozinho. Para
   // realmente compartilhar o mesmo FormationState entre as 4 arvores,
   // registramos o mesmo ponteiro em cada blackboard filho (mesmo objeto
   // compartilhado, ainda que cada blackboard tenha sua propria entrada).
   auto parentBlackboard = BT::Blackboard::create();

   for (int i = 0; i < 4; i++) {
      auto& w = wingmen[static_cast<std::size_t>(i)];
      auto childBlackboard = BT::Blackboard::create(parentBlackboard);
      childBlackboard->set<formations::FormationState*>("formationState", &formationState);
      childBlackboard->set<int>("slotIndex", i);
      childBlackboard->set<mixr::models::Autopilot*>("autopilot", w.autopilot);
      childBlackboard->set<mixr::models::Player*>("wingman", w.player);
      childBlackboard->set<const mixr::terrain::Terrain*>("terrain", worldModelConst->getTerrain());
      w.tree = btFactory.createTreeFromFile(
         "./poc/05-formation-flight/configs/wingman_tree.xml", childBlackboard);
   }

   // --- Tacview (protocolo Real-Time Telemetry, ver poc/04) + gravacao local ---
   tacview::RealtimeTelemetryServer tacviewServer("0.0.0.0", 1234);
   if (!tacviewServer.start()) {
      std::cerr << "Failed to start Tacview telemetry server!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   tacviewServer.startRecording("./poc/05-formation-flight/data/recordings/mission.acmi");

   const std::vector<std::pair<std::uint32_t, tacview::ObjectInfo>> objectInfos{
      {0x101, {"lead", "Air+FixedWing", "Blue"}},
      {0x102, {"wing1", "Air+FixedWing", "Blue"}},
      {0x103, {"wing2", "Air+FixedWing", "Blue"}},
      {0x104, {"wing3", "Air+FixedWing", "Blue"}},
      {0x105, {"wing4", "Air+FixedWing", "Blue"}},
   };
   std::vector<mixr::models::Player*> allPlayers{lead, wingmen[0].player, wingmen[1].player,
                                                  wingmen[2].player, wingmen[3].player};

   const auto dataRecorder = station->getDataRecorder();

   // --- loop ---
   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double simTime{};
   const double startTime{mixr::base::getComputerTime()};
   const unsigned int hwThreads{std::thread::hardware_concurrency()};
   const int numTcThreadsReported{static_cast<int>(hwThreads > 1 ? hwThreads - 1 : 1)};

   std::cout << "=== poc/05-formation-flight ===" << std::endl;
   std::cout << "(Ctrl+C para encerrar; roda indefinidamente)" << std::endl;

   long frameCount{};
   while (!g_stopRequested) {

      station->updateData(static_cast<double>(dt));

      for (auto& w : wingmen) w.tree.tickRoot();

      tacviewServer.acceptIfNeeded();
      tacviewServer.beginFrame(simTime);
      for (std::size_t i = 0; i < allPlayers.size(); i++) {
         const auto p = allPlayers[i];
         tacviewServer.updateObject(objectInfos[i].first,
                                    p->getLongitude(), p->getLatitude(), p->getAltitudeM(),
                                    p->getRollD(), p->getPitchD(), p->getHeadingD(),
                                    &objectInfos[i].second);
      }

      const std::string pendingEvent{keyboardHandler->consumePendingEvent()};
      if (!pendingEvent.empty()) {
         tacviewServer.logEvent(0x101, pendingEvent);
         if (dataRecorder != nullptr) {
            const unsigned int reid{pendingEvent.rfind("RTB", 0) == 0
               ? static_cast<unsigned int>(REID_RTB_ENGAGED)
               : static_cast<unsigned int>(REID_FORMATION_CHANGED)};
            const mixr::base::Object* pObjects[4]{lead, nullptr, nullptr, nullptr};
            const double values[4]{};
            dataRecorder->recordData(reid, pObjects, values);
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
         printStatusLine(numTcThreadsReported, frameCount / logEveryNFrames, allPlayers);

         if (dataRecorder != nullptr) {
            for (const auto p : allPlayers) {
               const mixr::base::Object* pObjects[4]{p, nullptr, nullptr, nullptr};
               const double values[4]{};
               dataRecorder->recordData(REID_PLAYER_DATA, pObjects, values);
            }
         }
      }
   }

   std::cout << "=== fim ===" << std::endl;

   tacviewServer.stop();
   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
