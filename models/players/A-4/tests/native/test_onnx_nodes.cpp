// OnnxScoreCondition/OnnxPolicyAction (bt/nodes/) testados nesta camada
// (nao em tree/) porque arrastam libs/xinfer -> sdk_dep (Requires: mixr),
// fora de bt_sources de proposito. Registrados por
// bt_nodes::registerSdkNodes(). Cobre os casos de borda (porta 'model'
// ausente, caminho inexistente, indice fora da faixa) que
// scenario-policy-onnx nao isola. Reusa o mesmo '.onnx' de pesos
// aleatorios que scenario-policy-onnx ja usa
// (models/players/A-4/configs/policy_example.onnx) -- forma certa
// (28 entradas -> 3 saidas), pesos irrelevantes pros casos testados aqui.

#include "bt/nodes/OnnxPolicyAction.hpp"
#include "bt/nodes/OnnxScoreCondition.hpp"
#include "bt/bt_factory_sdk.hpp"

#include "../tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

namespace bt_nodes = mixr::models::xA_4::bt_nodes;

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
   // 'saida' e' um std::array<float, 16> interno ao no -- indice 15 fica
   // dentro do array (zero-inicializado, nunca escrito pelas 3 saidas
   // reais), seguro de ler mas ainda >= numero de saidas escritas.
   // 'above="false"' prova que o teste exercita o bounds-check
   // especificamente: sem ele, ler saida[15]==0.0 contra o limiar default
   // 0.5 daria SUCCESS por acidente.
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

TEST(OnnxScoreCondition, SchemaAllFuncionaContraUmOnnxDe38Entradas)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxScore(
      "model=\"" POLICY_ALL38_ONNX "\" schema=\"all\" index=\"0\"")};
   BT::NodeStatus status{};
   EXPECT_NO_THROW({ status = arvore.tick(); });
   EXPECT_TRUE(status == BT::NodeStatus::SUCCESS || status == BT::NodeStatus::FAILURE);
}

TEST(OnnxScoreCondition, SchemaComNomeDesconhecidoFalhaSemLancar)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxScore(
      "model=\"" POLICY_EXAMPLE_ONNX "\" schema=\"northM campoInventado\"")};
   EXPECT_NO_THROW({ EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE); });
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
   // Sem porta 'schema', o DEFAULT e' "classic28" (28 entradas) -- a mesma
   // forma de POLICY_EXAMPLE_ONNX, mesmo com pesos aleatorios. SUCCESS e'
   // alcancavel, e o rotulo da PORTA (nao o default "ONNX") tem que
   // aparecer na decisao.
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy(
      "model=\"" POLICY_EXAMPLE_ONNX "\" label=\"ONNX-TESTE\"")};
   ASSERT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken);
   EXPECT_EQ(ctx.dec.label, "ONNX-TESTE");
}

//------------------------------------------------------------------------------
// OnnxPolicyAction -- a porta 'schema' (peca nova desta passada: observacao
// configuravel por schema nomeado, ver bt/ObservationSchema.hpp).
//------------------------------------------------------------------------------

TEST(OnnxPolicyAction, SchemaAllDecideContraUmOnnxDe38Entradas)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy(
      "model=\"" POLICY_ALL38_ONNX "\" schema=\"all\"")};
   ASSERT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken);
}

TEST(OnnxPolicyAction, SchemaAdHocDecideContraUmOnnxDeTresCampos)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy(
      "model=\"" POLICY_3FIELDS_ONNX "\" schema=\"northM eastM altitudeM\"")};
   ASSERT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken);
}

TEST(OnnxPolicyAction, SchemaComNomeDesconhecidoFalhaSemLancarENaoDecide)
{
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy(
      "model=\"" POLICY_EXAMPLE_ONNX "\" schema=\"northM campoInventado\"")};
   EXPECT_NO_THROW({ EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE); });
   EXPECT_FALSE(ctx.dec.taken);
}

TEST(OnnxPolicyAction, SchemaClassicDefaultRejeitaOnnxComMetadataDeCamposDivergente)
{
   // Mesma CONTAGEM (28) que 'classic28' resolve, mas a metadata
   // 'xrlbridge.fields' do proprio .onnx foi gravada com os dois primeiros
   // nomes TROCADOS (ver tests/native/fixtures/README.md) -- exatamente o
   // risco que so passou a existir com schema variavel: dois .onnx do mesmo
   // tamanho esperando campos (ou ordem) diferentes. A checagem de
   // CONTAGEM sozinha deixaria passar; a de IDENTIDADE (xinfer::fields())
   // tem que pegar.
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy(
      "model=\"" POLICY_CLASSIC28_METADATA_DIVERGENTE_ONNX "\"")};
   EXPECT_NO_THROW({ EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE); });
   EXPECT_FALSE(ctx.dec.taken);
}

TEST(OnnxPolicyAction, OnnxSemMetadataDeCamposAbreNormalmenteViaContagem)
{
   // POLICY_EXAMPLE_ONNX foi exportado ANTES desta funcionalidade existir --
   // sem metadata 'xrlbridge.fields'. So a checagem de CONTAGEM vale, como
   // sempre valeu -- nao pode virar FAILURE so por faltar um dado que nao
   // existia quando o arquivo foi gerado.
   FakeDecisionContext ctx{contextoValido()};
   ArvoreDeUmNo arvore{ctx, arvoreOnnxPolicy("model=\"" POLICY_EXAMPLE_ONNX "\"")};
   EXPECT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken);
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
