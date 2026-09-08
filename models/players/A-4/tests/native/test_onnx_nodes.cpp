// OnnxScoreCondition/OnnxPolicyAction (bt/nodes/) -- nesta camada (native),
// nao em tree/, porque os dois arrastam libs/xinfer -> sdk_dep
// (Requires: mixr no .pc), fora de bt_sources de proposito (ver o
// comentario grande em tests/meson.build). Registrados por
// bt_nodes::registerSdkNodes(), nunca por registerNodes().
//
// DEBITO DE COBERTURA FECHADO AQUI (achado por auditoria, nao redescobrir):
// o comentario de tests/meson.build ja afirmava "estes nos sao exercitados
// pelo test_native" -- mas nenhum teste, em lugar nenhum do repositorio,
// de fato construia/tickava OnnxScoreCondition/OnnxPolicyAction ate agora.
// A cobertura real vinha so de scenario-policy-onnx (o binario completo,
// caminho feliz com um .onnx de pesos aleatorios) -- que prova a cadeia
// inteira funciona, mas nao isola os tres casos de borda abaixo (porta
// 'model' ausente, caminho inexistente, indice fora da faixa). Reusa
// exatamente o mesmo '.onnx' de pesos aleatorios que scenario-policy-onnx
// ja usa (models/players/A-4/configs/policy_example.onnx) -- forma certa
// (28 entradas -> 3 saidas), pesos irrelevantes pros casos testados aqui.

#include "bt/nodes/OnnxPolicyAction.hpp"
#include "bt/nodes/OnnxScoreCondition.hpp"
#include "bt/bt_factory_sdk.hpp"

#include "../tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

#ifndef POLICY_EXAMPLE_ONNX
#error "POLICY_EXAMPLE_ONNX precisa ser definido pelo build -- ver tests/meson.build"
#endif

namespace {

using testing_support::FakeDecisionContext;

// Uma arvore de UM no so, construida de texto (sem arquivo .xml) -- o
// mesmo padrao de tests/tree/test_flight_tree_nav.cpp, so que registrando
// via registerSdkNodes() (nao registerNodes()) e criando via
// createTreeFromText() (o no vem parametrizado por atributo, entao nao ha
// vantagem num arquivo fixo por caso).
class ArvoreDeUmNo
{
public:
   ArvoreDeUmNo(FakeDecisionContext& ctx, const std::string& xml)
   {
      bt_nodes::NodeContext node_ctx;
      node_ctx.behavior = &ctx;
      bt_nodes::registerSdkNodes(factory_, node_ctx);
      tree_ = factory_.createTreeFromText(xml, BT::Blackboard::create());
   }

   BT::NodeStatus tick() { return tree_.tickRoot(); }

private:
   BT::BehaviorTreeFactory factory_;
   BT::Tree tree_;
};

FakeDecisionContext contextoValido()
{
   FakeDecisionContext ctx;
   ctx.snap.valid = true;
   ctx.snap.northM = 100.0;
   ctx.snap.eastM = 200.0;
   ctx.snap.altitudeM = 1750.0;
   ctx.snap.headingDeg = 90.0;
   ctx.snap.speedKts = 160.0;
   return ctx;
}

std::string arvoreOnnxScore(const std::string& atributos)
{
   return "<root main_tree_to_execute=\"MainTree\"><BehaviorTree ID=\"MainTree\">"
          "<OnnxScore " + atributos + "/></BehaviorTree></root>";
}

std::string arvoreOnnxPolicy(const std::string& atributos)
{
   return "<root main_tree_to_execute=\"MainTree\"><BehaviorTree ID=\"MainTree\">"
          "<OnnxPolicy " + atributos + "/></BehaviorTree></root>";
}

} // namespace

//------------------------------------------------------------------------------
// OnnxScoreCondition
//------------------------------------------------------------------------------

TEST(OnnxScoreCondition, PortaModelAusenteFalha)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxScore("")};
   EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE);
}

TEST(OnnxScoreCondition, CaminhoInexistenteFalhaSemLancar)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxScore("model=\"/nao/existe/politica.onnx\"")};
   EXPECT_NO_THROW({ EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE); });
}

TEST(OnnxScoreCondition, ModeloValidoComIndiceForaDaFaixaFalha)
{
   // O modelo de pesos aleatorios tem 3 saidas (XRLBRIDGE_ACTION_SIZE) --
   // pedir a saida 15 (dentro do buffer INTERNO de 16 floats do no, mas
   // fora das 3 que o modelo de fato escreve) tem que falhar pelo
   // bounds-check (tick(), linha "indice < 0 || indice >= escritos"), NAO
   // por acaso.
   //
   // Por que 15 e nao um numero bem maior (ex.: 99): 'saida' e' um
   // std::array<float, 16> interno ao no -- um indice ALEM desses 16 leria
   // fora do proprio buffer (UB de verdade, pegaria ASan em make
   // test-asan). 15 fica DENTRO do array (zero-inicializado, nunca escrito
   // pelas 3 saidas reais) -- seguro de ler, mas ainda >= escritos.
   //
   // 'above="false"' (SUCCESS se valor < limiar) e' o que faz este teste
   // PROVAR o bounds-check especificamente, nao so' "deu FAILURE por
   // acaso": sem o bounds-check, ler saida[15]==0.0 (zero-inicializado)
   // contra o limiar default 0.5 com above=false DARIA SUCCESS (0.0 < 0.5)
   // -- confirmado revertendo o bounds-check e rodando este teste, que
   // FALHA nesse cenario (rc=SUCCESS em vez do FAILURE esperado).
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxScore(
      "model=\"" POLICY_EXAMPLE_ONNX "\" index=\"15\" above=\"false\" threshold=\"0.5\"")};
   EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE);
}

TEST(OnnxScoreCondition, ModeloValidoComIndiceValidoNaoLancaEDecideAlgo)
{
   // Pesos aleatorios -- nao ha valor de saida previsivel pra afirmar
   // SUCCESS especificamente, so que o no COMPLETA (nao lanca, nao trava)
   // e devolve um NodeStatus valido (SUCCESS ou FAILURE, nunca IDLE/RUNNING
   // -- este e' um ConditionNode, tick() sincrono).
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxScore(
      "model=\"" POLICY_EXAMPLE_ONNX "\" index=\"0\" threshold=\"0.5\" above=\"true\"")};
   BT::NodeStatus status{};
   EXPECT_NO_THROW({ status = arvore.tick(); });
   EXPECT_TRUE(status == BT::NodeStatus::SUCCESS || status == BT::NodeStatus::FAILURE);
}

TEST(OnnxScoreCondition, SegundoTickReusaAAberturaSemReabrirOArquivo)
{
   // 'tentouAbrir_' e' UMA tentativa so' (mesmo molde de BtBehavior::
   // treeBuilt) -- tickar duas vezes o MESMO no nao pode lancar nem mudar
   // de FAILURE (porta ausente) pra outra coisa so' porque tentou de novo.
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxScore("")};
   EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE);
   EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE);
}

//------------------------------------------------------------------------------
// OnnxPolicyAction
//------------------------------------------------------------------------------

TEST(OnnxPolicyAction, PortaModelAusenteFalhaENaoDecide)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy("")};
   EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE);
   EXPECT_FALSE(ctx.dec.taken);
}

TEST(OnnxPolicyAction, CaminhoInexistenteFalhaSemLancar)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy("model=\"/nao/existe/politica.onnx\"")};
   EXPECT_NO_THROW({ EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE); });
   EXPECT_FALSE(ctx.dec.taken);
}

TEST(OnnxPolicyAction, ModeloComFormaCertaDecideEUsaORotuloDaPorta)
{
   // Forma 28->3 bate com o contrato (XRLBRIDGE_OBSERVATION_SIZE/
   // XRLBRIDGE_ACTION_SIZE) mesmo com pesos aleatorios -- SUCCESS e'
   // alcancavel, e o rotulo da PORTA (nao o default "ONNX") tem que
   // aparecer na decisao.
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy(
      "model=\"" POLICY_EXAMPLE_ONNX "\" label=\"ONNX-TESTE\"")};
   ASSERT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken);
   EXPECT_EQ(ctx.dec.label, "ONNX-TESTE");
}

TEST(OnnxPolicyAction, SaidaNaoNormalizadaUsaOsTresValoresCruDoModeloDireto)
{
   // 'normalized=false' pula unscaleCommand() -- os tres numeros crus do
   // modelo (pesos aleatorios, valor exato irrelevante) tem que ir DIRETO
   // pra heading/altitude/speed, sem a transformacao de escala. So' testa
   // que o CAMINHO nao normalizado nao lanca e ainda decide -- os valores
   // exatos dependem dos pesos aleatorios do fixture, entao nao sao
   // afirmados aqui.
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy(
      "model=\"" POLICY_EXAMPLE_ONNX "\" normalized=\"false\"")};
   EXPECT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken);
}
