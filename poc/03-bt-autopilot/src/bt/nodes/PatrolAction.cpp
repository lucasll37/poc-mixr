#include "bt/nodes/PatrolAction.hpp"

#include "domain/Mission.hpp"

#include "mixr/models/dynamics/DynamicsModel.hpp"
#include "mixr/base/units/Distances.hpp"

#include <iostream>

namespace bt_nodes {

PatrolAction::PatrolAction(const std::string& name, const BT::NodeConfiguration& config)
   : BT::SyncActionNode(name, config)
{
   mission_ = config.blackboard->get<domain::Mission*>("mission");
   dynamicsModel_ = config.blackboard->get<mixr::models::DynamicsModel*>("dynamicsModel");
}

BT::NodeStatus PatrolAction::tick()
{
   if (mission_->maybeAdvanceWaypoint()) {
      std::cout << "  [Patrol] avancando para waypoint " << mission_->currentWaypointIndex() << std::endl;
   }

   const auto& wp = mission_->currentWaypoint();
   dynamicsModel_->setCommandedHeadingD(wp.headingDeg);
   dynamicsModel_->setCommandedAltitude(wp.altitudeFt * mixr::base::distance::FT2M);
   dynamicsModel_->setCommandedVelocityKts(wp.speedKts);

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
