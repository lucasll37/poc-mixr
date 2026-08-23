#include "bt/nodes/ReturnToBaseAction.hpp"

#include "bt/nodes/apply_command.hpp"

#include "xdrone/BtPilot.hpp"
#include "xdrone/Drone.hpp"
#include "xdrone/FuelSystem.hpp"

namespace bt_nodes {

ReturnToBaseAction::ReturnToBaseAction(const std::string& name, const BT::NodeConfiguration& config,
                                       const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus ReturnToBaseAction::tick()
{
   if (!context_.complete()) return BT::NodeStatus::FAILURE;

   // Posicao NED (metros) a partir do ponto de referencia do cenario --
   // mesma referencia que o RtbPlan usa para a base (0,0).
   const mixr::base::Vec3d pos{context_.drone->getPosition()};
   const double n{pos[mixr::models::Player::INORTH]};
   const double e{pos[mixr::models::Player::IEAST]};

   auto& plan = context_.pilot->rtbPlan();

   if (plan.arrived(n, e)) {
      context_.fuel->setRefueling(true);
      context_.pilot->setState("REFUEL");
   } else {
      context_.pilot->setState("RTB");
   }

   if (context_.fuel->isFull()) context_.fuel->setRefueling(false);

   applyCommand(context_.dynamics, plan.command(n, e, context_.drone->getHeadingD()));
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
