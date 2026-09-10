#include "xnative/Paratrooper.hpp"

#include "xlog/Log.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {

IMPLEMENT_SUBCLASS(Paratrooper, "Paratrooper")

// clang-format off
BEGIN_SLOTTABLE(Paratrooper)
   "canopyDescentRate",   // 1
END_SLOTTABLE(Paratrooper)

BEGIN_SLOT_MAP(Paratrooper)
   ON_SLOT(1, setSlotCanopyDescentRate, base::Number)
END_SLOT_MAP()

Paratrooper::Paratrooper()
// clang-format on
{
   STANDARD_CONSTRUCTOR()

   // Tipo default "PARATROOPER" -- mesma convencao de
   // models/players/C-130/include/xnative/ParatrooperPlaceholder.hpp, para
   // que esta classe seja um substituto de EDL direto do placeholder (o
   // Steerpoint action: do C-130 casa por Player::getType(), nunca por
   // dynamic_cast -- ver docs/ARCHITECTURE.md).
   static base::String generic("PARATROOPER");
   setType(&generic);

   // 'Effect::Effect()' ja reduziu o 'maxTOF' herdado de AbstractWeapon
   // (60 s) para 10 s -- curto demais para um salto inteiro (queda livre +
   // velame passa facilmente de 60-90 s). 300 s da folga generosa; quem
   // fica LANDED nunca chega a testar esse teto de qualquer jeito, porque
   // updateTOF() para de incrementar (ver abaixo).
   setMaxTOF(300.0);

   // dragIndex humano: Effect::weaponDynamics() converge para
   // v_terminal = g/dragIndex (equilibrio de arrasto-gravidade, g = ETHG*
   // FT2M =~ 9,8067 m/s2). O default de Effect (0,0006) da 16 km/s -- puro
   // balistico. 0,18 da ~54,5 m/s, queda livre humana ventre-para-baixo
   // plausivel.
   setDragIndex(0.18);

   // Effect::Effect() herda de AbstractWeapon um raio letal (50 m) e um
   // alcance maximo de estouro (500 m) pensados para uma ARMA -- sem
   // sentido para um soldado. Zerados aqui; checkDetonationEffect() nunca
   // e chamado por este modelo mesmo assim (ver crashNotification()
   // abaixo), mas zerar os dois documenta a intencao e neutraliza qualquer
   // caminho nativo futuro que venha a consulta-los.
   setLethalRange(0.0);
   setMaxBurstRng(0.0);
}

void Paratrooper::copyData(const Paratrooper& org, const bool)
{
   BaseClass::copyData(org);
   stage_ = org.stage_;
   canopyDescentRateMps_ = org.canopyDescentRateMps_;
}

EMPTY_DELETEDATA(Paratrooper)

const char* Paratrooper::getDescription() const   { return "Paratrooper"; }
const char* Paratrooper::getNickname() const      { return "Trooper"; }

//------------------------------------------------------------------------------
// setJumpStage() -- log de transicao SO na borda (edge-detected). Trivial
// aqui porque este objeto e' o LAR PERSISTENTE do estagio (ao contrario de
// models/players/A-4/src/ubf/FlightAction.cpp, que precisou de um mapa
// estatico por player-id porque o objeto Action e' recriado a cada decisao).
//------------------------------------------------------------------------------
void Paratrooper::setJumpStage(const domain::Stage s)
{
   if (s == stage_) return;

   LOG(INFO) << "[Paratrooper] id=" << getID() << ": " << domain::labelOf(stage_)
             << " -> " << domain::labelOf(s);
   stage_ = s;
}

//------------------------------------------------------------------------------
// weaponDynamics() -- despacha por estagio. Chamado a partir de
// AbstractWeapon::dynamics(), na fase 0 do frame de tempo critico, ANTES de
// Player::dynamics()/positionUpdate() integrar a posicao com a velocidade
// que este metodo acabou de fixar.
//------------------------------------------------------------------------------
void Paratrooper::weaponDynamics(const double dt)
{
   switch (stage_) {
      case domain::Stage::FREEFALL:
         // A fisica nativa do Effect -- arrasto linear + gravidade, com o
         // dragIndex humano fixado no construtor.
         BaseClass::weaponDynamics(dt);
         break;

      case domain::Stage::CANOPY:
         // Reta pra baixo, taxa constante -- sem deriva de vento (limitacao
         // documentada, nao bug: ver docs/ARCHITECTURE.md). A atitude e'
         // fixada ANTES da velocidade porque setEulerAngles() muta a matriz
         // de rotacao que setVelocity() usa para recalcular a velocidade em
         // eixos do corpo.
         setEulerAngles(0.0, 0.0, getHeadingR());
         setVelocity(0.0, 0.0, canopyDescentRateMps_);
         setAcceleration(0.0, 0.0, 0.0);
         setAngularVelocities(0.0, 0.0, 0.0);
         break;

      case domain::Stage::LANDED:
         // Velocidade zero faz 'vp' (a magnitude total) zerar dentro de
         // setVelocity() -- e' o que faz o guard 'vp > 0' de
         // Player::positionUpdate() virar um no-op permanente, travando a
         // posicao SEM tocar setMode()/sem passar pela cascata de
         // crashNotification() (ver o cabecalho da classe).
         setEulerAngles(0.0, 0.0, getHeadingR());
         setVelocity(0.0, 0.0, 0.0);
         setAcceleration(0.0, 0.0, 0.0);
         setAngularVelocities(0.0, 0.0, 0.0);
         break;
   }
}

//------------------------------------------------------------------------------
// updateTOF() -- para de incrementar (e portanto nunca expira 'maxTOF') uma
// vez LANDED. O mecanismo de TOF de AbstractWeapon e' puramente TEMPORAL,
// independente da AGL -- sem esta guarda, um paraquedista parado no chao
// numa simulacao mais longa que 'maxTOF' se autodetonaria (setMode(DETONATED)
// dentro de AbstractWeapon::updateTOF()) mesmo tendo pousado em seguranca.
//------------------------------------------------------------------------------
void Paratrooper::updateTOF(const double dt)
{
   if (stage_ == domain::Stage::LANDED) return;
   BaseClass::updateTOF(dt);
}

//------------------------------------------------------------------------------
// crashNotification()/collisionNotification() -- a rede de seguranca contra
// o CRASH_EVENT generico de Player::dynamics() (AGL < 0 e major type WEAPON,
// ver Player.cpp) disparar antes do ciclo de decisao ter tempo de reagir (um
// frame de atraso e' possivel -- ver docs/ARCHITECTURE.md). SEM esta
// sobrescrita, 'Effect::crashNotification()' (que NAO respeita
// 'crashOverride', ao contrario de 'AbstractWeapon::crashNotification()')
// chamaria killedNotification() -- KILL_EVENT para os subcomponentes, dano/
// fumaca/chamas em 1.0 -- e detonaria (setMode(DETONATED)). Errado para um
// pouso seguro: aqui vira so' LANDED, sem cascata nenhuma.
//
// 'isCrashOverride()' e' respeitado mesmo assim (restaura o contrato que
// Effect quebra) -- um EDL com 'crashOverride: true' continua fazendo o
// crash generico virar no-op total, sem nem transicionar para LANDED.
//------------------------------------------------------------------------------
bool Paratrooper::crashNotification()
{
   if (isCrashOverride()) return true;
   setJumpStage(domain::Stage::LANDED);
   return true;
}

bool Paratrooper::collisionNotification(mixr::models::Player* const)
{
   if (isCrashOverride()) return true;
   setJumpStage(domain::Stage::LANDED);
   return true;
}

bool Paratrooper::setSlotCanopyDescentRate(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   canopyDescentRateMps_ = msg->getReal();
   return true;
}

} // namespace xparatrooper
} // namespace models
} // namespace mixr
