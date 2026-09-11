//
// A ARVORE DE PRODUCAO, carregada de verdade, SEM levantar Station nenhuma.
//
// E' a camada que so' existe porque bt/DecisionContext.hpp e' uma interface:
// os nos nunca tocam um objeto MIXR, entao um contexto FALSO (abaixo) basta
// para exercitar a maquina de estados inteira. 'ldd' neste binario nao mostra
// nenhuma lib do MIXR -- se algum dia mostrar, alguem incluiu um header do
// framework em bt/ e a propriedade se perdeu.
//
// O caminho do .xml chega por -DAAA_TREE_XML (ver tests/meson.build): e' o
// arquivo REAL de configs/, nunca uma copia -- uma copia comecaria certa e
// envelheceria em silencio.
//
#include "bt/DecisionContext.hpp"
#include "bt/bt_factory.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace mixr::models::xaaa;

// O contexto falso: guarda o que os nos leem e o que eles escrevem, e nada
// mais.
class FakeContext final : public bt::DecisionContext
{
public:
   bool hasTarget() const override                   { return hasTarget_; }
   const std::string& targetName() const override    { return targetName_; }
   double targetRangeM() const override               { return targetRangeM_; }
   bool weaponReady() const override                  { return weaponReady_; }
   const domain::Dome& dome() const override           { return dome_; }
   bt::AaaDecision& decision() override                { return decision_; }

   // Sufixo '_' porque os cinco nomes nus ja sao os metodos virtuais acima.
   bool hasTarget_{};
   std::string targetName_;
   double targetRangeM_{};
   bool weaponReady_{};
   domain::Dome dome_{};
   bt::AaaDecision decision_{};
};

// Monta a MESMA factory que ubf::AaaBehavior monta e carrega a MESMA arvore
// que o .so instala.
BT::Tree carregarArvore(FakeContext& ctx, BT::BehaviorTreeFactory& factory)
{
   bt::NodeContext node;
   node.behavior = &ctx;
   bt::registerNodes(factory, node);
   return factory.createTreeFromFile(AAA_TREE_XML, BT::Blackboard::create());
}

bt::AaaDecision decidir(FakeContext& ctx, BT::Tree& arvore)
{
   ctx.decision_.reset();
   arvore.tickRoot();
   return ctx.decision_;
}

TEST(AaaTree, CarregaSemNoDesconhecido)
{
   FakeContext ctx;
   BT::BehaviorTreeFactory factory;
   EXPECT_NO_THROW({ BT::Tree t = carregarArvore(ctx, factory); });
}

TEST(AaaTree, SemAlvoCaiNoRamoDeObservacao)
{
   FakeContext ctx;
   ctx.dome_ = {500.0, 5000.0};
   ctx.hasTarget_ = false;
   ctx.weaponReady_ = true;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   const bt::AaaDecision d{decidir(ctx, arvore)};
   EXPECT_EQ("WATCHING", d.label);
   EXPECT_FALSE(d.fireRequested);
}

TEST(AaaTree, AlvoDentroDoDomoComArmaDisponivelDispara)
{
   FakeContext ctx;
   ctx.dome_ = {500.0, 5000.0};
   ctx.hasTarget_ = true;
   ctx.targetName_ = "a4_intruder";
   ctx.targetRangeM_ = 2000.0;
   ctx.weaponReady_ = true;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   const bt::AaaDecision d{decidir(ctx, arvore)};
   EXPECT_EQ("FIRE", d.label);
   EXPECT_TRUE(d.fireRequested);
   EXPECT_EQ("a4_intruder", d.targetName);
}

TEST(AaaTree, SemMunicaoNaoDisparaMesmoComAlvoNoDomo)
{
   FakeContext ctx;
   ctx.dome_ = {500.0, 5000.0};
   ctx.hasTarget_ = true;
   ctx.targetName_ = "a4_intruder";
   ctx.targetRangeM_ = 2000.0;
   ctx.weaponReady_ = false;   // cabide vazio

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   const bt::AaaDecision d{decidir(ctx, arvore)};
   EXPECT_EQ("WATCHING", d.label);
   EXPECT_FALSE(d.fireRequested);
}

TEST(AaaTree, AlvoForaDoAlcanceMaximoCaiParaObservacao)
{
   FakeContext ctx;
   ctx.dome_ = {500.0, 5000.0};
   ctx.hasTarget_ = true;
   ctx.targetName_ = "a4_intruder";
   ctx.targetRangeM_ = 5000.1;
   ctx.weaponReady_ = true;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   const bt::AaaDecision d{decidir(ctx, arvore)};
   EXPECT_EQ("WATCHING", d.label);
   EXPECT_FALSE(d.fireRequested);
}

TEST(AaaTree, AlvoDentroDoAlcanceMinimoCaiParaObservacao)
{
   FakeContext ctx;
   ctx.dome_ = {500.0, 5000.0};
   ctx.hasTarget_ = true;
   ctx.targetName_ = "a4_intruder";
   ctx.targetRangeM_ = 499.9;
   ctx.weaponReady_ = true;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   const bt::AaaDecision d{decidir(ctx, arvore)};
   EXPECT_EQ("WATCHING", d.label);
   EXPECT_FALSE(d.fireRequested);
}

// A arvore sempre decide alguma coisa: um Fallback cujo ultimo ramo fosse
// condicional devolveria FAILURE e deixaria a entidade sem decisao no
// frame -- e' a propriedade que justifica o no Watch existir.
TEST(AaaTree, TodaPercepcaoProduzUmaDecisao)
{
   FakeContext ctx;
   ctx.dome_ = {500.0, 5000.0};

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   const struct { bool hasTarget; double rangeM; bool weaponReady; } casos[] = {
      {false, 0.0, true},
      {true, 100.0, true},
      {true, 500.0, true},
      {true, 2500.0, false},
      {true, 5000.0, true},
      {true, 1e6, true},
   };

   for (const auto& c : casos) {
      ctx.hasTarget_ = c.hasTarget;
      ctx.targetRangeM_ = c.rangeM;
      ctx.weaponReady_ = c.weaponReady;
      ctx.decision_.reset();
      const BT::NodeStatus status{arvore.tickRoot()};
      EXPECT_EQ(BT::NodeStatus::SUCCESS, status);
      EXPECT_FALSE(ctx.decision_.label.empty());
   }
}

} // namespace
