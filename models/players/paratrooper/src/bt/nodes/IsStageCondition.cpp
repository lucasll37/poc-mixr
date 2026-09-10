#include "bt/nodes/IsStageCondition.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {
namespace bt {

IsStageCondition::IsStageCondition(const std::string& name,
                                   const BT::NodeConfiguration& config,
                                   const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::PortsList IsStageCondition::providedPorts()
{
   return { BT::InputPort<std::string>("stage", "LANDED",
                                       "estagio a comparar: FREEFALL, CANOPY ou LANDED") };
}

BT::NodeStatus IsStageCondition::tick()
{
   // Um no NUNCA pode presumir que o contexto existe: 'dump-tree-model'
   // (tools/) monta a factory com um NodeContext vazio so para ler o
   // manifesto -- e um teste pode fazer o mesmo. FAILURE e a degradacao
   // certa; derrubar o processo, nao.
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const BT::Optional<std::string> input{getInput<std::string>("stage")};
   if (!input) return BT::NodeStatus::FAILURE;

   domain::Stage target{};
   if (!domain::stageFromLabel(input.value().c_str(), target)) return BT::NodeStatus::FAILURE;

   return (context_.behavior->stage() == target) ? BT::NodeStatus::SUCCESS
                                                  : BT::NodeStatus::FAILURE;
}

} // namespace bt
} // namespace xparatrooper
} // namespace models
} // namespace mixr
