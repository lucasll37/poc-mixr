#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Voa de volta para a base (origem da area de jogo) e, ao chegar, comanda
// o reabastecimento no FuelSystem ate encher.
class ReturnToBaseAction final : public BT::SyncActionNode
{
public:
   ReturnToBaseAction(const std::string& name, const BT::NodeConfiguration& config,
                      const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
