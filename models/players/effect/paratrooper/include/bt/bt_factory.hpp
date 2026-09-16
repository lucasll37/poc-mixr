#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

namespace mixr {
namespace models {
namespace xparatrooper {
namespace bt {

// Registra os nos DESTE modelo na factory do BehaviorTree.CPP. Cada
// ExampleBehavior (isto e, cada entidade do cenario) tem a SUA factory e
// passa o seu NodeContext.
//
// E a UNICA lista de nos do modelo: 'tools/dump_tree_model' chama esta MESMA
// funcao para gerar o <TreeNodesModel> que o Groot le (ver 'make update-bt'),
// entao um no registrado aqui aparece na paleta do editor sem nenhum passo a
// mais.
void registerNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context);

} // namespace bt
} // namespace xparatrooper
} // namespace models
} // namespace mixr
