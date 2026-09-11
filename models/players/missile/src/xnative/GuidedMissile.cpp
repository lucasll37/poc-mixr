#include "xnative/GuidedMissile.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/units/angle_utils.hpp"
#include "mixr/base/util/constants.hpp"

#include <algorithm>
#include <cmath>

namespace mixr {
namespace models {
namespace xmissile {

IMPLEMENT_SUBCLASS(GuidedMissile, "GuidedMissile")
EMPTY_SLOTTABLE(GuidedMissile)

namespace {
constexpr double kNavRatio{4.0};
}

GuidedMissile::GuidedMissile()
{
   STANDARD_CONSTRUCTOR()

   static base::String type("GuidedMissile");
   setType(&type);

   setMaxTOF(60.0);
   setLethalRange(30.0);
   setMaxBurstRng(150.0);
   setTSG(1.0);
   setSOBT(0.0);
   setEOBT(50.0);

   setVpMin(0.0);
   setVpMax(280.0);  // m/s (~544 kt) -- bem acima da velocidade de cruzeiro do A-4 (~260 kt)
   setVpMaxG(280.0);
   setMaxG(15.0);     // missil vira muito mais apertado que uma aeronave tripulada
   setMaxAccel(80.0);
}

void GuidedMissile::copyData(const GuidedMissile& org, const bool)
{
   BaseClass::copyData(org);

   cmdHeadingRad_ = org.cmdHeadingRad_;
   cmdPitchRad_ = org.cmdPitchRad_;
   cmdSpeedMps_ = org.cmdSpeedMps_;
   fuzeState_ = org.fuzeState_;
   detonatedLingerSec_ = org.detonatedLingerSec_;
}

void GuidedMissile::deleteData()
{
}

const char* GuidedMissile::getDescription() const { return "Guided"; }
const char* GuidedMissile::getNickname() const     { return "GuidedMissile"; }

//------------------------------------------------------------------------------
// updateTC() -- so' acrescenta o timer de remocao pos-detonacao; o resto
// (TOF, positionTracking(), a transicao PRE_RELEASE->ACTIVE) continua
// inteiramente a cargo de AbstractWeapon::updateTC(), via BaseClass.
//------------------------------------------------------------------------------
void GuidedMissile::updateTC(const double dt)
{
   BaseClass::updateTC(dt);

   if (isLocalPlayer() && isMode(DETONATED)) {
      // MESMO gating de fase que AbstractWeapon::updateTC() usa para o TOF
      // (fase 3, dt*4.0 -- o frame de tempo critico inteiro, nao so' o
      // quarto que corresponde a esta fase).
      const unsigned int ph{getWorldModel()->phase()};
      if (ph == 3) {
         detonatedLingerSec_ += dt * 4.0;
         if (detonatedLingerSec_ >= kLingerSec) {
            setMode(DELETE_REQUEST);
         }
      }
   }
}

//------------------------------------------------------------------------------
// weaponGuidance() -- le a posicao/velocidade do alvo DIRETO do Player*
// (mesmo padrao de Missile::calculateVectors() nativo -- nao usa o cache
// tgtPos/tgtVel de AbstractWeapon, que mistura convencao absoluta com
// relativa entre os dois campos). Delega a lei de guiagem inteira para
// domain::proportionalNavigation() (sem MIXR, testada isolada em
// tests/domain/test_Guidance.cpp) e guarda o comando para
// weaponDynamics() consumir no MESMO frame.
//------------------------------------------------------------------------------
void GuidedMissile::weaponGuidance(const double dt)
{
   const Player* const tgt{getTargetPlayer()};
   if (tgt == nullptr || !tgt->isActive()) return;

   const base::Vec3d& tgtPos{tgt->getPosition()};
   const base::Vec3d& tgtVel{tgt->getVelocity()};
   const base::Vec3d& ownPos{getPosition()};
   const base::Vec3d& ownVel{getVelocity()};

   const domain::Vec3 relPos{tgtPos.x() - ownPos.x(), tgtPos.y() - ownPos.y(), tgtPos.z() - ownPos.z()};
   const domain::Vec3 relVel{tgtVel.x() - ownVel.x(), tgtVel.y() - ownVel.y(), tgtVel.z() - ownVel.z()};

   if (isGuidanceEnabled()) {
      const domain::GuidanceGains gains{kNavRatio, /*cruiseSpeedMps=*/getVpMax()};
      const auto cmd{domain::proportionalNavigation(relPos, relVel, gains)};
      cmdHeadingRad_ = cmd.cmdHeadingRad;
      cmdPitchRad_ = cmd.cmdPitchRad;
      cmdSpeedMps_ = cmd.cmdSpeedMps;
   }

   // Espoleta de proximidade -- roda independente de isGuidanceEnabled()
   // (mesmo padrao do Missile nativo): usa o alcance/velocidade relativa de
   // VERDADE, nao o comando -- um missil ainda sem guiagem ligada (tof <
   // tsg) pode passar perto o bastante do alvo por trajetoria balistica.
   if (!isDummy() && getTOF() > 2.0) {
      const auto outcome{domain::proximityFuze(relPos, relVel, getMaxBurstRng(), fuzeState_)};
      fuzeState_ = outcome.nextState;

      if (outcome.closestApproachReached) {
         setMode(DETONATED);
         if (outcome.hit) {
            setDetonationResults(DETONATE_ENTITY_IMPACT);
            checkDetonationEffect();
         } else {
            // passou do ponto de menor aproximacao sem acertar --
            // autodestruicao, mesmo comportamento do Missile nativo.
            setDetonationResults(DETONATE_DETONATION);
            setTargetPlayer(nullptr, false);
            setTargetTrack(nullptr, false);
         }
      }
   }
}

//------------------------------------------------------------------------------
// weaponDynamics() -- integra heading/pitch/velocidade em direcao ao
// comando guardado por weaponGuidance(), limitado por taxa de giro (de
// maxG, herdado de Missile) e por aceleracao (maxAccel, idem). NAO integra
// posicao: confirmado lendo AbstractWeapon::dynamics() do fork vendorizado
// -- ele chama weaponGuidance()+weaponDynamics() e, logo em seguida,
// BaseClass::dynamics(dt) (Player::dynamics()), que SEMPRE chama
// positionUpdate(dt), integrando a posicao a partir da velocidade que
// acabamos de escrever aqui. Escrever a posicao nos dois lugares
// duplicaria a integracao.
//------------------------------------------------------------------------------
void GuidedMissile::weaponDynamics(const double dt)
{
   const double speed{std::max(getTotalVelocity(), 1.0)};

   // g em METROS/s^2 (base::ETHGM), nao base::ETHG (pes/s^2) -- este missil
   // trabalha em m/s do inicio ao fim; a constante em pes daria uma taxa de
   // giro maxima ~3,28x errada.
   const double maxTurnRateRadPerS{(getMaxG() * base::ETHGM) / speed};

   double dPitch{base::angle::aepcdRad(cmdPitchRad_ - getPitchR())};
   dPitch = std::clamp(dPitch, -maxTurnRateRadPerS * dt, maxTurnRateRadPerS * dt);
   const double newPitch{getPitchR() + dPitch};

   double dHeading{base::angle::aepcdRad(cmdHeadingRad_ - getHeadingR())};
   dHeading = std::clamp(dHeading, -maxTurnRateRadPerS * dt, maxTurnRateRadPerS * dt);
   double newHeading{getHeadingR() + dHeading};
   if (newHeading < 0.0) newHeading += 2.0 * base::PI;
   if (newHeading >= 2.0 * base::PI) newHeading -= 2.0 * base::PI;

   // Angulo de banco so' cosmetico (proporcional a fracao da taxa de giro
   // maxima em uso) -- nao alimenta a dinamica, so' deixa o Tacview mostrar
   // o missil inclinado na curva.
   const double bankRad{(maxTurnRateRadPerS > 1.0e-9 && dt > 0.0)
      ? std::clamp((dHeading / dt) / maxTurnRateRadPerS, -1.0, 1.0) * (60.0 * base::angle::D2RCC)
      : 0.0};

   setEulerAngles(bankRad, newPitch, newHeading);

   double dSpeed{cmdSpeedMps_ - getTotalVelocity()};
   dSpeed = std::clamp(dSpeed, -getMaxAccel() * dt, getMaxAccel() * dt);
   const double newSpeed{getTotalVelocity() + dSpeed};

   const double cosPitch{std::cos(newPitch)};
   const double vN{newSpeed * cosPitch * std::cos(newHeading)};
   const double vE{newSpeed * cosPitch * std::sin(newHeading)};
   const double vD{-newSpeed * std::sin(newPitch)};
   setVelocity(vN, vE, vD);
}

} // namespace xmissile
} // namespace models
} // namespace mixr
