#include "bt/bt_factory_sdk.hpp"

#include "bt/nodes/OnnxScoreCondition.hpp"

#include <memory>
#include <string>

namespace bt_nodes {

namespace {

// Mesmo molde de bt/bt_factory.cpp -- registerBuilder<T>(ID, builder) e o
// ponto de extensao do BehaviorTree.CPP v3 para construtores com argumentos
// extras.
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

void registerSdkNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context)
{
   registerWithContext<OnnxScoreCondition>(factory, "OnnxScore", context);
}

} // namespace bt_nodes
