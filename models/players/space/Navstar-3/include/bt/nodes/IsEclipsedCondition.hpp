#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace bt {

// SUCCESS quando o satelite esta na sombra da Terra neste ciclo (o estado ja
// foi calculado por ubf::Navstar3BtBehavior::genAction(), via
// domain::sunState() -- este no so LE, nunca recalcula geometria nenhuma).
class IsEclipsedCondition final : public BT::ConditionNode
{
public:
   IsEclipsedCondition(const std::string& name, const BT::NodeConfiguration& config,
                       const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
