#include "bt/nodes/RwrThreatDetectedCondition.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

RwrThreatDetectedCondition::RwrThreatDetectedCondition(const std::string& name,
                                                        const BT::NodeConfiguration& config,
                                                        const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::NodeStatus RwrThreatDetectedCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   return context_.behavior->rwrThreatPolicy().engaged() ? BT::NodeStatus::SUCCESS
                                                          : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
