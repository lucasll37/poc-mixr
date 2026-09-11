#include "bt/nodes/FireMissileAction.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xaaa {
namespace bt {

FireMissileAction::FireMissileAction(const std::string& name,
                                     const BT::NodeConfiguration& config,
                                     const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus FireMissileAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   context_.behavior->decision().fire(context_.behavior->targetName());
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt
} // namespace xaaa
} // namespace models
} // namespace mixr
