#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Entrega o comando que rwrThreatPolicy() ja' fixou na entrada da manobra
// (mesma mecanica de ReportAndEvadeAction, so' que contra a SEGUNDA
// instancia de domain::ThreatPolicy -- ver bt/DecisionContext.hpp). Rotulos
// distintos de ReportAndEvadeAction (RWR_EVADE/RWR_BREAK, nao EVADE/BREAK)
// de proposito: e' o que deixa claro, no xboard/dump/Tacview, qual gatilho
// causou a manobra -- um contato de radar proprio ou uma ameaca de RWR.
//
// NAO transmite alerta tatico (ao contrario de ReportAndEvadeAction): esta
// e' uma reacao defensiva a uma ameaca terrestre, nao um contato aereo que
// outros companheiros precisem reconverter para apoiar.
class EvadeRwrThreatAction final : public BT::SyncActionNode
{
public:
   EvadeRwrThreatAction(const std::string& name, const BT::NodeConfiguration& config,
                        const NodeContext& context);

   static BT::PortsList providedPorts() { return {}; }

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
