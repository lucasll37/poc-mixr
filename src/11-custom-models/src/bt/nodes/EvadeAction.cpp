#include "bt/nodes/EvadeAction.hpp"

#include "bt/nodes/apply_command.hpp"

#include "domain/ThreatPolicy.hpp"

#include "xdrone/BtPilot.hpp"
#include "xdrone/Drone.hpp"
#include "xdrone/ProximitySensor.hpp"

namespace bt_nodes {

EvadeAction::EvadeAction(const std::string& name, const BT::NodeConfiguration& config,
                         const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus EvadeAction::tick()
{
   if (!context_.complete()) return BT::NodeStatus::FAILURE;

   const mixr::xdrone::ProximitySensor::Contact contact{context_.sensor->getContact()};
   if (!contact.valid) return BT::NodeStatus::FAILURE;

   context_.pilot->setState("EVADE");

   domain::ThreatContact threat;
   threat.rangeM = contact.rangeM;
   threat.relBearingDeg = contact.relBearingDeg;
   threat.deltaAltM = contact.deltaAltM;

   const domain::FlightCommand cmd{context_.pilot->threatPolicy().evade(
      threat, context_.drone->getHeadingD(), context_.drone->getAltitudeM())};

   applyCommand(context_.dynamics, cmd);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
