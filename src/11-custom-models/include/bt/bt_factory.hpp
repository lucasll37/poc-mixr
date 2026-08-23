#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

namespace bt_nodes {

// Registra os nos proprios desta PoC na factory do BehaviorTree.CPP --
// exatamente o mesmo papel que mixr::xdrone::factory() cumpre para os
// objetos MIXR, so que do lado da arvore de comportamento.
//
// Cada BtPilot tem a SUA BehaviorTreeFactory (uma por player) e passa o
// seu proprio NodeContext, entao os nos ja nascem ligados aos objetos
// daquela aeronave -- sem lookup em blackboard e sem estado global.
void registerNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context);

} // namespace bt_nodes
