// A arvore de DEMONSTRACAO de lancamento de missil (configs/
// flight_tree_missile_demo.xml) -- a mesma prioridade de flight_tree.xml
// com um ramo novo ("disparar") entre RTB e evasao. Nao ha copia do XML
// aqui de proposito, mesmo raciocinio de test_flight_tree.cpp: o caminho
// vem do meson (FLIGHT_TREE_MISSILE_DEMO_XML).
//
// Nenhum destes testes sobe Station, player, radar ou StoresMgr -- so' a
// arvore contra um FakeDecisionContext.

#include "bt/bt_factory.hpp"
#include "tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

#include <string>

namespace {

constexpr double TOL{1e-9};

using testing_support::FakeDecisionContext;

class ArvoreDeVooComMissil
{
public:
   explicit ArvoreDeVooComMissil(FakeDecisionContext& ctx)
   {
      bt_nodes::NodeContext node_ctx;
      node_ctx.behavior = &ctx;
      bt_nodes::registerNodes(factory_, node_ctx);
      tree_ = factory_.createTreeFromFile(FLIGHT_TREE_MISSILE_DEMO_XML, BT::Blackboard::create());
   }

   BT::NodeStatus tick() { return tree_.tickRoot(); }

private:
   BT::BehaviorTreeFactory factory_;
   BT::Tree tree_;
};

FakeDecisionContext contextoEmPatrulha()
{
   FakeDecisionContext ctx;
   ctx.configurarComoNoCenario();
   ctx.snap.valid = true;
   ctx.snap.northM = 9260.0;
   ctx.snap.eastM = 0.0;
   ctx.snap.altitudeM = 1750.0;
   ctx.snap.headingDeg = 90.0;
   ctx.snap.speedKts = 260.0;
   ctx.snap.fuelFraction = 0.90;
   ctx.snap.terrainValid = true;
   ctx.snap.terrainElevM = 800.0;
   ctx.snap.altitudeAglM = 950.0;
   return ctx;
}

void verContatoNoEnvelope(FakeDecisionContext& ctx)
{
   ctx.snap.weaponReady = true;
   ctx.snap.hasContact = true;
   ctx.snap.contactName = "bandit1";
   ctx.snap.contactRangeM = 5000.0;       // dentro de [500, 9000]
   ctx.snap.contactRelBearingDeg = 10.0;  // dentro de +-45
   ctx.snap.contactDeltaAltM = 100.0;
   ctx.snap.contactNorthM = 14000.0;
   ctx.snap.contactEastM = 1000.0;
   ctx.snap.contactAltitudeM = 1850.0;
}

//------------------------------------------------------------------------------
// A arvore carrega, e todo no do XML tem construtor registrado
//------------------------------------------------------------------------------

TEST(FlightTreeMissile, CarregaOXmlDeDemoSemLancar)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   EXPECT_NO_THROW({ ArvoreDeVooComMissil arvore{ctx}; });
}

//------------------------------------------------------------------------------
// O ramo de disparo, isolado
//------------------------------------------------------------------------------

TEST(FlightTreeMissile, ArmaDisponivelEContatoNoEnvelopeDisparam)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComMissil arvore{ctx};
   verContatoNoEnvelope(ctx);
   ctx.alimentarPolitica(ctx.frameDt);

   ASSERT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_EQ(ctx.dec.label, "LAUNCH");
   EXPECT_TRUE(ctx.dec.launchRequested);
   EXPECT_EQ(ctx.dec.launchTargetName, "bandit1");
}

TEST(FlightTreeMissile, DisparoMantemRumoAltitudeEVelocidadeAtuais)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComMissil arvore{ctx};
   verContatoNoEnvelope(ctx);
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   ASSERT_EQ(ctx.dec.label, "LAUNCH");
   EXPECT_NEAR(ctx.dec.command.headingDeg, 90.0, TOL);
   EXPECT_NEAR(ctx.dec.command.altitudeM, 1750.0, TOL);
   EXPECT_NEAR(ctx.dec.command.speedKts, 260.0, TOL);
}

TEST(FlightTreeMissile, SemArmaDisponivelCaiParaEvasao)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComMissil arvore{ctx};
   verContatoNoEnvelope(ctx);
   ctx.snap.weaponReady = false;   // cabide vazio
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "EVADE");
   EXPECT_FALSE(ctx.dec.launchRequested);
}

TEST(FlightTreeMissile, ContatoForaDoAlcanceMaximoCaiParaEvasao)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComMissil arvore{ctx};
   verContatoNoEnvelope(ctx);
   ctx.snap.contactRangeM = 20.0 * 1852.0;   // 20 NM, acima dos 9000 m do envelope
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "EVADE");
   EXPECT_FALSE(ctx.dec.launchRequested);
}

TEST(FlightTreeMissile, ContatoForaDoConeCaiParaEvasao)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComMissil arvore{ctx};
   verContatoNoEnvelope(ctx);
   ctx.snap.contactRelBearingDeg = 90.0;   // acima dos +-45 do envelope
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "EVADE");
   EXPECT_FALSE(ctx.dec.launchRequested);
}

TEST(FlightTreeMissile, SemContatoNenhumNaoDisparaEPatrulha)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComMissil arvore{ctx};
   ctx.snap.weaponReady = true;   // arma pronta, mas ninguem a vista
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "PATROL");
   EXPECT_FALSE(ctx.dec.launchRequested);
}

//------------------------------------------------------------------------------
// PRIORIDADE -- combustivel baixo vence o disparo, igual venceria a evasao
//------------------------------------------------------------------------------

TEST(FlightTreeMissile, CombustivelBaixoVenceODisparo)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComMissil arvore{ctx};
   ctx.snap.fuelFraction = 0.20;
   verContatoNoEnvelope(ctx);
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "RTB") << "sem combustivel nao se dispara";
   EXPECT_FALSE(ctx.dec.launchRequested);
}

} // namespace
