#pragma once

#include "bt/NodeContext.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

#include <string>

namespace bt_nodes {

// SUCCESS quando a saida de um modelo .onnx passa de um limiar.
//
// E o no GENERICO de inferencia: ele nao sabe o que o modelo calcula. Monta o
// vetor de entrada a partir do WorldView (a MESMA ordem canonica que o treino
// em src/rl usa -- ver shared/xrlbridge/ObservationFields.hpp), roda, e compara
// UMA das saidas com o limiar. Serve para um classificador de ameaca, um
// estimador de risco, um "vale a pena engajar?" -- qualquer pergunta de
// SIM/NAO que se queira treinar em vez de escrever a mao.
//
// PORTS (convencao do BehaviorTree.CPP: providedPorts + getInput):
//   model      caminho do .onnx (obrigatorio)
//   threshold  limiar de comparacao (default 0.5)
//   index      qual saida comparar (default 0)
//   above      true = SUCCESS se saida > limiar; false = se saida < limiar
//
// DEGRADACAO: modelo ausente, forma errada ou falha de inferencia devolvem
// FAILURE -- nunca excecao. O Fallback da arvore cai no ramo seguinte, que e
// exatamente o que se quer: um .onnx quebrado nao pode tirar a aeronave do ar.
class OnnxScoreCondition final : public BT::ConditionNode
{
public:
   OnnxScoreCondition(const std::string& name, const BT::NodeConfiguration& config,
                      const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;

   // Carga preguicosa e UNICA por no: a sessao custa ~9 ms (51 ms a frio), o
   // que nao cabe num frame de 20 ms se quatro avioes carregarem juntos. O
   // cache de verdade e por CAMINHO e mora em shared/xinfer -- aqui so se
   // guarda o id ja resolvido, e a flag que impede uma segunda tentativa.
   int modelId_{};
   bool tentouAbrir_{};
};

} // namespace bt_nodes
