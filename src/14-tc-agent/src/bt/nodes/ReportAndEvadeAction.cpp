#include "bt/nodes/ReportAndEvadeAction.hpp"

#include "domain/ThreatPolicy.hpp"
#include "ubf/BtBehavior.hpp"

namespace bt_nodes {

ReportAndEvadeAction::ReportAndEvadeAction(const std::string& name,
                                           const BT::NodeConfiguration& config,
                                           const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus ReportAndEvadeAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   if (!snap.hasContact) return BT::NodeStatus::FAILURE;

   domain::ThreatContact threat;
   threat.rangeM = snap.contactRangeM;
   threat.relBearingDeg = snap.contactRelBearingDeg;
   threat.deltaAltM = snap.contactDeltaAltM;

   const domain::FlightCommand cmd{
      context_.behavior->threatPolicy().evade(threat, snap.headingDeg, snap.altitudeM)};

   FlightDecision& decision{context_.behavior->decision()};
   decision.take(cmd, "EVADE");

   // O "influencia os demais": este no NAO alcanca outro player -- ele so
   // marca o pedido. Quem transmite e o AlertRadio, na fase 1 do frame
   // seguinte, com a mensagem chegando aos outros como evento nativo.
   decision.broadcastAlert = true;
   decision.alertContactName = snap.contactName;
   decision.alertNorthM = snap.contactNorthM;
   decision.alertEastM = snap.contactEastM;
   decision.alertAltitudeM = snap.contactAltitudeM;
   decision.alertRangeM = snap.contactRangeM;

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
