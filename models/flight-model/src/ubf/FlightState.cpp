#include "ubf/FlightState.hpp"

#include "xnative/AlertDatalink.hpp"
#include "xnative/RadarScan.hpp"

#include "xboard/Board.hpp"
#include "xtrack/TrackQuery.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(FlightState, "FlightState")
EMPTY_SLOTTABLE(FlightState)
EMPTY_DELETEDATA(FlightState)

namespace {
const double RAD2DEG{57.295779513082320876798154814105};
}

FlightState::FlightState()
{
   STANDARD_CONSTRUCTOR()
}

void FlightState::copyData(const FlightState& org, const bool)
{
   BaseClass::copyData(org);
   snap = org.snap;
}

//------------------------------------------------------------------------------
// updateState() -- percepcao, lendo SO acessores nativos:
//
//   combustivel/mach/G/AoA  -> AirVehicle (que delega ao dynamics model)
//   contato                 -> xtrack::nearestHostileTrack() (radar nativo)
//   alerta                  -> Player::getDatalink()
//
// Nenhum updateSystemPointers() nosso: o Player ja resolve DynamicsModel,
// OnboardComputer e Datalink POR TIPO, sozinho.
//------------------------------------------------------------------------------
void FlightState::updateState(const base::Component* const actor)
{
   BaseClass::updateState(actor);

   const auto air = dynamic_cast<const models::AirVehicle*>(actor);
   if (air == nullptr) {
      snap = Snapshot{};
      return;
   }

   Snapshot s;
   s.valid = true;

   const base::Vec3d& pos{air->getPosition()};
   s.northM = pos[models::Player::INORTH];
   s.eastM = pos[models::Player::IEAST];
   s.altitudeM = air->getAltitudeM();
   s.headingDeg = air->getHeadingD();
   s.speedKts = air->getTotalVelocityKts();
   s.rollDeg = air->getRollD();
   s.pitchDeg = air->getPitchD();

   // Referencia de solo, tambem pela pilha nativa: quem consulta o banco de
   // elevacao do WorldModel e o proprio Player::updateElevation(), na fase de
   // BACKGROUND (Player.cpp:630, dentro de updateData()) -- nao numa das
   // quatro fases do frame de tempo critico. Consequencia pratica: onde a
   // decisao roda na fase 3 a 50 Hz contra um background de 10 Hz (ver a poc
   // multi-thread), este valor pode estar ate 100 ms velho (~8 m percorridos), o que
   // e irrelevante para um piso com centenas de metros de folga. E continua
   // deterministico: em -deterministic o laco faz tcFrame() e updateData()
   // em sequencia no mesmo passo, com qualquer numero de threads T/C.
   //
   // ARMADILHA: terrainValid NAO garante cobertura. O updateElevation()
   // nativo ignora o retorno de getElevation(), entao uma aeronave fora da
   // celula do tile recebe elevacao 0.0 com o flag LIGADO. Quem trata isso e
   // o piso absoluto de domain/TerrainFloor.hpp, nao este campo.
   s.terrainValid = air->isTerrainElevationValid();
   s.terrainElevM = air->getTerrainElevationM();
   s.altitudeAglM = air->getAltitudeAglM();

   // Telemetria do 6-DOF -- tudo via AirVehicle, que repassa ao JSBSimModel
   const double fuelMax{air->getFuelWtMax()};
   s.fuelFraction = (fuelMax > 0.0) ? (air->getFuelWt() / fuelMax) : 1.0;
   s.mach = air->getMach();
   s.gLoad = air->getGload();
   s.alphaDeg = air->getAngleOfAttack() * RAD2DEG;

   // --- contato: pista do radar NATIVO (Antenna/Tws -> AirTrkMgr) ---
   const xtrack::TrackInfo track{xtrack::nearestHostileTrack(air)};
   if (track.found) {
      s.hasContact = true;
      s.contactName = track.name;
      s.contactRangeM = track.rangeM;
      s.contactRelBearingDeg = track.relBearingDeg;
      s.contactDeltaAltM = track.deltaAltM;

      // A pista vem RELATIVA ao ownship; somando a nossa posicao sai a
      // posicao absoluta, que e o que vai no alerta para os outros avioes.
      s.contactNorthM = s.northM + track.relNorthM;
      s.contactEastM = s.eastM + track.relEastM;
      s.contactAltitudeM = s.altitudeM + track.deltaAltM;
   }

   // --- alerta recebido pelo datalink NATIVO ---
   const auto datalink = dynamic_cast<const AlertDatalink*>(air->getDatalink());
   if (datalink != nullptr) {
      const auto alert = datalink->getAlert();
      s.hasAlert = alert.valid;
      if (alert.valid) {
         s.alertSender = alert.senderName;
         s.alertContactName = alert.contactName;
         s.alertNorthM = alert.northM;
         s.alertEastM = alert.eastM;
         s.alertAltitudeM = alert.altitudeM;
         s.alertRangeM = alert.rangeM;
      }
   }

   snap = s;

   // Para onde a antena esta apontando AGORA -- publicado no quadro de leitura
   // para o host empurrar ao Tacview.
   //
   // Isto e do MODELO e nao do host: quem sabe o que a aeronave esta
   // enxergando e quem percebe. O host so relaia o que o quadro disser, e se
   // um modelo nunca publicar, ele simplesmente nao desenha varredura -- um
   // modelo sem radar e legitimo.
   //
   // Nao entra no dump deterministico: a varredura so alimenta o caminho de
   // tempo real (ver app/RealTimeRun.cpp).
   const RadarScanInfo scan{radarScanOf(air)};
   xboard::setRadarScan(air->getID(), scan.found, scan.azimuthDeg, scan.elevationDeg,
                        scan.rangeM, scan.horizontalBeamwidthDeg, scan.verticalBeamwidthDeg);
}

} // namespace xnative
} // namespace mixr
