#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace mixr {
namespace models {
namespace xC_130 {
namespace bt {

// Relaia a guiagem NATIVA do Route/Steerpoint (domain::WorldView::
// hasNavSteering/navTrueBrgDeg/...) como uma decisao do UBF, com UM unico
// acrescimo: um limitador de taxa no RUMO comandado. Adaptado (nao linkado)
// de models/players/A-4/include/bt/nodes/NavigateAction.hpp -- ver o .cpp
// para o porque do limitador.
class NavigateAction final : public BT::SyncActionNode
{
public:
   NavigateAction(const std::string& name, const BT::NodeConfiguration& config,
                  const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;

   // Estado PRIVADO deste no -- o rumo comandado do quadro anterior so
   // interessa a ESTE no.
   bool hasCommandedHeading_{};
   double commandedHeadingDeg_{};
};

} // namespace bt
} // namespace xC_130
} // namespace models
} // namespace mixr
