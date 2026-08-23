#include "bt/nodes/PatrolAction.hpp"

#include "bt/bt_factory.hpp"
#include "domain/Sentinel.hpp"

#include <iostream>

namespace bt {

PatrolAction::PatrolAction(const std::string& name, const BT::NodeConfiguration& config)
   : BT::SyncActionNode(name, config)
{
   config.blackboard->get<domain::Sentinel*>(kSentinelKey, sentinel_);
}

BT::NodeStatus PatrolAction::tick()
{
   if (sentinel_ == nullptr) return BT::NodeStatus::FAILURE;

   sentinel_->patrol();
   std::cout << "  [Patrol]   patrulhando...   battery=" << sentinel_->battery() << std::endl;
   return BT::NodeStatus::SUCCESS;
}

}
