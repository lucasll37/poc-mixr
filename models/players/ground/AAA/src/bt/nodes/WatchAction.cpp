#include "bt/nodes/WatchAction.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xAAA {
namespace bt {

WatchAction::WatchAction(const std::string& name, const BT::NodeConfiguration& config,
                         const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus WatchAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   context_.behavior->decision().watch();
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt
} // namespace xAAA
} // namespace models
} // namespace mixr
