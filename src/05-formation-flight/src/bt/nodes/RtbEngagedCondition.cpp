#include "bt/nodes/RtbEngagedCondition.hpp"

#include "formations.hpp"

namespace bt_nodes {

RtbEngagedCondition::RtbEngagedCondition(const std::string& name, const BT::NodeConfiguration& config)
   : BT::ConditionNode(name, config)
{
   formationState = config.blackboard->get<formations::FormationState*>("formationState");
}

BT::NodeStatus RtbEngagedCondition::tick()
{
   return formationState->rtbEngaged ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
