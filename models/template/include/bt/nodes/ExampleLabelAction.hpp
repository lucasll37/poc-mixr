#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace mixr {
namespace models {
namespace xtemplate {
namespace bt {

// Fecha a decisao deste tick com o rotulo que o XML mandar. Sempre SUCCESS
// -- e o ramo "aconteceu alguma coisa" de um Fallback.
//
// PORT 'label': e o que permite UM no C++ servir a TODOS os ramos da arvore
// (ver configs/example_tree.xml, que o usa duas vezes com rotulos
// diferentes). Um modelo de verdade normalmente tem um no POR ramo, cada um
// com sua propria logica -- ver ( Patrol )/( ReportAndEvade )/( SupportAlert )
// em models/players/A-4. Aqui um no so basta porque nao ha logica nenhuma
// alem do rotulo.
class ExampleLabelAction final : public BT::SyncActionNode
{
public:
   ExampleLabelAction(const std::string& name, const BT::NodeConfiguration& config,
                      const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt
} // namespace xtemplate
} // namespace models
} // namespace mixr
