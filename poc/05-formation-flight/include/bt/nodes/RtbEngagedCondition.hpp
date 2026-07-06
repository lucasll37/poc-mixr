#pragma once

#include "behaviortree_cpp_v3/condition_node.h"

namespace formations { struct FormationState; }

namespace bt_nodes {

// SUCCESS quando o RTB foi engajado (flag no FormationState compartilhado,
// setado pelo KeyboardIoHandler ao apertar a tecla de RTB).
class RtbEngagedCondition final : public BT::ConditionNode
{
public:
   RtbEngagedCondition(const std::string& name, const BT::NodeConfiguration& config);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   formations::FormationState* formationState{};
};

} // namespace bt_nodes
