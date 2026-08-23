#include "bt/nodes/RechargeAction.hpp"

#include "bt/bt_factory.hpp"
#include "domain/Sentinel.hpp"

#include <iostream>

namespace bt {

RechargeAction::RechargeAction(const std::string& name, const BT::NodeConfiguration& config)
   : BT::SyncActionNode(name, config)
{
   config.blackboard->get<domain::Sentinel*>(kSentinelKey, sentinel_);
}

BT::NodeStatus RechargeAction::tick()
{
   if (sentinel_ == nullptr) return BT::NodeStatus::FAILURE;

   sentinel_->recharge();
   std::cout << "  [Recharge] recarregando...  battery=" << sentinel_->battery() << std::endl;
   return BT::NodeStatus::SUCCESS;
}

}
