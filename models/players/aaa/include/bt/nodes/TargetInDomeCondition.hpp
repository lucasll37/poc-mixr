#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace mixr {
namespace models {
namespace xaaa {
namespace bt {

// SUCCESS quando ha um alvo hostil percebido (ubf::AaaState, via o radar de
// aquisicao proprio da antiaerea), o cabide tem municao
// (StoresMgr::available() > 0, nunca SamVehicle::isLauncherReady() -- ver a
// armadilha documentada em include/xnative/AaaSite.hpp) e o alcance cai
// dentro do domo (domain::inDome).
//
// SEM HISTERESE, ao contrario do gatilho do template (ExampleThreshold): e'
// uma checagem instantanea, reavaliada do zero a cada tick -- a antiaerea
// nao "lembra" ter visto o alvo antes, so' reage ao que ve' agora.
class TargetInDomeCondition final : public BT::ConditionNode
{
public:
   TargetInDomeCondition(const std::string& name, const BT::NodeConfiguration& config,
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
