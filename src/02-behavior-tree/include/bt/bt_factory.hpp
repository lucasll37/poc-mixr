#pragma once

#include "behaviortree_cpp_v3/bt_factory.h"

namespace domain { class Sentinel; }

namespace bt {

// Chave do blackboard pela qual os nos recebem o Sentinel injetado.
inline constexpr const char* kSentinelKey{"sentinel"};

// Registra os nos desta poc na factory do BehaviorTree.CPP.
void registerNodes(BT::BehaviorTreeFactory& factory);

// Cria o blackboard raiz com o Sentinel ja injetado.
BT::Blackboard::Ptr makeBlackboard(domain::Sentinel* sentinel);

}
