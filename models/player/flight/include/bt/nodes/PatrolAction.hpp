#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Avanca o circuito de patrulha e comanda a perna atual.
class PatrolAction final : public BT::SyncActionNode
{
public:
   PatrolAction(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
