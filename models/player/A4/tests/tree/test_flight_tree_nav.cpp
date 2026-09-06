// A arvore de NAVEGACAO (configs/flight_tree_nav.xml): um no so, ( Navigate
// ), sem Fallback nenhum por baixo -- ver o cabecalho do proprio XML para o
// porque. Nao ha copia do XML aqui de proposito, mesmo raciocinio de
// test_flight_tree.cpp: o caminho vem do meson (FLIGHT_TREE_NAV_XML).
//
// Nenhum destes testes sobe Station, player, radar ou terreno -- o
// FakeDecisionContext e o mesmo usado pela arvore de producao, so que aqui
// quem se escreve e domain::WorldView::hasNavSteering/navTrueBrgDeg/... em
// vez de contato/alerta/combustivel.

#include "bt/bt_factory.hpp"
#include "tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

namespace {

constexpr double TOL{1e-9};

using testing_support::FakeDecisionContext;

class ArvoreDeNavegacao
{
public:
   explicit ArvoreDeNavegacao(FakeDecisionContext& ctx)
   {
      bt_nodes::NodeContext node_ctx;
      node_ctx.behavior = &ctx;
      bt_nodes::registerNodes(factory_, node_ctx);
      tree_ = factory_.createTreeFromFile(FLIGHT_TREE_NAV_XML, BT::Blackboard::create());
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
   ctx.snap.altitudeM = 1750.0;
   ctx.snap.headingDeg = 90.0;
   ctx.snap.speedKts = 160.0;
   ctx.snap.hasNavSteering = true;
   ctx.snap.navTrueBrgDeg = 42.0;
   ctx.snap.hasNavCmdAlt = true;
   ctx.snap.navCmdAltM = 1850.0;
   ctx.snap.hasNavCmdSpeed = true;
   ctx.snap.navCmdSpeedKts = 175.0;
   return ctx;
}

//------------------------------------------------------------------------------
// A arvore carrega, e o no do XML tem construtor registrado
//------------------------------------------------------------------------------

TEST(FlightTreeNav, CarregaOXmlSemLancar)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   EXPECT_NO_THROW({ ArvoreDeNavegacao arvore{ctx}; });
}

//------------------------------------------------------------------------------
// Com guiagem valida: repassa rumo/altitude/velocidade do steerpoint "to"
//------------------------------------------------------------------------------

TEST(FlightTreeNav, ComRotaValidaSegueORumoEOSteerpoint)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   ArvoreDeNavegacao arvore{ctx};

   EXPECT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken);
   EXPECT_EQ(ctx.dec.label, "NAV");
   EXPECT_NEAR(ctx.dec.command.headingDeg, 42.0, TOL);
   EXPECT_NEAR(ctx.dec.command.altitudeM, 1850.0, TOL);
   EXPECT_NEAR(ctx.dec.command.speedKts, 175.0, TOL);
}

// Sem altitude/velocidade comandada pelo steerpoint (ex.: um FIX sem
// 'altitude:'/'airspeed:' no EDL), o comando cai para o valor ATUAL --
// mantem o que ja estava, em vez de comandar zero.
TEST(FlightTreeNav, SemAltitudeOuVelocidadeComandadaMantemOAtual)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   ctx.snap.hasNavCmdAlt = false;
   ctx.snap.hasNavCmdSpeed = false;
   ArvoreDeNavegacao arvore{ctx};

   arvore.tick();

   EXPECT_NEAR(ctx.dec.command.altitudeM, ctx.snap.altitudeM, TOL);
   EXPECT_NEAR(ctx.dec.command.speedKts, ctx.snap.speedKts, TOL);
}

//------------------------------------------------------------------------------
// Sem guiagem valida: nao ha rede de seguranca -- a arvore nao decide
//------------------------------------------------------------------------------

TEST(FlightTreeNav, SemGuiagemValidaNaoDecideNada)
{
   FakeDecisionContext ctx{contextoComRotaValida()};
   ctx.snap.hasNavSteering = false;
   ArvoreDeNavegacao arvore{ctx};

   EXPECT_EQ(arvore.tick(), BT::NodeStatus::FAILURE);
   EXPECT_FALSE(ctx.dec.taken) << "esta arvore nao tem Patrol/RTB/Evade por baixo";
}

} // namespace
