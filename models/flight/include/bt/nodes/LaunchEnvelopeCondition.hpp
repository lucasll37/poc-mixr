#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace bt_nodes {

// SUCCESS quando ha arma disponivel (WorldView::weaponReady) E o contato esta
// dentro do envelope de lancamento (domain::LaunchPolicy).
class LaunchEnvelopeCondition final : public BT::ConditionNode
{
public:
   LaunchEnvelopeCondition(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
