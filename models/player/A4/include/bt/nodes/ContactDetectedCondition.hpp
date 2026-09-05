#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace bt_nodes {

// SUCCESS quando o sensor de proximidade tem contato valido.
class ContactDetectedCondition final : public BT::ConditionNode
{
public:
   ContactDetectedCondition(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
