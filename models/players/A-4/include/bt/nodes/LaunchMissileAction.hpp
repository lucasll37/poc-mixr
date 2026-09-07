#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Marca o pedido de lancamento na decisao (decision.launchRequested). Quem
// aciona o StoresMgr de verdade e xnative::FlightAction::execute() -- este
// no nao toca objeto MIXR nenhum, mesmo padrao de ReportAndEvadeAction para
// broadcastAlert.
class LaunchMissileAction final : public BT::SyncActionNode
{
public:
   LaunchMissileAction(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
