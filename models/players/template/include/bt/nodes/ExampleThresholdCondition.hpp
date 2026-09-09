#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

namespace mixr {
namespace models {
namespace xtemplate {
namespace bt {

// SUCCESS enquanto o gatilho de domain::ExampleThreshold esta engajado.
//
// E ESTE no que AVANCA a regra (le o estado anterior do contexto, calcula o
// proximo, escreve de volta) -- o comportamento so guarda o valor entre
// ticks. Ver o .cpp para o porque de ser aqui e nao la.
//
// PORT 'margin': convencao do BehaviorTree.CPP para parametrizar um no pelo
// XML (providedPorts + getInput). A divisao de responsabilidade e a mesma do
// ( FuelLow margin ) de models/players/A-4: os limiares sao propriedade da
// ENTIDADE (slots onValue/offValue do .edl); a margem e propriedade da
// ARVORE, entao o mesmo modelo serve a dois cenarios com sensibilidades
// diferentes sem recompilar.
class ExampleThresholdCondition final : public BT::ConditionNode
{
public:
   ExampleThresholdCondition(const std::string& name, const BT::NodeConfiguration& config,
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
