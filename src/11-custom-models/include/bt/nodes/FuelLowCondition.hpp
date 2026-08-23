#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace bt_nodes {

// SUCCESS quando o combustivel esta abaixo da reserva OU o drone ainda
// esta reabastecendo (para o ramo de RTB continuar engajado ate encher).
//
// PORT 'margin' (opcional, default 0.0): margem somada a reserva do
// FuelSystem. E a forma idiomatica do BehaviorTree.CPP de parametrizar um
// no pelo XML -- providedPorts() + getInput<T>() -- e existe aqui para
// mostrar a diferenca de papeis: a reserva do TANQUE e propriedade do
// veiculo (slot EDL do FuelSystem), enquanto a margem de decisao e
// propriedade daquela ARVORE (atributo no XML). A mesma arvore pode ser
// reusada com margens diferentes sem tocar no cenario.
class FuelLowCondition final : public BT::ConditionNode
{
public:
   FuelLowCondition(const std::string& name, const BT::NodeConfiguration& config,
                    const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
