#include "bt/nodes/TargetInDomeCondition.hpp"

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xaaa {
namespace bt {

TargetInDomeCondition::TargetInDomeCondition(const std::string& name,
                                             const BT::NodeConfiguration& config,
                                             const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::NodeStatus TargetInDomeCondition::tick()
{
   // Um no NUNCA pode presumir que o contexto existe: 'dump-tree-model'
   // monta a factory com um NodeContext vazio so' para ler o manifesto -- e
   // um teste pode fazer o mesmo. FAILURE e' a degradacao certa; derrubar o
   // processo, nao.
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   if (!context_.behavior->hasTarget()) return BT::NodeStatus::FAILURE;
   if (!context_.behavior->weaponReady()) return BT::NodeStatus::FAILURE;

   const bool inDome{domain::inDome(context_.behavior->dome(), context_.behavior->targetRangeM())};
   return inDome ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt
} // namespace xaaa
} // namespace models
} // namespace mixr
