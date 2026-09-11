#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace mixr {
namespace models {
namespace xaaa {
namespace bt {

// Ramo de degradacao do Fallback (ver configs/aaa_tree.xml): sempre
// SUCCESS, nunca dispara. Existe para a arvore SEMPRE produzir uma decisao
// observavel (bt=WATCHING no xboard/dump) mesmo sem alvo no domo -- sem um
// ultimo ramo incondicional, um Fallback devolveria FAILURE quando nada
// casasse, e a antiaerea ficaria sem decisao nenhuma no frame (o mesmo
// raciocinio do ramo IDLE do template que originou este scaffold).
class WatchAction final : public BT::SyncActionNode
{
public:
   WatchAction(const std::string& name, const BT::NodeConfiguration& config,
              const NodeContext& context);

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt
} // namespace xaaa
} // namespace models
} // namespace mixr
