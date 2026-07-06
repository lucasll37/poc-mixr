#pragma once

#include "behaviortree_cpp_v3/action_node.h"

namespace domain { class Mission; }
namespace mixr { namespace models { class DynamicsModel; } }

namespace bt_nodes {

// Aplica o waypoint atual da missao como comando no dynamics model e avanca
// para o proximo waypoint quando o tempo de permanencia expira.
class PatrolAction final : public BT::SyncActionNode
{
public:
   PatrolAction(const std::string& name, const BT::NodeConfiguration& config);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   domain::Mission* mission_{};
   mixr::models::DynamicsModel* dynamicsModel_{};
};

} // namespace bt_nodes
