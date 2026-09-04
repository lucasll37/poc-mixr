#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/action_node.h"

#include <string>

namespace bt_nodes {

// Comanda a aeronave com a saida de uma politica .onnx -- o DEPLOY do que foi
// treinado em src/rl.
//
// E o fecho do ciclo. Ate aqui a politica de RL so rodava com um processo
// Python DIRIGINDO o frame de fora (src/rl/bindings + ubf/RLBridgeBehavior,
// uma caixa de correio com um frame de latencia). Este no roda a MESMA
// politica, exportada para .onnx, DENTRO do genAction() -- sem Python, sem
// latencia de um frame: le o WorldView deste frame e comanda neste frame.
//
// A ordem dos 28 campos de entrada e a ordem canonica de
// xrlbridge/ObservationFields.hpp -- a MESMA que o exportador
// (src/rl/tools/export_onnx.py) usa para montar o modelo. Nao ha duas listas.
//
// PORTS:
//   model       caminho do .onnx (obrigatorio)
//   normalized  true (default): a saida do modelo esta em [-1,1] e e
//               desnormalizada com os limites de ObservationFields.hpp -- e o
//               que um export do Stable-Baselines3 produz (o Tanh final).
//               false: a saida ja vem em graus/metros/nos.
//   label       rotulo que aparece no dump e no quadro (default "ONNX")
//
// DEGRADACAO: modelo ausente, forma errada ou falha de inferencia devolvem
// FAILURE sem comandar nada -- o Fallback da arvore cai no ramo seguinte
// (tipicamente a patrulha escrita a mao). Uma politica que nao carrega nao
// pode tirar a aeronave do ar.
class OnnxPolicyAction final : public BT::SyncActionNode
{
public:
   OnnxPolicyAction(const std::string& name, const BT::NodeConfiguration& config,
                    const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;

   // Uma tentativa de carga, e so -- mesmo molde do 'treeBuilt' de
   // ubf/BtBehavior. O cache por caminho mora em shared/xinfer.
   int modelId_{};
   bool tentouAbrir_{};
};

} // namespace bt_nodes
