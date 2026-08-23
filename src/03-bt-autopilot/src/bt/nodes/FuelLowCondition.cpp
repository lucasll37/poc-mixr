#include "bt/nodes/FuelLowCondition.hpp"

#include "domain/Mission.hpp"

namespace bt_nodes {

FuelLowCondition::FuelLowCondition(const std::string& name, const BT::NodeConfiguration& config)
   : BT::ConditionNode(name, config)
{
   mission_ = config.blackboard->get<domain::Mission*>("mission");
}

BT::NodeStatus FuelLowCondition::tick()
{
   return mission_->isFuelLow() ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
