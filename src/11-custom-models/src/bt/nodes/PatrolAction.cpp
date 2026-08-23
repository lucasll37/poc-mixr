#include "bt/nodes/PatrolAction.hpp"

#include "bt/nodes/apply_command.hpp"

#include "xdrone/BtPilot.hpp"

namespace bt_nodes {

PatrolAction::PatrolAction(const std::string& name, const BT::NodeConfiguration& config,
                           const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus PatrolAction::tick()
{
   const auto pilot = context_.pilot;
   if (pilot == nullptr || context_.dynamics == nullptr) return BT::NodeStatus::FAILURE;

   pilot->setState("PATROL");

   // O relogio da perna so corre quando a patrulha esta de fato ativa --
   // se o drone estiver em RTB ou evadindo, este no nem e ticado, entao a
   // patrulha e retomada de onde parou.
   auto& plan = pilot->patrolPlan();
   plan.advance(pilot->getFrameDt());

   applyCommand(context_.dynamics, plan.command());
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
