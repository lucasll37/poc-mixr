#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

namespace bt_nodes {

// Registra os nos desta PoC na factory do BehaviorTree.CPP. Cada BtBehavior
// (isto e, cada aeronave) tem a SUA factory e passa o seu NodeContext.
void registerNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context);

} // namespace bt_nodes
