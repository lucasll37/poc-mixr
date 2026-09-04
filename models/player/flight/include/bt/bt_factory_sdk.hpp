#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

namespace bt_nodes {

// Registra os nos que dependem do SDK do poc-mixr (shared/xinfer,
// shared/xpyembed) -- separados de bt/bt_factory.hpp DE PROPOSITO.
//
// O PORQUE: bt/bt_factory.cpp e compilado tanto pelo plugin quanto por
// models/flight/tests (alvo 'test-tree'), e o test-tree tem uma propriedade
// que vale a pena preservar -- ele NAO linka o MIXR, o que permite exercitar a
// arvore de PRODUCAO em ~10 ms sem levantar Station nenhuma (ver o cabecalho
// de bt/DecisionContext.hpp). As libs do SDK chegam por poc-mixr-sdk.pc, que
// declara 'Requires: mixr'; bastaria UM registro aqui para o test-tree passar
// a arrastar o framework inteiro.
//
// Quem chama e ubf/BtBehavior, logo depois de registerNodes(). Uma arvore que
// use estes nos so carrega, portanto, dentro do modelo de verdade -- que e
// exatamente onde ela roda.
void registerSdkNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context);

} // namespace bt_nodes
