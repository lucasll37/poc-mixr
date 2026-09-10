#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

namespace bt_nodes {

// Executa um slow roll (giro de 360 graus em torno do eixo longitudinal) de
// tempos em tempos, em instantes sorteados.
//
// FALHA quando nao e' hora de rolar -- e' isso que deixa o no ser posto no
// TOPO de um Fallback sem sequestrar a arvore: enquanto o sorteio nao vence,
// o Fallback segue para o ramo de baixo (( Navigate ), ( Patrol ), o que for)
// e a aeronave voa normalmente. Quando vence, este no passa a devolver
// SUCCESS e assume o comando ate o giro fechar.
//
// Quem decide QUANDO e domain::AerobaticPlan (regra pura, com o gerador e a
// semente derivada por player); este no so faz a ponte entre o plano e o
// FlightCommand.
class SlowRollAction final : public BT::SyncActionNode
{
public:
   SlowRollAction(const std::string& name, const BT::NodeConfiguration& config,
                  const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;
};

} // namespace bt_nodes
