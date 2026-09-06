#include "ubf/ExampleAction.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace models {
namespace xtemplate {

IMPLEMENT_SUBCLASS(ExampleAction, "ExampleAction")
EMPTY_SLOTTABLE(ExampleAction)
EMPTY_DELETEDATA(ExampleAction)

ExampleAction::ExampleAction()
{
   STANDARD_CONSTRUCTOR()
}

ExampleAction::ExampleAction(const std::string& l) : label(l)
{
   STANDARD_CONSTRUCTOR()
}

void ExampleAction::copyData(const ExampleAction& org, const bool)
{
   BaseClass::copyData(org);
   label = org.label;
}

//------------------------------------------------------------------------------
// execute() -- as DUAS chamadas obrigatorias estao aqui embaixo. Ver
// CONTRATO.md secao 3 e o cabecalho deste arquivo. 'xboard' e a UNICA coisa
// que o modelo e o host compartilham: o modelo escreve, o host (a tela de
// status, o dump '-deterministic') so le. Sem elas, o dump sai com
// 'bt=--'/'dec=0' -- sem erro nenhum, confirmado no proprio
// models/player/fixtures/stub deste repositorio.
//------------------------------------------------------------------------------
bool ExampleAction::execute(base::Component* const actor)
{
   const auto player = dynamic_cast<models::Player*>(actor);
   if (player == nullptr) return false;

   xboard::setBehaviorLabel(player->getID(), label);
   xboard::bumpDecisionCount(player->getID());
   return true;
}

} // namespace xtemplate
} // namespace models
} // namespace mixr
