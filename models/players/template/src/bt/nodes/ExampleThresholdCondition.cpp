#include "bt/nodes/ExampleThresholdCondition.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xtemplate {
namespace bt {

ExampleThresholdCondition::ExampleThresholdCondition(const std::string& name,
                                                     const BT::NodeConfiguration& config,
                                                     const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::PortsList ExampleThresholdCondition::providedPorts()
{
   return { BT::InputPort<double>("margin", 0.0,
                                  "margem somada ao limiar de engajamento (onValue)") };
}

BT::NodeStatus ExampleThresholdCondition::tick()
{
   // Um no NUNCA pode presumir que o contexto existe: 'dump-tree-model'
   // (tools/) monta a factory com um NodeContext vazio so para ler o
   // manifesto -- e um teste pode fazer o mesmo. FAILURE e a degradacao
   // certa; derrubar o processo, nao.
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   double margin{};
   const BT::Optional<double> input{getInput<double>("margin")};
   if (input) margin = input.value();

   // A regra e PURA (domain/), entao aplicar a margem e copiar e ajustar a
   // copia -- nunca mexer na do comportamento, que veio do .edl e vale para
   // todos os ticks. Dois nos com margens diferentes na MESMA arvore
   // continuam corretos por construcao.
   domain::ExampleThreshold rule{context_.behavior->rule()};
   rule.onValue += margin;

   const bool next{rule.next(context_.behavior->value(), context_.behavior->engaged())};
   context_.behavior->setEngaged(next);

   return next ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt
} // namespace xtemplate
} // namespace models
} // namespace mixr
