#pragma once

#include "behaviortree_cpp_v3/bt_factory.h"

namespace bt_nodes {

// Registra os TreeNodes proprios desta PoC (usados pelos 4 wingmen) na
// factory do BehaviorTree.CPP.
void registerNodes(BT::BehaviorTreeFactory& factory);

} // namespace bt_nodes
