#pragma once

#include "behaviortree_cpp_v3/action_node.h"

namespace domain { class Mission; }
namespace mixr { namespace models { class DynamicsModel; } }

namespace bt_nodes {

// Comanda o dynamics model de volta para o waypoint-base enquanto o
// combustivel estiver baixo; ao completar o tempo de RTB, reabastece a
// missao (via Mission::maybeCompleteRtb()).
class ReturnToBaseAction final : public BT::SyncActionNode
{
public:
   ReturnToBaseAction(const std::string& name, const BT::NodeConfiguration& config);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   domain::Mission* mission_{};
   mixr::models::DynamicsModel* dynamicsModel_{};
};

} // namespace bt_nodes
