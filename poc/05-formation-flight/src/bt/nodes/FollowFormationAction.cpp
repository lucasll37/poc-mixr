#include "bt/nodes/FollowFormationAction.hpp"

#include "mixr/models/system/Autopilot.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/terrain/Terrain.hpp"

namespace bt_nodes {

FollowFormationAction::FollowFormationAction(const std::string& name, const BT::NodeConfiguration& config)
   : BT::SyncActionNode(name, config)
{
   formationState = config.blackboard->get<formations::FormationState*>("formationState");
   slotIndex = config.blackboard->get<int>("slotIndex");
   autopilot = config.blackboard->get<mixr::models::Autopilot*>("autopilot");
   wingman = config.blackboard->get<mixr::models::Player*>("wingman");
   terrain = config.blackboard->get<const mixr::terrain::Terrain*>("terrain");
}

BT::NodeStatus FollowFormationAction::tick()
{
   bool forceTrail{};
   getInput("forceTrail", forceTrail);

   const formations::Formation formation{
      forceTrail ? formations::Formation::TRAIL : formationState->current};
   formations::Offset offset{formations::formationOffset(formation, slotIndex)};

   // Rede de seguranca simples: se ja estamos abaixo do terreno + margem,
   // empurra o delta de altitude pedido pra cima na mesma medida (nao e
   // um seguidor de terreno rigoroso, so evita a colisao mais obvia).
   if (terrain != nullptr && wingman != nullptr) {
      double elevM{};
      const bool valid{terrain->getElevation(&elevM, wingman->getLatitude(), wingman->getLongitude(), true)};
      const double marginM{100.0};
      if (valid) {
         const double curAltM{wingman->getAltitudeM()};
         const double deficitM{(elevM + marginM) - curAltM};
         if (deficitM > 0.0) offset.altOffsetM += deficitM;
      }
   }

   const bool changed{!hasAppliedOffset
      || offset.trailM != lastOffset.trailM
      || offset.rightM != lastOffset.rightM
      || offset.altOffsetM != lastOffset.altOffsetM};

   if (changed) {
      autopilot->setLeadFollowingDistanceTrail(offset.trailM);
      autopilot->setLeadFollowingDistanceRight(offset.rightM);
      autopilot->setLeadFollowingDeltaAltitude(offset.altOffsetM);
      lastOffset = offset;
      hasAppliedOffset = true;
   }

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
