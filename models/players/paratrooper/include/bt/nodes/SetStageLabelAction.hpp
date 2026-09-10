#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace mixr {
namespace models {
namespace xparatrooper {
namespace bt {

// Fecha a decisao deste tick com o rotulo que o XML mandar. Sempre SUCCESS
// -- e o ramo "aconteceu alguma coisa" de um Fallback.
//
// PORT 'label': o que permite UM no C++ servir aos TRES ramos da arvore
// (ver configs/paratrooper_jump_tree.xml) -- mesmo padrao do
// ExampleLabelAction do template.
class SetStageLabelAction final : public BT::SyncActionNode
{
public:
   SetStageLabelAction(const std::string& name, const BT::NodeConfiguration& config,
                       const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt
} // namespace xparatrooper
} // namespace models
} // namespace mixr
