#include "bt/nodes/PatrolAction.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

PatrolAction::PatrolAction(const std::string& name, const BT::NodeConfiguration& config,
                           const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus PatrolAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   auto& plan = context_.behavior->patrolPlan();
   plan.advance(context_.behavior->getFrameDt());

   context_.behavior->decision().take(plan.command(), "PATROL");
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
