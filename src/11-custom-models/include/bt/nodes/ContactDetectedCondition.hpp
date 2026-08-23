#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace bt_nodes {

// SUCCESS quando o ProximitySensor tem um contato valido -- da varredura
// da fase 2 do MESMO frame deste tick, ou ainda dentro da memoria de
// pista (slot 'holdTime' do sensor).
class ContactDetectedCondition final : public BT::ConditionNode
{
public:
   ContactDetectedCondition(const std::string& name, const BT::NodeConfiguration& config,
                            const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
