#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Volta para a base quando o combustivel (real, do JSBSim) cai abaixo da reserva.
class ReturnToBaseAction final : public BT::SyncActionNode
{
public:
   ReturnToBaseAction(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
