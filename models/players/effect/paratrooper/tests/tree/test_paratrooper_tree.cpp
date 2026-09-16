//
// A ARVORE DE PRODUCAO, carregada de verdade, SEM levantar Station nenhuma.
//
// 'ldd' neste binario nao mostra nenhuma lib do MIXR -- se algum dia mostrar,
// alguem incluiu um header do framework em bt/ e a propriedade se perdeu.
//
// O caminho do .xml chega por -DPARATROOPER_TREE_XML (ver tests/meson.build):
// e' o arquivo REAL de configs/, nunca uma copia.
//
#include "bt/DecisionContext.hpp"
#include "bt/bt_factory.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace mixr::models::xparatrooper;

// O contexto falso: guarda o que os nos leem e o que eles escrevem, e nada
// mais. Diferente do template, 'stage_' NUNCA e mutado por este teste dentro
// de um tick -- e' a propriedade que NenhumTickAvancaOEstagio confere.
class FakeContext final : public bt::DecisionContext
{
public:
   domain::Stage stage() const override      { return stage_; }
   bt::JumpDecision& decision() override     { return decision_; }

   domain::Stage stage_{domain::Stage::FREEFALL};
   bt::JumpDecision decision_{};
};

// Monta a MESMA factory que ubf::ParatrooperBtBehavior monta e carrega a
// MESMA arvore que o .so instala.
BT::Tree carregarArvore(FakeContext& ctx, BT::BehaviorTreeFactory& factory)
{
   bt::NodeContext node;
   node.behavior = &ctx;
   bt::registerNodes(factory, node);
   return factory.createTreeFromFile(PARATROOPER_TREE_XML, BT::Blackboard::create());
}

std::string decidir(FakeContext& ctx, BT::Tree& arvore)
{
   ctx.decision_.reset();
   arvore.tickRoot();
   return ctx.decision_.label;
}

TEST(ParatrooperTree, CarregaSemNoDesconhecido)
{
   FakeContext ctx;
   BT::BehaviorTreeFactory factory;
   EXPECT_NO_THROW({ BT::Tree t = carregarArvore(ctx, factory); });
}

TEST(ParatrooperTree, FreefallCaiNoRamoIncondicional)
{
   FakeContext ctx;
   ctx.stage_ = domain::Stage::FREEFALL;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   EXPECT_EQ("FREEFALL", decidir(ctx, arvore));
}

TEST(ParatrooperTree, CanopyVenceOSegundoRamo)
{
   FakeContext ctx;
   ctx.stage_ = domain::Stage::CANOPY;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   EXPECT_EQ("CANOPY", decidir(ctx, arvore));
}

TEST(ParatrooperTree, LandedVenceTudo)
{
   FakeContext ctx;
   ctx.stage_ = domain::Stage::LANDED;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   EXPECT_EQ("LANDED", decidir(ctx, arvore));
}

// A propriedade central desta arvore (diferente do template): os nos sao
// PUROS. Tickar repetidas vezes com o MESMO estagio nunca deveria mudar
// 'ctx.stage_' -- se algum dia alguem reintroduzir a mutacao dentro de um
// no (como o ExampleThresholdCondition do template faz), este teste nao
// pegaria diretamente uma mutacao NO-OP, mas qualquer sequencia abaixo que
// dependa da ordem de avaliacao das duas condicoes pegaria uma dupla
// contagem -- ver TransicaoManualEntreOsTresEstagios.
TEST(ParatrooperTree, TickRepetidoNaoMudaOEstagioExterno)
{
   FakeContext ctx;
   ctx.stage_ = domain::Stage::CANOPY;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   for (int i = 0; i < 10; ++i) {
      decidir(ctx, arvore);
      EXPECT_EQ(domain::Stage::CANOPY, ctx.stage_);
   }
}

// A MESMA arvore, sem reconstruir, serve aos tres estagios -- prova de que
// ela e' sem estado proprio quanto ao "qual estagio" (o unico estado dela e'
// o blackboard interno do BT.CPP, que os nos daqui nem usam).
TEST(ParatrooperTree, MesmaArvoreServeTodosOsEstagios)
{
   FakeContext ctx;
   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   ctx.stage_ = domain::Stage::FREEFALL;
   EXPECT_EQ("FREEFALL", decidir(ctx, arvore));

   ctx.stage_ = domain::Stage::CANOPY;
   EXPECT_EQ("CANOPY", decidir(ctx, arvore));

   ctx.stage_ = domain::Stage::LANDED;
   EXPECT_EQ("LANDED", decidir(ctx, arvore));
}

// A arvore sempre decide alguma coisa: o ultimo ramo do Fallback e'
// incondicional -- sem ele, FAILURE deixaria a entidade sem decisao no
// frame. Vale inclusive para LANDED, que precisa continuar produzindo 'dec='
// (ver tests/native -- um LANDED que parasse de decidir travaria o contador
// de decisoes do check_determinism.sh).
TEST(ParatrooperTree, TodoEstagioProduzUmaDecisao)
{
   FakeContext ctx;
   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   for (const domain::Stage s : {domain::Stage::FREEFALL, domain::Stage::CANOPY, domain::Stage::LANDED}) {
      ctx.stage_ = s;
      ctx.decision_.reset();
      const BT::NodeStatus status{arvore.tickRoot()};
      EXPECT_EQ(BT::NodeStatus::SUCCESS, status) << "estagio " << domain::labelOf(s);
      EXPECT_TRUE(ctx.decision_.taken) << "estagio " << domain::labelOf(s);
   }
}

// O caminho do 'dump-tree-model': factory montada com um NodeContext SEM
// comportamento (behavior == nullptr). Os nos degradam para FAILURE sem
// lancar nem crashar.
TEST(ParatrooperTree, ContextoNuloDegradaSemCrashar)
{
   BT::BehaviorTreeFactory factory;
   bt::NodeContext node{};   // behavior == nullptr
   bt::registerNodes(factory, node);

   EXPECT_NO_THROW({
      BT::Tree arvore{factory.createTreeFromFile(PARATROOPER_TREE_XML, BT::Blackboard::create())};
      const BT::NodeStatus status{arvore.tickRoot()};
      EXPECT_EQ(BT::NodeStatus::FAILURE, status);
   });
}

} // namespace
