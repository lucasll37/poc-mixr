#include "bt/bt_factory.hpp"

#include "bt/nodes/BatteryLowCondition.hpp"
#include "bt/nodes/PatrolAction.hpp"
#include "bt/nodes/RechargeAction.hpp"

namespace bt {

void registerNodes(BT::BehaviorTreeFactory& factory)
{
   // Nos como CLASSES registradas na factory (nao registerSimpleAction com
   // lambdas): e o padrao do BehaviorTree.CPP para nos com estado/injecao,
   // o mesmo usado em poc/03 e poc/05.
   factory.registerNodeType<BatteryLowCondition>("BatteryLow");
   factory.registerNodeType<PatrolAction>("Patrol");
   factory.registerNodeType<RechargeAction>("Recharge");
}

BT::Blackboard::Ptr makeBlackboard(domain::Sentinel* sentinel)
{
   // Injecao de dependencia pelo blackboard, antes de createTreeFromFile:
   // e assim que os nos recebem o objeto de dominio sem conhece-lo de
   // forma global.
   auto blackboard = BT::Blackboard::create();
   blackboard->set<domain::Sentinel*>(kSentinelKey, sentinel);
   return blackboard;
}

}
