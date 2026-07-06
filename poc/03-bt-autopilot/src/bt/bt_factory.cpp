#include "bt/bt_factory.hpp"

#include "bt/nodes/FuelLowCondition.hpp"
#include "bt/nodes/PatrolAction.hpp"
#include "bt/nodes/ReturnToBaseAction.hpp"

namespace bt_nodes {

void registerNodes(BT::BehaviorTreeFactory& factory)
{
   factory.registerNodeType<FuelLowCondition>("FuelLow");
   factory.registerNodeType<ReturnToBaseAction>("ReturnToBase");
   factory.registerNodeType<PatrolAction>("Patrol");
}

} // namespace bt_nodes
