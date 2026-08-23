#pragma once

#include "behaviortree_cpp_v3/bt_factory.h"

namespace bt_nodes {

// Registra todos os TreeNodes proprios desta PoC na factory do
// BehaviorTree.CPP (equivalente ao factory() encadeado do MIXR, so que
// para os nos da arvore de comportamento).
void registerNodes(BT::BehaviorTreeFactory& factory);

} // namespace bt_nodes
