#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace bt_nodes {

// SUCCESS quando ha arma disponivel, contato de verdade (nao so o arrasto
// da histerese de ThreatPolicy) e ele esta dentro do envelope de lancamento.
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
