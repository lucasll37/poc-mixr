#include "bt/bt_factory.hpp"

#include "bt/nodes/ContactDetectedCondition.hpp"
#include "bt/nodes/EvadeAction.hpp"
#include "bt/nodes/FuelLowCondition.hpp"
#include "bt/nodes/PatrolAction.hpp"
#include "bt/nodes/ReturnToBaseAction.hpp"

namespace bt_nodes {

namespace {

// Um builder por tipo de no: e o ponto de extensao que o BehaviorTree.CPP
// oferece na v3 para construtores com argumentos extras
// (factory.registerBuilder<T>(ID, builder) -- ver NodeContext.hpp).
template <typename NodeType>
void registerWithContext(BT::BehaviorTreeFactory& factory, const std::string& id,
                         const NodeContext& context)
{
   BT::NodeBuilder builder{
      [context](const std::string& name, const BT::NodeConfiguration& config) {
         return std::make_unique<NodeType>(name, config, context);
      }};
   factory.registerBuilder<NodeType>(id, builder);
}

}

void registerNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context)
{
   registerWithContext<FuelLowCondition>(factory, "FuelLow", context);
   registerWithContext<ReturnToBaseAction>(factory, "ReturnToBase", context);
   registerWithContext<ContactDetectedCondition>(factory, "ContactDetected", context);
   registerWithContext<EvadeAction>(factory, "Evade", context);
   registerWithContext<PatrolAction>(factory, "Patrol", context);
}

} // namespace bt_nodes
