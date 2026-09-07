#include "bt/nodes/LaunchMissileAction.hpp"

#include "bt/DecisionContext.hpp"
#include "domain/ThreatPolicy.hpp"

namespace bt_nodes {

LaunchMissileAction::LaunchMissileAction(const std::string& name,
                                         const BT::NodeConfiguration& config,
                                         const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

//------------------------------------------------------------------------------
// O comando de voo desta acao e o MESMO que ReportAndEvade usaria (o alvo ja
// fixado por domain::ThreatPolicy na entrada da manobra -- LaunchEnvelope so
// chega a SUCCESS com ContactDetected/threatPolicy().engaged() ja true, ver
// bt/nodes/LaunchEnvelopeCondition.cpp). Nao ha comando "so lancar": no tick
// do lancamento a aeronave ja esta quebrando, so o ROTULO muda para "LAUNCH"
// -- e o que da visibilidade ao evento na linha de status sem inventar um
// terceiro perfil de voo.
//------------------------------------------------------------------------------
BT::NodeStatus LaunchMissileAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   const domain::ThreatPolicy& policy{context_.behavior->threatPolicy()};
   FlightDecision& decision{context_.behavior->decision()};

   decision.take(policy.command(), "LAUNCH");
   decision.launchRequested = true;
   decision.launchTargetName = snap.contactName;

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
