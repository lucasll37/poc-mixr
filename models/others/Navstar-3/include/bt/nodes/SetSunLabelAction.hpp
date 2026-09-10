#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace bt {

// Fecha a decisao deste tick com o rotulo que o XML mandar. Sempre SUCCESS
// -- e o ramo "aconteceu alguma coisa" de um Fallback.
//
// PORT 'label': o mesmo no C++ serve aos dois ramos da arvore (ver
// configs/navstar3_sun_tree.xml), cada um com um rotulo diferente
// ("ECLIPSE"/"SUNLIT").
class SetSunLabelAction final : public BT::SyncActionNode
{
public:
   SetSunLabelAction(const std::string& name, const BT::NodeConfiguration& config,
                     const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
