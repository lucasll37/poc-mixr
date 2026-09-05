#include "xmissile/GuidedMissile.hpp"

#include "domain/Guidance.hpp"
#include "events/EventTokens.hpp"
#include "xlog/Log.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/base/osg/Vec3d"

namespace mixr {
namespace xmissile {

IMPLEMENT_SUBCLASS(GuidedMissile, "GuidedMissile")
EMPTY_SLOTTABLE(GuidedMissile)

GuidedMissile::GuidedMissile()
{
   STANDARD_CONSTRUCTOR()
}

void GuidedMissile::copyData(const GuidedMissile& org, const bool)
{
   BaseClass::copyData(org);
   detonatedTof_ = org.detonatedTof_;
   lastRollNorm_ = org.lastRollNorm_;
   lastPitchNorm_ = org.lastPitchNorm_;
}

void GuidedMissile::deleteData()
{
}

const char* GuidedMissile::getDescription() const
{
   return "Guided missile (academic demo, 6-DOF via JSBSimModel)";
}

const char* GuidedMissile::getNickname() const   { return "GuidedMissile"; }
int GuidedMissile::getCategory() const           { return GUIDED | MISSILE; }

bool GuidedMissile::collisionNotification(models::Player* const p)
{
   const bool r{BaseClass::collisionNotification(p)};
   if (isMode(DETONATED) && detonatedTof_ < 0.0) detonatedTof_ = 0.0;
   return r;
}

bool GuidedMissile::crashNotification()
{
   const bool r{BaseClass::crashNotification()};
   if (isMode(DETONATED) && detonatedTof_ < 0.0) detonatedTof_ = 0.0;
   return r;
}

//------------------------------------------------------------------------------
// guide() -- calcula o vetor NED ate o alvo (mesma origem para os dois:
// "posicao a partir do ponto de referencia do WorldModel", Player.hpp),
// pede o comando a domain::pursuit() e o aplica DIRETO no FCS do JSBSim, por
// fora do Autopilot nativo -- este player nao tem ( Autopilot ), o
// dynamicsModel recebe o comando de manche/manete igual a um joystick
// fisico (mesma faixa -1..1, ver shared/xjoystick).
//
// domain::slewTowards() e o amortecimento que falta na lei P pura (ver o
// "porque" em domain/Guidance.hpp e no comentario de aim1.xml sobre a
// inercia) -- sem ele o comando pula de 0 a +-1 no primeiro frame com erro
// de rumo, e a resposta da aeronave diverge antes de o proximo tick
// corrigir.
//------------------------------------------------------------------------------
namespace {
constexpr double RAD2DEG{57.295779513082320876798154814105};
}

void GuidedMissile::guide(const double dt)
{
   const auto target = getTargetPlayer();
   if (target == nullptr || getDynamicsModel() == nullptr) return;

   const base::Vec3d rel{target->getPosition() - getPosition()};
   // [P,Q,R] = [rolagem,arfagem,guinada], corpo, rad/s -- ver o "porque" do
   // termo de taxa em domain/Guidance.hpp.
   const base::Vec3d& rates{getAngularVelocities()};
   const domain::GuidanceCommand cmd{
      domain::pursuit(rel[INORTH], rel[IEAST], rel[IDOWN], getHeadingD(), getPitchD(),
                      rates[0] * RAD2DEG, rates[1] * RAD2DEG)
   };

   const double maxDelta{kMaxSlewPerSec * dt};
   lastRollNorm_ = domain::slewTowards(lastRollNorm_, cmd.rollNorm, maxDelta);
   lastPitchNorm_ = domain::slewTowards(lastPitchNorm_, cmd.pitchNorm, maxDelta);

   setControlStickRollInput(lastRollNorm_);
   setControlStickPitchInput(lastPitchNorm_);

   const double full{1.0};
   setThrottles(&full, 1);
}

//------------------------------------------------------------------------------
// dynamics() -- HOOK CERTO para um Player-arma com DynamicsModel anexado (ver
// o "porque" no header): AbstractWeapon::dynamics() so chama
// weaponGuidance()/weaponDynamics() quando getDynamicsModel() == nullptr, que
// nao e o nosso caso. A guiagem entra ANTES do BaseClass::dynamics(dt), que e
// quem de fato integra o JSBSim no frame com o comando ja aplicado.
//------------------------------------------------------------------------------
void GuidedMissile::dynamics(const double dt)
{
   if (isMode(ACTIVE)) guide(dt);

   BaseClass::dynamics(dt);
}

//------------------------------------------------------------------------------
// updateTC() -- so o timer de permanencia pos-detonacao mora aqui, e nao em
// dynamics(): ver o "porque" (confirmado rodando) no header.
//
// updateTC() e chamado uma vez POR FASE (4 fases por frame de tempo
// critico, cada uma com dt = dt_do_frame/4 -- ver Simulation::updateTC(),
// que faz updateTcPlayerList(..., dt0/4.0, ...) quatro vezes, uma por fase)
// -- por isso o acumulador so soma na fase 3, o mesmo ponto onde
// AbstractWeapon::updateTC() atualiza o TOF (contexts/src/mixr/src/models/
// player/weapon/AbstractWeapon.cpp:239-247), e pela MESMA razao multiplica
// por 4: sem os dois, o timer ou soma 4x por frame (sem a guarda de fase)
// ou soma 1/4 do dt de frame por vez (sem a multiplicacao) -- confirmado
// rodando: sem o *4.0, kLingerSec=2s levava quase 8s de tempo simulado
// para disparar.
//------------------------------------------------------------------------------
void GuidedMissile::updateTC(const double dt)
{
   BaseClass::updateTC(dt);

   if (isMode(DETONATED) && getWorldModel() != nullptr && getWorldModel()->phase() == 3) {
      if (detonatedTof_ < 0.0) detonatedTof_ = 0.0;
      detonatedTof_ += dt * 4.0;
      if (detonatedTof_ > kLingerSec) setMode(DELETE_REQUEST);
   }
}

//------------------------------------------------------------------------------
// EID_ALERT -- despacho pela macro de evento nativa (BEGIN_EVENT_HANDLER
// expande um override de event()), NAO por um hook ja nomeado do framework
// (ao contrario de AlertDatalink::onDatalinkMessageEvent()): este player nao
// tem Datalink nenhum, entao o UNICO jeito de ser alcancado e o broadcast
// direto que xnative::AlertDatalink::broadcastAlert() faz com
// player->event(events::EID_ALERT, msg) (ver events/README.md e o
// comentario correspondente em models/player/A4).
//------------------------------------------------------------------------------
BEGIN_EVENT_HANDLER(GuidedMissile)
   ON_EVENT_OBJ(events::EID_ALERT, onAlertEvent, events::TacticalAlert)
END_EVENT_HANDLER()

bool GuidedMissile::onAlertEvent(events::TacticalAlert* const ev)
{
   LOG(INFO) << "GuidedMissile viu alerta de " << ev->getSenderName()
             << " sobre " << ev->getContactName();
   return true;
}

} // namespace xmissile
} // namespace mixr
