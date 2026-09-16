#pragma once

#include "bt/NodeContext.hpp"
#include "domain/WorldViewFieldRegistry.hpp"

#include "xrlbridge/Schema.hpp"

#include "behaviortree_cpp_v3/condition_node.h"

#include <string>

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

// SUCCESS quando a saida de um modelo .onnx passa de um limiar.
//
// E o no GENERICO de inferencia: ele nao sabe o que o modelo calcula. Monta o
// vetor de entrada a partir do WorldView (os campos que a porta 'schema'
// resolver -- ver bt/ObservationSchema.hpp), roda, e compara UMA das saidas
// com o limiar. Serve para um classificador de ameaca, um estimador de
// risco, um "vale a pena engajar?" -- qualquer pergunta de SIM/NAO que se
// queira treinar em vez de escrever a mao.
//
// PORTS (convencao do BehaviorTree.CPP: providedPorts + getInput):
//   model      caminho do .onnx (obrigatorio)
//   threshold  limiar de comparacao (default 0.5)
//   index      qual saida comparar (default 0)
//   above      true = SUCCESS se saida > limiar; false = se saida < limiar
//   schema     campos da observacao, e ordem: "classic28" (default, os 28
//              historicos), "all" (os 38 completos) ou uma lista ad-hoc
//              separada por espaco.
//
// DEGRADACAO: modelo ausente, forma errada (contagem OU identidade dos
// campos -- ver xinfer::fields()), nome de campo desconhecido no schema, ou
// falha de inferencia devolvem FAILURE -- nunca excecao. O Fallback da
// arvore cai no ramo seguinte, que e exatamente o que se quer: um .onnx
// quebrado nao pode tirar a aeronave do ar.
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
   // cache de verdade e por CAMINHO e mora em libs/xinfer -- aqui so se
   // guarda o id ja resolvido, e a flag que impede uma segunda tentativa.
   int modelId_{};
   bool tentouAbrir_{};

   // O schema resolvido na PRIMEIRA tentativa -- nunca reavaliado depois.
   xrlbridge::BoundSchema<domain::WorldView> bound_;
};

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
