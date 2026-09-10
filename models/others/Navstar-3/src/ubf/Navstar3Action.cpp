#include "ubf/Navstar3Action.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace models {
namespace xNavstar_3 {

IMPLEMENT_SUBCLASS(Navstar3Action, "Navstar3Action")
EMPTY_SLOTTABLE(Navstar3Action)
EMPTY_DELETEDATA(Navstar3Action)

Navstar3Action::Navstar3Action()
{
   STANDARD_CONSTRUCTOR()
}

Navstar3Action::Navstar3Action(const std::string& l, const base::Vec3d& pos, const base::Vec3d& vel)
   : label(l), ecefPosM(pos), ecefVelMps(vel)
{
   STANDARD_CONSTRUCTOR()
}

void Navstar3Action::copyData(const Navstar3Action& org, const bool)
{
   BaseClass::copyData(org);
   label = org.label;
   ecefPosM = org.ecefPosM;
   ecefVelMps = org.ecefVelMps;
}

//------------------------------------------------------------------------------
// execute() -- move o player (posicao ECEF calculada pelo propagador orbital,
// 'slaved=true' desliga o integrador cinematico nativo) e faz as DUAS
// chamadas obrigatorias ao xboard. Ver CONTRATO.md secao 3 e o cabecalho
// deste arquivo.
//------------------------------------------------------------------------------
bool Navstar3Action::execute(base::Component* const actor)
{
   const auto player = dynamic_cast<models::Player*>(actor);
   if (player == nullptr) return false;

   player->setGeocPosition(ecefPosM, /*slaved=*/true);
   player->setGeocVelocity(ecefVelMps);

   xboard::setBehaviorLabel(player->getID(), label);
   xboard::bumpDecisionCount(player->getID());
   return true;
}

} // namespace xNavstar_3
} // namespace models
} // namespace mixr
