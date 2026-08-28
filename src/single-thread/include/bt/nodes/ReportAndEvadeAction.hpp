#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Manobra de evasao E pede a transmissao do alerta aos demais avioes.
class ReportAndEvadeAction final : public BT::SyncActionNode
{
public:
   ReportAndEvadeAction(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
