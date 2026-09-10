// A arvore de PRODUCAO (configs/c130_nav_tree.xml): um no so, ( Navigate ),
// sem Fallback -- este modelo so navega. Nao ha copia do XML aqui de
// proposito: o caminho vem do meson (C130_NAV_TREE_XML).
//
// Nenhum destes testes sobe Station, player, radar ou terreno.

#include "bt/bt_factory.hpp"
#include "tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

namespace {

using namespace mixr::models::xC_130;

constexpr double TOL{1e-9};

using testing_support::FakeDecisionContext;

class ArvoreDeNavegacao
{
public:
   explicit ArvoreDeNavegacao(FakeDecisionContext& ctx)
   {
      bt::NodeContext node_ctx;
      node_ctx.behavior = &ctx;
      bt::registerNodes(factory_, node_ctx);
      tree_ = factory_.createTreeFromFile(C130_NAV_TREE_XML, BT::Blackboard::create());
   }

   BT::NodeStatus tick() { return tree_.tickRoot(); }

private:
   BT::BehaviorTreeFactory factory_;
   BT::Tree tree_;
};

FakeDecisionContext contextoComRotaValida()
{
   FakeDecisionContext ctx;
   ctx.snap.valid = true;
   ctx.snap.northM = 9260.0;
   ctx.snap.eastM = 0.0;
   ctx.snap.altitudeM = 2500.0;
   ctx.snap.headingDeg = 90.0;
   ctx.snap.speedKts = 200.0;
   ctx.snap.hasNavSteering = true;
   ctx.snap.navTrueBrgDeg = 42.0;
   ctx.snap.hasNavCmdAlt = true;
   ctx.snap.navCmdAltM = 2600.0;
   ctx.snap.hasNavCmdSpeed = true;
   ctx.snap.navCmdSpeedKts = 220.0;
   return ctx;
}

TEST(C130NavTree, CarregaOXmlSemLancar)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   EXPECT_NO_THROW({ ArvoreDeNavegacao arvore{ctx}; });
}

TEST(C130NavTree, ComRotaValidaSegueORumoEOSteerpoint)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   ArvoreDeNavegacao arvore{ctx};

   EXPECT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken);
   EXPECT_EQ(ctx.dec.label, "NAV");
   EXPECT_NEAR(ctx.dec.command.headingDeg, 42.0, TOL);
   EXPECT_NEAR(ctx.dec.command.altitudeM, 2600.0, TOL);
   EXPECT_NEAR(ctx.dec.command.speedKts, 220.0, TOL);
}

// Sem altitude/velocidade comandada pelo steerpoint (ex.: um FIX sem
// 'altitude:'/'airspeed:'), o comando cai para o valor ATUAL.
TEST(C130NavTree, SemAltitudeOuVelocidadeComandadaMantemOAtual)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   ctx.snap.hasNavCmdAlt = false;
   ctx.snap.hasNavCmdSpeed = false;
   ArvoreDeNavegacao arvore{ctx};

   arvore.tick();

   EXPECT_NEAR(ctx.dec.command.altitudeM, ctx.snap.altitudeM, TOL);
   EXPECT_NEAR(ctx.dec.command.speedKts, ctx.snap.speedKts, TOL);
}

TEST(C130NavTree, SemGuiagemValidaNaoDecideNada)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   ctx.snap.hasNavSteering = false;
   ArvoreDeNavegacao arvore{ctx};

   EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE);
   EXPECT_FALSE(ctx.dec.taken) << "esta arvore nao tem Patrol/RTB/Evade por baixo";
}

// ACHADO POR AUDITORIA na A-4 (nao redescobrir aqui): um GAP de guiagem
// invalida tem que "recomecar do zero" no rumo comandado, nao retomar
// suavizando a partir do valor congelado de ANTES do gap.
TEST(C130NavTree, GapDeGuiagemInvalidaReiniciaASuavizacaoNaProximaBearing)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   ArvoreDeNavegacao arvore{ctx};

   ASSERT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   ASSERT_NEAR(ctx.dec.command.headingDeg, 42.0, TOL);

   ctx.snap.hasNavSteering = false;
   ctx.dec.reset();
   ASSERT_EQ(arvore.tick(), BT::NodeStatus::FAILURE);

   ctx.snap.hasNavSteering = true;
   ctx.snap.navTrueBrgDeg = 200.0;
   ASSERT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_NEAR(ctx.dec.command.headingDeg, 200.0, TOL)
      << "deveria ir direto pra 200 (reinicio), nao suavizar a partir do rumo congelado";
}

} // namespace
