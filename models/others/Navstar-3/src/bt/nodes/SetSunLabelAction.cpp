#include "bt/nodes/SetSunLabelAction.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace bt {

SetSunLabelAction::SetSunLabelAction(const std::string& name,
                                     const BT::NodeConfiguration& config,
                                     const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::PortsList SetSunLabelAction::providedPorts()
{
   return { BT::InputPort<std::string>("label", "SUNLIT",
                                       "rotulo que vai para o xboard e para o dump") };
}

BT::NodeStatus SetSunLabelAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   std::string label{"SUNLIT"};
   const BT::Optional<std::string> input{getInput<std::string>("label")};
   if (input) label = input.value();

   context_.behavior->decision().take(label);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
