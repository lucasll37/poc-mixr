#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Relaia a guiagem NATIVA do Route/Steerpoint (domain::WorldView::
// hasNavSteering/navTrueBrgDeg/...) como uma decisao do UBF, com UM
// unico acrescimo: um limitador de taxa no RUMO comandado. Ver
// src/bt/nodes/NavigateAction.cpp para o porque (a mesma classe de
// problema, e a mesma classe de solucao, ja documentada na secao "Demo:
// missil guiado" deste repositorio para domain::pursuit()).
class NavigateAction final : public BT::SyncActionNode
{
public:
   NavigateAction(const std::string& name, const BT::NodeConfiguration& config, const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;

   // Estado PRIVADO deste no -- ao contrario de PatrolPlan/RtbPlan/
   // ThreatPolicy (compartilhados via DecisionContext porque outros nos
   // tambem os leem), o rumo comandado do quadro anterior so interessa a
   // ESTE no, entao fica aqui, nao em domain:: exposto pela interface.
   bool hasCommandedHeading_{};
   double commandedHeadingDeg_{};
};

} // namespace bt_nodes
