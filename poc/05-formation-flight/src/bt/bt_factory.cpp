#include "bt/bt_factory.hpp"

#include "bt/nodes/FollowFormationAction.hpp"
#include "bt/nodes/RtbEngagedCondition.hpp"

namespace bt_nodes {

void registerNodes(BT::BehaviorTreeFactory& factory)
{
   factory.registerNodeType<RtbEngagedCondition>("RtbEngaged");
   factory.registerNodeType<FollowFormationAction>("FollowFormation");
}

} // namespace bt_nodes
