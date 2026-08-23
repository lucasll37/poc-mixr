#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Traduz o contato do ProximitySensor em ThreatContact (dominio puro),
// pede a manobra a ThreatPolicy e aplica no dynamics model.
class EvadeAction final : public BT::SyncActionNode
{
public:
   EvadeAction(const std::string& name, const BT::NodeConfiguration& config,
               const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
