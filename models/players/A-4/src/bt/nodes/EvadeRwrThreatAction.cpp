#include "bt/nodes/EvadeRwrThreatAction.hpp"

#include "domain/ThreatPolicy.hpp"
#include "bt/DecisionContext.hpp"

namespace bt_nodes {

EvadeRwrThreatAction::EvadeRwrThreatAction(const std::string& name,
                                           const BT::NodeConfiguration& config,
                                           const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus EvadeRwrThreatAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const domain::ThreatPolicy& policy{context_.behavior->rwrThreatPolicy()};
   if (!policy.engaged()) return BT::NodeStatus::FAILURE;

   FlightDecision& decision{context_.behavior->decision()};
   decision.take(policy.command(), policy.contactLive() ? "RWR_EVADE" : "RWR_BREAK");

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
