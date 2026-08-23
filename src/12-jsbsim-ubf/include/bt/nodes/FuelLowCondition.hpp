#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace bt_nodes {

// SUCCESS quando o combustivel (lido do JSBSim, nao de um modelo nosso)
// esta abaixo da reserva do comportamento mais a margem do XML.
//
// PORT 'margin': convencao do BehaviorTree.CPP para parametrizar um no pelo
// XML (providedPorts + getInput). A reserva e propriedade da AERONAVE
// (slot EDL do BtBehavior); a margem e propriedade da ARVORE.
class FuelLowCondition final : public BT::ConditionNode
{
public:
   FuelLowCondition(const std::string& name, const BT::NodeConfiguration& config,
                    const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
