#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace mixr {
namespace models {
namespace xparatrooper {
namespace bt {

// SUCCESS se o estagio ATUAL (ja avancado por
// ubf::ParatrooperBtBehavior::genAction(), antes deste tick) bater com o
// port 'stage'. LEITURA PURA -- nunca muta o contexto (ver o comentario de
// bt/DecisionContext.hpp para o porque disto ser diferente do
// ExampleThresholdCondition do template).
//
// PORT 'stage': "FREEFALL"/"CANOPY"/"LANDED" -- os MESMOS rotulos de
// domain::labelOf()/stageFromLabel(), unica fonte de verdade.
class IsStageCondition final : public BT::ConditionNode
{
public:
   IsStageCondition(const std::string& name, const BT::NodeConfiguration& config,
                    const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt
} // namespace xparatrooper
} // namespace models
} // namespace mixr
