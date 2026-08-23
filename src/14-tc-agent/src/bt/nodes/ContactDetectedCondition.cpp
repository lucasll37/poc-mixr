#include "bt/nodes/ContactDetectedCondition.hpp"

#include "ubf/BtBehavior.hpp"

namespace bt_nodes {

ContactDetectedCondition::ContactDetectedCondition(const std::string& name,
                                                   const BT::NodeConfiguration& config,
                                                   const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::NodeStatus ContactDetectedCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;
   return context_.behavior->snapshot().hasContact ? BT::NodeStatus::SUCCESS
                                                   : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
