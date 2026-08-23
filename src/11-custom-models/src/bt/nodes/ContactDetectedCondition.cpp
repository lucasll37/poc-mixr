#include "bt/nodes/ContactDetectedCondition.hpp"

#include "xdrone/ProximitySensor.hpp"

namespace bt_nodes {

ContactDetectedCondition::ContactDetectedCondition(const std::string& name,
                                                   const BT::NodeConfiguration& config,
                                                   const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::NodeStatus ContactDetectedCondition::tick()
{
   const auto sensor = context_.sensor;
   if (sensor == nullptr) return BT::NodeStatus::FAILURE;
   return sensor->hasContact() ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
