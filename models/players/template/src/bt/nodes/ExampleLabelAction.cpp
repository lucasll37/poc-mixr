#include "bt/nodes/ExampleLabelAction.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xtemplate {
namespace bt {

ExampleLabelAction::ExampleLabelAction(const std::string& name,
                                       const BT::NodeConfiguration& config,
                                       const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::PortsList ExampleLabelAction::providedPorts()
{
   return { BT::InputPort<std::string>("label", "ENGAGED",
                                       "rotulo que vai para o xboard e para o dump") };
}

BT::NodeStatus ExampleLabelAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   std::string label{"ENGAGED"};
   const BT::Optional<std::string> input{getInput<std::string>("label")};
   if (input) label = input.value();

   context_.behavior->decision().take(label);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt
} // namespace xtemplate
} // namespace models
} // namespace mixr
