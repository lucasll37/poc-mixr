#include "bt/nodes/BatteryLowCondition.hpp"

#include "bt/bt_factory.hpp"
#include "domain/Sentinel.hpp"

namespace bt {

BatteryLowCondition::BatteryLowCondition(const std::string& name, const BT::NodeConfiguration& config)
   : BT::ConditionNode(name, config)
{
   config.blackboard->get<domain::Sentinel*>(kSentinelKey, sentinel_);
}

BT::NodeStatus BatteryLowCondition::tick()
{
   if (sentinel_ == nullptr) return BT::NodeStatus::FAILURE;
   return sentinel_->batteryLow() ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

}
