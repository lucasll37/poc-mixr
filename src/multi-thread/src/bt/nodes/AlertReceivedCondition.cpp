#include "bt/nodes/AlertReceivedCondition.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

AlertReceivedCondition::AlertReceivedCondition(const std::string& name,
                                               const BT::NodeConfiguration& config,
                                               const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::NodeStatus AlertReceivedCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   // O alerta so aparece aqui um frame depois de transmitido (o AlertRadio
   // promove a entrada na fase 2) -- latencia fixa, igual para todos.
   return context_.behavior->snapshot().hasAlert ? BT::NodeStatus::SUCCESS
                                                 : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
