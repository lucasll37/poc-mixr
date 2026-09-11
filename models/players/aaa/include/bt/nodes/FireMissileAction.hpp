#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace mixr {
namespace models {
namespace xaaa {
namespace bt {

// Registra o pedido de disparo contra o alvo percebido. Sempre SUCCESS --
// so' roda depois que TargetInDomeCondition ja confirmou alvo+arma+domo, no
// Sequence da arvore (ver configs/aaa_tree.xml). Quem de fato aciona o
// StoresMgr e' ubf::AaaAction::execute(), a partir do nome do alvo/rotulo
// que esta classe grava em AaaDecision -- este no NUNCA toca um objeto MIXR.
class FireMissileAction final : public BT::SyncActionNode
{
public:
   FireMissileAction(const std::string& name, const BT::NodeConfiguration& config,
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
