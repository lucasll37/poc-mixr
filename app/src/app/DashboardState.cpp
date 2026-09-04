#include "app/DashboardState.hpp"

#include "app/Fleet.hpp"

#include "xboard/Board.hpp"
#include "xclock/ClockStation.hpp"
#include "xtacview/TacviewOutput.hpp"
#include "xtrack/TrackQuery.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/simulation/Station.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/Statistic.hpp"

#include <cxxabi.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <memory>

namespace app {

namespace {

const double M2NM{1.0 / 1852.0};

// Memoria residente do processo, em KiB. /proc/self/statm da o dado sem
// nenhuma dependencia nova (campo 2 = paginas residentes); onde o arquivo
// nao existir devolve 0 e o painel omite a linha. Ler um arquivo minusculo
// a 10 Hz e desprezivel -- e e informacao de tempo NAO critico por
// definicao, que e do que esta aba trata.
long residentKb()
{
   std::ifstream statm("/proc/self/statm");
   if (!statm.is_open()) return 0;
   long totalPages{}, residentPages{};
   if (!(statm >> totalPages >> residentPages)) return 0;
   return residentPages * (sysconf(_SC_PAGESIZE) / 1024);
}

// Nome de classe C++ mais derivada via RTTI pura -- sem incluir header
// nenhum do modelo. E o fallback de 'typeLabel' quando o EDL nao declarou
// 'type:'; funciona para QUALQUER .so carregado, inclusive um "modelo
// desconhecido" (ver models/fixtures/stub/docs/CONTRATO.md).
std::string demangledClassName(const mixr::models::Player& player)
{
   int status{};
   const std::unique_ptr<char, void(*)(void*)> demangled{
      abi::__cxa_demangle(typeid(player).name(), nullptr, nullptr, &status),
      std::free};
   if (status != 0 || demangled == nullptr) return "?";

   std::string name{demangled.get()};
   const std::string prefix{"mixr::"};
   if (name.rfind(prefix, 0) == 0) name = name.substr(prefix.size());
   return name;
}

EntityState captureEntity(mixr::models::Player* const player)
{
   EntityState s;
   s.id = player->getID();
   s.name = (player->getName() != nullptr) ? player->getName()->getString() : "?";

   const mixr::base::String* const type{player->getType()};
   s.typeLabel = (type != nullptr && type->len() > 0) ? type->getString() : demangledClassName(*player);

   s.majorType = player->getMajorType();
   s.side = static_cast<unsigned int>(player->getSide());
   s.mode = player->getMode();

   const mixr::base::Vec3d& pos{player->getPosition()};
   s.northM = pos[mixr::models::Player::INORTH];
   s.eastM = pos[mixr::models::Player::IEAST];

   s.altitudeM = player->getAltitudeM();
   s.terrainElevM = player->getTerrainElevationM();
   s.altitudeAglM = player->getAltitudeAglM();
   s.headingDeg = player->getHeadingD();
   s.rollDeg = player->getRollD();
   s.pitchDeg = player->getPitchD();
   s.speedKts = player->getTotalVelocityKts();
   s.machNum = player->getMach();

   // Bloco exclusivo de AirVehicle -- combustivel/G/empuxo de motor sao
   // conceitos aerodinamicos, sem equivalente generico em Player. Um
   // GroundVehicle/Ship/GuidedMissile/modelo futuro simplesmente nao
   // preenche isto, e app/FleetPanel.cpp esconde as linhas correspondentes.
   if (const auto* const air = dynamic_cast<const mixr::models::AirVehicle*>(player)) {
      double thrust[4]{};
      const int nEng{air->getEngThrust(thrust, 4)};
      s.hasThrust = true;
      s.thrustLb = (nEng > 0) ? (thrust[0] + thrust[1]) : 0.0;

      const double fuelMax{air->getFuelWtMax()};
      s.hasFuel = true;
      s.fuelFrac = (fuelMax > 0.0) ? (air->getFuelWt() / fuelMax) : 1.0;

      s.hasGload = true;
      s.gLoad = air->getGload();

      const mixr::xtrack::TrackInfo track{mixr::xtrack::nearestHostileTrack(air)};
      s.hasTrack = track.found;
      s.trackName = track.name;
      s.trackRangeNm = track.rangeM * M2NM;
   }

   // shared/xboard -- ja agnostico (ver o cabecalho de Board.hpp): so
   // preenche se o MODELO escreveu algo para este id, o que degrada em
   // silencio (rotulo "--") para um modelo que nunca chama setBehaviorLabel.
   const mixr::xboard::Readout board{mixr::xboard::get(s.id)};
   s.behaviorLabel = board.label;
   s.decisions = board.decisions;
   s.threadTag = board.threadTag;
   s.hasAlert = board.alertValid;
   s.alertSender = board.alertSender;
   s.alertContact = board.alertContact;

   return s;
}

}

DashboardState captureState(mixr::models::WorldModel* const worldModel,
                            mixr::simulation::Station* const station,
                            const mixr::xtacview::TacviewOutput* const tacviewOutput,
                            const double wallSec, const double simSec,
                            const mixr::xclock::ClockStation* const clockStation,
                            const int numTcThreads, const std::string& scenarioLabel,
                            const std::vector<ClassStat>& previousClassStats)
{
   DashboardState state;
   state.scenarioLabel = scenarioLabel;
   state.wallSec = wallSec;
   state.simSec = simSec;
   state.numTcThreads = numTcThreads;

   if (clockStation != nullptr) {
      state.timeScale = clockStation->getTimeScale();
      state.paused = clockStation->isPaused();
   }

   for (const auto player : discoverPlayers(worldModel)) {
      state.entities.push_back(captureEntity(player));
   }

   state.classStats = sampleMetaObjects(previousClassStats);

   if (worldModel != nullptr) {
      // getTerrain() non-const e protected -- so a versao const e API
      // publica (WorldModel.hpp).
      const auto* const constWorld{worldModel};
      state.background.terrainLoaded = (constWorld->getTerrain() != nullptr);
      state.background.refLatDeg = constWorld->getRefLatitude();
      state.background.refLonDeg = constWorld->getRefLongitude();

      // Contadores do executivo (Simulation) -- o OUTRO lado desta aba: o
      // frame de tempo critico, que nao roda neste laco. Ver o painel.
      state.background.execCycle = constWorld->cycle();
      state.background.execFrame = constWorld->frame();
      state.background.execPhase = constWorld->phase();
      state.background.execCounter = constWorld->getExecCounter();
      state.background.simTimeOfDaySec = constWorld->getSimTimeOfDay();
      state.background.playerCount = static_cast<int>(state.entities.size());
   }
   if (station != nullptr) {
      // ARMADILHA MEDIDA (nao redescobrir): Station::getNetworks() NAO e
      // pre-ref()'d. Ao contrario de getPlayers(), que o cabecalho do
      // framework marca assim, ele devolve o membro cru (Station.cpp:639-642
      // -- 'return networks;'). O unref() que estava aqui soltava uma
      // referencia nunca tomada, a 10 Hz: em poucos segundos o contador
      // chegava a zero e a PROPRIA Station estourava ao ref()-ar a lista em
      // processNetworkInputTasks() ('ExpInvalidRefCount'). So aparecia em
      // cenario com 'networks:' -- os tres cenarios proprios deste app nao
      // tem nenhum, e por isso ficou latente ate o ./app virar o runner das
      // pocs de DIS.
      const mixr::base::PairStream* const networks{station->getNetworks()};
      state.background.networkHandlerCount = (networks != nullptr)
         ? static_cast<int>(networks->entries()) : 0;
      state.background.networkRateHz = station->getNetworkRate();
      state.background.networkThreadRunning = station->doWeHaveTheNetThread();
      state.background.stationTcRateHz = station->getTimeCriticalRate();
      state.background.stationBgRateHz = station->getBackgroundRate();
      state.background.fastForwardRate = station->getFastForwardRate();
      state.background.tcThreadRunning = station->doWeHaveTheTcThread();

      // Duracao real do frame T/C -- so existe se alguem ligou
      // setTimingStatsEnabled(true) (DashboardLoop.cpp faz, uma vez).
      const mixr::base::Statistic* const timing{station->getTimingStats()};
      if (timing != nullptr && timing->getN() > 0) {
         state.background.tcTimingAvailable = true;
         state.background.tcFrameLastMs = timing->value() * 1000.0;
         state.background.tcFrameMeanMs = timing->mean() * 1000.0;
         state.background.tcFrameMaxMs = timing->maxValue() * 1000.0;
         state.background.tcFrameSamples = timing->getN();
      }
      state.background.bgThreadRunning = station->doWeHaveTheBgThread();
   }
   if (tacviewOutput != nullptr) {
      const auto& srv{tacviewOutput->telemetry()};
      auto& bg{state.background};
      bg.tacviewEnabled = true;
      bg.tacviewInitialized = tacviewOutput->isInitialized();
      bg.tacviewInitFailed = tacviewOutput->didInitFail();
      bg.tacviewListening = srv.isListening();
      bg.tacviewConnected = srv.isConnected();
      bg.tacviewRecording = srv.isRecording();
      bg.tacviewHost = srv.listenHost();
      bg.tacviewPort = srv.listenPort();
      bg.tacviewCallsign = srv.callsign();
      bg.tacviewFile = srv.recordingPath();
      bg.tacviewConnections = srv.connectionCount();
      bg.tacviewBytesSent = srv.bytesSent();
      bg.tacviewLines = srv.linesWritten();
      bg.tacviewFrames = srv.framesEmitted();
      bg.tacviewDeclared = tacviewOutput->declaredObjectCount();
      bg.tacviewIdentified = tacviewOutput->identifiedObjectCount();
      bg.tacviewStreamTime = tacviewOutput->currentStreamTime();
   }

   state.background.residentKb = residentKb();

   return state;
}

} // namespace app
