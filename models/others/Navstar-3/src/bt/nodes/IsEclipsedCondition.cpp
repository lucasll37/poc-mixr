#include "bt/nodes/IsEclipsedCondition.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace bt {

IsEclipsedCondition::IsEclipsedCondition(const std::string& name,
                                         const BT::NodeConfiguration& config,
                                         const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::PortsList IsEclipsedCondition::providedPorts()
{
   return {};
}

BT::NodeStatus IsEclipsedCondition::tick()
{
   // Um no NUNCA pode presumir que o contexto existe: 'dump-tree-model'
   // (tools/) monta a factory com um NodeContext vazio so para ler o
   // manifesto -- e um teste pode fazer o mesmo. FAILURE e a degradacao
   // certa; derrubar o processo, nao.
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const bool eclipsed{context_.behavior->sunState() == domain::SunState::ECLIPSE};
   return eclipsed ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
