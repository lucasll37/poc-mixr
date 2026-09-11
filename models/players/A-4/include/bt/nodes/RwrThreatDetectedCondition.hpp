#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace bt_nodes {

// SUCCESS enquanto a manobra evasiva disparada por RWR esta' valendo
// (domain::ThreatPolicy::engaged() da SEGUNDA instancia, rwrThreatPolicy()
// -- independente da threatPolicy() de contato de radar proprio, que
// continua reagindo so' a aeronaves via TWS). So' fica 'engaged()' depois
// que domain::EvasionReactionPlan liberou a manobra (ver
// ubf::BtBehavior::feedRwrEvasion()) -- ou seja, o atraso estocastico de
// reacao ja' aconteceu ANTES desta condicao poder suceder.
class RwrThreatDetectedCondition final : public BT::ConditionNode
{
public:
   RwrThreatDetectedCondition(const std::string& name, const BT::NodeConfiguration& config,
                              const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
