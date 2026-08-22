#pragma once

#include "behaviortree_cpp_v3/action_node.h"

namespace domain { class Sentinel; }

namespace bt {

// Acao: uma volta de patrulha.
class PatrolAction final : public BT::SyncActionNode
{
public:
   PatrolAction(const std::string& name, const BT::NodeConfiguration& config);
   static BT::PortsList providedPorts() { return {}; }

   BT::NodeStatus tick() override;

private:
   domain::Sentinel* sentinel_{};
};

}
