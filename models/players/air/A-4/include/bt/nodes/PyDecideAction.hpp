#pragma once

#include "bt/NodeContext.hpp"
#include "domain/WorldViewFieldRegistry.hpp"

#include "xrlbridge/Schema.hpp"

#include "behaviortree_cpp_v3/action_node.h"

#include <string>

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

// Comanda a aeronave com a saida de um script PYTHON -- a peca de
// prototipagem.
//
// Escreva a regra em Python, rode, veja o efeito. Se ela se provar, reescreva
// em C++ como um no de verdade. E a tensao registrada no TODO.md ("testar em
// python facilita a prototipagem mas abre margem para desafios de
// integracao") com os desafios de integracao ja resolvidos em
// libs/xpyembed.
//
// O SCRIPT tem de definir:
//
//     def decide(obs):        # obs: lista de floats, na ordem do 'schema'
//         return (heading_deg, altitude_m, speed_kts)
//
// O DEFAULT ("classic28") e' a mesma ordem historica de 28 campos de sempre
// -- um script existente continua funcionando sem tocar em nada. Quem muda
// a porta 'schema' precisa reescrever o script para a NOVA lista/ordem (o
// script indexa por POSICAO, sem checagem de nome nenhuma do lado do
// interprete embutido -- ver src/poc/python-flight/README.md).
//
// PORTS:
//   script  caminho do .py (obrigatorio)
//   label   rotulo no dump e no quadro (default "PY")
//   schema  campos da observacao, e ordem: "classic28" (default, os 28
//           historicos), "all" (os 38 completos) ou uma lista ad-hoc
//           separada por espaco.
//
// ISOLAMENTO POR AERONAVE: cada BtBehavior constroi a SUA arvore, entao cada
// aeronave tem a SUA instancia deste no -- e cada instancia recebe um id
// proprio, que vira um dicionario de globais proprio em libs/xpyembed. Dois
// avioes rodando o mesmo arquivo nao compartilham estado, que e o que mantem o
// resultado independente da ordem em que as threads pegam o GIL.
//
// (O id vem de um contador de instancia, e nao do playerId, porque
// DecisionContext nao expoe o player -- e nao precisa: o que importa e que
// cada aeronave tenha um id ESTAVEL e DISTINTO, nao qual numero ele e. A
// ordem de construcao pode variar entre execucoes; o conteudo de cada
// dicionario nao, porque todos comecam iguais e cada aeronave usa sempre o
// seu.)
//
// DEGRADACAO: sem Python no sistema, script ausente, sem decide(), ou excecao
// -> FAILURE, e o Fallback da arvore cai no ramo seguinte.
class PyDecideAction final : public BT::SyncActionNode
{
public:
   PyDecideAction(const std::string& name, const BT::NodeConfiguration& config,
                  const NodeContext& context);

   static BT::PortsList providedPorts();

protected:
   BT::NodeStatus tick() override;

private:
   NodeContext context_;

   int scriptId_{};
   bool tentouCarregar_{};
   int instanciaId_{};

   // O schema resolvido na PRIMEIRA tentativa -- nunca reavaliado depois.
   xrlbridge::BoundSchema<domain::WorldView> bound_;
};

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
