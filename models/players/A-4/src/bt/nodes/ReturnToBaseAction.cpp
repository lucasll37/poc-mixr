#include "bt/nodes/ReturnToBaseAction.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

ReturnToBaseAction::ReturnToBaseAction(const std::string& name, const BT::NodeConfiguration& config,
                                       const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus ReturnToBaseAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   auto& plan = context_.behavior->rtbPlan();

   domain::FlightCommand cmd{plan.command(snap.northM, snap.eastM, snap.headingDeg)};
   const bool home{plan.arrived(snap.northM, snap.eastM)};

   // ACHADO POR AUDITORIA (nao redescobrir, ver bt/DecisionContext.hpp):
   // RtbPlan e' geometria pura, sem nocao de terreno -- rtbAltitude e' um
   // valor FIXO do EDL, calibrado contra o pico do PROPRIO circuito de cada
   // falcon, nao contra o caminho de volta ate a base (que pode cruzar
   // relevo bem diferente). Sem este clamp, nada impedia a aeronave de
   // voar reto para dentro de uma montanha no meio do trajeto de RTB.
   cmd.altitudeM = context_.behavior->clampAltitudeToTerrain(cmd.altitudeM);

   context_.behavior->decision().take(cmd, home ? "HOME" : "RTB");
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
