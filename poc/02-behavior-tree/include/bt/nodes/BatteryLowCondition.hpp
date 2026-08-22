#pragma once

#include "behaviortree_cpp_v3/condition_node.h"

namespace domain { class Sentinel; }

namespace bt {

// Condicao: a bateria do sentinela esta baixa?
// O no nao decide o limiar -- so pergunta ao domain.
class BatteryLowCondition final : public BT::ConditionNode
{
public:
   BatteryLowCondition(const std::string& name, const BT::NodeConfiguration& config);
   static BT::PortsList providedPorts() { return {}; }

   BT::NodeStatus tick() override;

private:
   domain::Sentinel* sentinel_{};
};

}
