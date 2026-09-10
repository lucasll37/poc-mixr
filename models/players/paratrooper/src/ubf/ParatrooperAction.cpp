#include "ubf/ParatrooperAction.hpp"

#include "domain/ParachuteFsm.hpp"
#include "xnative/Paratrooper.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {

IMPLEMENT_SUBCLASS(ParatrooperAction, "ParatrooperAction")
EMPTY_SLOTTABLE(ParatrooperAction)
EMPTY_DELETEDATA(ParatrooperAction)

ParatrooperAction::ParatrooperAction()
{
   STANDARD_CONSTRUCTOR()
}

ParatrooperAction::ParatrooperAction(const std::string& l) : label(l)
{
   STANDARD_CONSTRUCTOR()
}

void ParatrooperAction::copyData(const ParatrooperAction& org, const bool)
{
   BaseClass::copyData(org);
   label = org.label;
}

//------------------------------------------------------------------------------
// execute() -- comanda o proprio Paratrooper e escreve as DUAS chamadas
// obrigatorias do xboard. Ver o cabecalho para o porque das duas rodarem
// mesmo se o dynamic_cast para xnative::Paratrooper falhar.
//------------------------------------------------------------------------------
bool ParatrooperAction::execute(base::Component* const actor)
{
   const auto player = dynamic_cast<models::Player*>(actor);
   if (player == nullptr) return false;

   domain::Stage stage{};
   if (domain::stageFromLabel(label.c_str(), stage)) {
      const auto trooper = dynamic_cast<Paratrooper*>(actor);
      if (trooper != nullptr) trooper->setJumpStage(stage);
   }

   xboard::setBehaviorLabel(player->getID(), label);
   xboard::bumpDecisionCount(player->getID());
   return true;
}

} // namespace xparatrooper
} // namespace models
} // namespace mixr
