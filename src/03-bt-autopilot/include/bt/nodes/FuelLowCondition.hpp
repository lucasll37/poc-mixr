#pragma once

#include "behaviortree_cpp_v3/condition_node.h"

namespace domain { class Mission; }

namespace bt_nodes {

// SUCCESS quando o combustivel da missao esta abaixo do limiar baixo.
class FuelLowCondition final : public BT::ConditionNode
{
public:
   FuelLowCondition(const std::string& name, const BT::NodeConfiguration& config);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   domain::Mission* mission_{};
};

} // namespace bt_nodes
