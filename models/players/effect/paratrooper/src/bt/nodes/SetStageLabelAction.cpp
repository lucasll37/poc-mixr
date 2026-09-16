#include "bt/nodes/SetStageLabelAction.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {
namespace bt {

SetStageLabelAction::SetStageLabelAction(const std::string& name,
                                         const BT::NodeConfiguration& config,
                                         const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::PortsList SetStageLabelAction::providedPorts()
{
   return { BT::InputPort<std::string>("label", "FREEFALL",
                                       "rotulo que vai para o xboard e para o dump") };
}

BT::NodeStatus SetStageLabelAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   std::string label{"FREEFALL"};
   const BT::Optional<std::string> input{getInput<std::string>("label")};
   if (input) label = input.value();

   context_.behavior->decision().take(label);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt
} // namespace xparatrooper
} // namespace models
} // namespace mixr
