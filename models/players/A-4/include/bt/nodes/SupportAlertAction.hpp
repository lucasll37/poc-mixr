#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Voa para o ponto do alerta recebido -- a reacao ao evento de outro player.
class SupportAlertAction final : public BT::SyncActionNode
{
public:
   SupportAlertAction(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
