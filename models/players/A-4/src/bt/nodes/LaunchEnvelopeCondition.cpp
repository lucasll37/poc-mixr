#include "bt/nodes/LaunchEnvelopeCondition.hpp"

#include "bt/DecisionContext.hpp"
#include "domain/LaunchPolicy.hpp"

namespace bt_nodes {

LaunchEnvelopeCondition::LaunchEnvelopeCondition(const std::string& name,
                                                 const BT::NodeConfiguration& config,
                                                 const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

//------------------------------------------------------------------------------
// Deliberadamente CONTATO DE VERDADE (snapshot().hasContact), nao
// threatPolicy().engaged() -- ao contrario de ContactDetectedCondition, que
// consulta a histerese justamente para NAO oscilar entre evadir e apoiar.
// Disparar contra uma pista que ja sumiu (so' sobrevivendo no arrasto da
// histerese) mandaria o missil atras de uma posicao velha.
//------------------------------------------------------------------------------
BT::NodeStatus LaunchEnvelopeCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   if (!snap.weaponReady || !snap.hasContact) return BT::NodeStatus::FAILURE;

   return domain::inLaunchEnvelope(context_.behavior->launchEnvelope(),
                                   snap.contactRangeM, snap.contactRelBearingDeg)
      ? BT::NodeStatus::SUCCESS
      : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
