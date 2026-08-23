#pragma once

#include "behaviortree_cpp_v3/action_node.h"

namespace domain { class Sentinel; }

namespace bt {

// Acao: recarga completa.
class RechargeAction final : public BT::SyncActionNode
{
public:
   RechargeAction(const std::string& name, const BT::NodeConfiguration& config);
   static BT::PortsList providedPorts() { return {}; }

   BT::NodeStatus tick() override;

private:
   domain::Sentinel* sentinel_{};
};

}
