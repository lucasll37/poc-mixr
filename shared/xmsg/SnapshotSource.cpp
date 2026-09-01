#include "xmsg/SnapshotSource.hpp"

#include "mixr/models/Track.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/OnboardComputer.hpp"
#include "mixr/models/system/trackmanager/TrackManager.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/safe_ptr.hpp"

#include <algorithm>
#include <cstring>

namespace mixr {
namespace xmsg {

namespace {

const int MAX_TRACKS{20};

void copyName(char* const dst, const std::size_t cap, const char* const src)
{
   if (src == nullptr) { dst[0] = '\0'; return; }
   std::strncpy(dst, src, cap - 1);
   dst[cap - 1] = '\0';
}

const char* sideName(const models::Player::Side s)
{
   switch (s) {
      case models::Player::BLUE:   return "blue";
      case models::Player::RED:    return "red";
      case models::Player::YELLOW: return "yellow";
      case models::Player::CYAN:   return "cyan";
      case models::Player::GRAY:   return "gray";
      case models::Player::WHITE:  return "white";
      default:                     return "?";
   }
}

const char* modeName(const simulation::AbstractPlayer::Mode m)
{
   switch (m) {
      case simulation::AbstractPlayer::INACTIVE:       return "inactive";
      case simulation::AbstractPlayer::ACTIVE:         return "active";
      case simulation::AbstractPlayer::KILLED:         return "killed";
      case simulation::AbstractPlayer::CRASHED:        return "crashed";
      case simulation::AbstractPlayer::DETONATED:      return "detonated";
      case simulation::AbstractPlayer::PRE_RELEASE:    return "preRelease";
      case simulation::AbstractPlayer::LAUNCHED:       return "launched";
      case simulation::AbstractPlayer::DELETE_REQUEST: return "deleteRequest";
      default:                                         return "?";
   }
}

//------------------------------------------------------------------------------
// Motor -- a parte com mais armadilha do arquivo inteiro.
//
// 1) So existe com AerodynamicsModel por tras (na pratica, JSBSimModel).
//    RacModel e LaeroModel devolvem contagem zero, e o player recebido por DIS
//    e clonado de um template SEM dynamicsModel -- por isso a validade do
//    grupo depende de getNumberOfEngines() > 0, e nao de o cast ter dado certo.
//
// 2) getEngRPM() e polimorfico na unidade: RPM absoluto em pistao e eletrico,
//    %N2 em turbina, %N1 em turboprop. Nao ha nome fixo correto -- dai
//    'engRpmRaw', e dai o empuxo (lbf em todo tipo) ser o sinal primario.
//
// 3) O switch de getEngRPM() tem um 'default:' que NAO ESCREVE no array mas
//    ainda conta o indice no retorno. Por isso os arrays sao zerados antes.
//------------------------------------------------------------------------------
void fillEngine(Snapshot& snap, const models::AirVehicle* const air)
{
   double thrust[MAX_ENGINES]{};
   double rpm[MAX_ENGINES]{};
   double ff[MAX_ENGINES]{};
   double oil[MAX_ENGINES]{};
   double pla[MAX_ENGINES]{};

   const int n{air->getNumberOfEngines()};
   if (n <= 0) return;                       // grupo continua invalido

   const int nt{air->getEngThrust(thrust, MAX_ENGINES)};
   air->getEngRPM(rpm, MAX_ENGINES);
   air->getEngFuelFlow(ff, MAX_ENGINES);
   air->getEngOilPressure(oil, MAX_ENGINES);
   air->getEngPLA(pla, MAX_ENGINES);

   const int count{std::min(n, MAX_ENGINES)};

   double total{}, minThrust{}, maxThrust{}, minRpm{}, totalFf{}, minOil{}, minPla{};
   int worst{};

   for (int i = 0; i < count; i++) {
      total += thrust[i];
      totalFf += ff[i];
      if (i == 0 || thrust[i] < minThrust) { minThrust = thrust[i]; worst = i + 1; }
      if (i == 0 || thrust[i] > maxThrust) maxThrust = thrust[i];
      if (i == 0 || rpm[i] < minRpm) minRpm = rpm[i];
      if (i == 0 || oil[i] < minOil) minOil = oil[i];
      if (i == 0 || pla[i] < minPla) minPla = pla[i];
   }

   snap.v[F_engCount] = static_cast<double>(n);
   snap.v[F_engThrustTotalLb] = total;
   snap.v[F_engThrustMinLb] = minThrust;
   snap.v[F_engRpmMinRaw] = minRpm;
   snap.v[F_engFuelFlowTotalPph] = totalFf;
   snap.v[F_engOilPsiMin] = minOil;
   snap.v[F_engPlaMinPct] = minPla;

   // Assimetria de empuxo: 0 simetrico, ->1 um motor morto. Adimensional, e
   // por isso o unico indicador de pane que nao precisa de calibracao por
   // aeronave. Com um motor so, ou com todos parados, nao ha assimetria a
   // medir -- e zero, nao "pane".
   snap.v[F_engThrustAsymFrac] =
      (count > 1 && maxThrust > 0.0) ? ((maxThrust - minThrust) / maxThrust) : 0.0;

   // Qual motor esta pior so faz sentido se houver assimetria de verdade.
   snap.v[F_engWorstIndex] =
      (snap.v[F_engThrustAsymFrac] > 0.0) ? static_cast<double>(worst) : 0.0;

   for (int i = 0; i < MAX_ENGINES; i++) {
      snap.v[F_eng1ThrustLb + i] = (i < count) ? thrust[i] : 0.0;
      snap.v[F_eng1RpmRaw + i] = (i < count) ? rpm[i] : 0.0;
   }

   // 'nt' so entra na conta de validade: se o framework disse que ha n motores
   // mas nao preencheu nenhum empuxo, o grupo nao vale.
   if (nt > 0) snap.mark(Group::Engine);
}

void fillTrack(Snapshot& snap, const models::AirVehicle* const air,
               const std::string& trackManagerName)
{
   if (trackManagerName.empty()) return;

   // Mesmo const_cast confinado de shared/xtrack/TrackQuery.cpp: a consulta e de
   // leitura, mas o getter nao e const no framework.
   const auto obc = const_cast<models::OnboardComputer*>(air->getOnboardComputer());
   if (obc == nullptr) return;

   models::TrackManager* const trkMgr{obc->getTrackManagerByName(trackManagerName.c_str())};
   if (trkMgr == nullptr) return;

   base::safe_ptr<models::Track> tracks[MAX_TRACKS];
   const int n{trkMgr->getTrackList(tracks, MAX_TRACKS)};

   const models::Track* best{};
   int hostis{};
   for (int i = 0; i < n; i++) {
      const models::Track* const trk{tracks[i]};
      if (trk == nullptr) continue;

      const models::Player* const tgt{trk->getTarget()};
      if (tgt != nullptr && tgt->getSide() == air->getSide()) continue;
      ++hostis;

      // Desempate por menor trackID: sem isso a escolha dependeria da ordem
      // da lista do TrackManager, e a saida deixaria de ser deterministica.
      if (best == nullptr
          || trk->getRange() < best->getRange()
          || (trk->getRange() == best->getRange() && trk->getTrackID() < best->getTrackID())) {
         best = trk;
      }
   }

   snap.v[F_trackCount] = static_cast<double>(hostis);
   if (best != nullptr) {
      snap.v[F_trackRangeM] = best->getRange();
      snap.v[F_trackBearingDeg] = best->getRelAzimuthD();
      snap.v[F_trackDeltaAltM] = -best->getPosition()[models::Player::IDOWN];

      const models::Player* const tgt{best->getTarget()};
      if (tgt != nullptr && tgt->getName() != nullptr) {
         copyName(snap.trackName, NAME_LEN, tgt->getName()->getString());
      }
   }
   snap.mark(Group::Track);
}

} // namespace

void fillSnapshot(Snapshot& snap, models::Player* const p, const unsigned groupMask,
                  const std::string& trackManagerName)
{
   snap = Snapshot{};
   if (p == nullptr) return;

   if (p->getName() != nullptr) copyName(snap.playerName, NAME_LEN, p->getName()->getString());
   copyName(snap.sideName, SHORT_LEN, sideName(p->getSide()));
   copyName(snap.modeName, SHORT_LEN, modeName(p->getMode()));

   if (groupMask & groupBit(Group::Ident)) {
      snap.v[F_playerId] = static_cast<double>(p->getID());
      snap.v[F_sideNum] = static_cast<double>(p->getSide());
      snap.v[F_modeNum] = static_cast<double>(p->getMode());
      snap.mark(Group::Ident);
   }

   if (groupMask & groupBit(Group::Kinematics)) {
      const base::Vec3d& pos{p->getPosition()};
      const base::Vec3d& vel{p->getVelocity()};

      snap.v[F_latDeg] = p->getLatitude();
      snap.v[F_lonDeg] = p->getLongitude();
      snap.v[F_northM] = pos[models::Player::INORTH];
      snap.v[F_eastM] = pos[models::Player::IEAST];
      snap.v[F_altMslM] = p->getAltitudeM();
      snap.v[F_altAglM] = p->getAltitudeAglM();
      snap.v[F_terrainElevM] = p->getTerrainElevationM();
      snap.v[F_hdgDeg] = p->getHeadingD();
      snap.v[F_pitchDeg] = p->getPitchD();
      snap.v[F_rollDeg] = p->getRollD();
      snap.v[F_speedKts] = p->getTotalVelocityKts();
      // NED: 'down' e positivo para baixo, entao subir e o negativo dele.
      snap.v[F_climbMps] = -vel[models::Player::IDOWN];
      snap.mark(Group::Kinematics);
   }

   if (groupMask & groupBit(Group::Status)) {
      snap.v[F_damage] = p->getDamage();
      snap.v[F_crashedFlag] = p->isCrashed() ? 1.0 : 0.0;
      snap.v[F_killedFlag] = p->isKilled() ? 1.0 : 0.0;
      snap.v[F_smokeFrac] = p->getSmoke();
      snap.v[F_flamesFrac] = p->getFlames();
      snap.mark(Group::Status);
   }

   // O resto so existe em aeronave. Um player que nao seja AirVehicle deixa
   // esses grupos invalidos -- de novo: 'null', nunca zero.
   const auto air = dynamic_cast<const models::AirVehicle*>(p);
   if (air == nullptr) return;

   if (groupMask & groupBit(Group::AirData)) {
      snap.v[F_machNum] = air->getMach();
      snap.v[F_gLoad] = air->getGload();
      snap.v[F_aoaDeg] = air->getAngleOfAttackD();
      snap.mark(Group::AirData);
   }

   if (groupMask & groupBit(Group::Fuel)) {
      const double max{air->getFuelWtMax()};
      snap.v[F_fuelLb] = air->getFuelWt();
      snap.v[F_fuelMaxLb] = max;
      snap.v[F_fuelFrac] = (max > 0.0) ? (air->getFuelWt() / max) : 0.0;
      if (max > 0.0) snap.mark(Group::Fuel);
   }

   if (groupMask & groupBit(Group::Engine)) fillEngine(snap, air);
   if (groupMask & groupBit(Group::Track)) fillTrack(snap, air, trackManagerName);
}

} // namespace xmsg
} // namespace mixr
