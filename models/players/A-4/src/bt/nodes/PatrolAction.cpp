#include "bt/nodes/PatrolAction.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

PatrolAction::PatrolAction(const std::string& name, const BT::NodeConfiguration& config,
                           const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus PatrolAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   auto& plan = context_.behavior->patrolPlan();
   plan.advance(context_.behavior->getFrameDt());

   // Mesma rede de seguranca de ReturnToBaseAction/SupportAlertAction (ver
   // bt/DecisionContext.hpp) -- patrolAltitude ja e' calibrado a mao contra
   // o pico do proprio circuito (normalmente um no-op), mas nada validava
   // isso em runtime; um erro de configuracao futuro (ou reuso deste
   // modelo sobre outro tile SRTM) nao seria pego por nada em producao.
   domain::FlightCommand cmd{plan.command()};
   cmd.altitudeM = context_.behavior->clampAltitudeToTerrain(cmd.altitudeM);

   context_.behavior->decision().take(cmd, "PATROL");
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
