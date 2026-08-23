#include "bt/nodes/ReturnToBaseAction.hpp"

#include "domain/Mission.hpp"

#include "mixr/models/dynamics/DynamicsModel.hpp"
#include "mixr/base/units/Distances.hpp"

#include <iostream>

namespace bt_nodes {

ReturnToBaseAction::ReturnToBaseAction(const std::string& name, const BT::NodeConfiguration& config)
   : BT::SyncActionNode(name, config)
{
   mission_ = config.blackboard->get<domain::Mission*>("mission");
   dynamicsModel_ = config.blackboard->get<mixr::models::DynamicsModel*>("dynamicsModel");
}

BT::NodeStatus ReturnToBaseAction::tick()
{
   if (!mission_->isInRtb()) {
      mission_->enterRtb();
      std::cout << "  [ReturnToBase] combustivel baixo, retornando a base..." << std::endl;
   }

   const auto& wp = mission_->baseWaypoint();
   dynamicsModel_->setCommandedHeadingD(wp.headingDeg);
   dynamicsModel_->setCommandedAltitude(wp.altitudeFt * mixr::base::distance::FT2M);
   dynamicsModel_->setCommandedVelocityKts(wp.speedKts);

   if (mission_->maybeCompleteRtb()) {
      std::cout << "  [ReturnToBase] reabastecido, retomando patrulha" << std::endl;
   }

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
