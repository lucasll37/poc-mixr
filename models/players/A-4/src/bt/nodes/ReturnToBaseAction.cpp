#include "bt/nodes/ReturnToBaseAction.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

ReturnToBaseAction::ReturnToBaseAction(const std::string& name, const BT::NodeConfiguration& config,
                                       const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus ReturnToBaseAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   auto& plan = context_.behavior->rtbPlan();

   const domain::FlightCommand cmd{plan.command(snap.northM, snap.eastM, snap.headingDeg)};
   const bool home{plan.arrived(snap.northM, snap.eastM)};

   context_.behavior->decision().take(cmd, home ? "HOME" : "RTB");
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
