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
// Duas perguntas independentes, nesta ordem: temos arma (mais barato -- so um
// bool), e so entao o alvo esta no envelope (ver domain/LaunchPolicy.hpp).
//------------------------------------------------------------------------------
BT::NodeStatus LaunchEnvelopeCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   if (!snap.weaponReady || !snap.hasContact) return BT::NodeStatus::FAILURE;

   const bool inEnvelope{domain::inLaunchEnvelope(context_.behavior->launchEnvelope(),
                                                  snap.contactRangeM, snap.contactRelBearingDeg)};
   return inEnvelope ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
