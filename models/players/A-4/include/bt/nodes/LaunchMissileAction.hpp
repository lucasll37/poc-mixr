#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Pede o lancamento de um missil contra o contato atual. Nao toca em objeto
// MIXR nenhum -- ver o comentario grande em ubf/FlightAction.cpp:
// StoresMgr::releaseOneMissile() + setTargetPlayer() sao o UNICO ponto
// deste modelo que faz isso, e so' rodam na atuacao.
class LaunchMissileAction final : public BT::SyncActionNode
{
public:
   LaunchMissileAction(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
