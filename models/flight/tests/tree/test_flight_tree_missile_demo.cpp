// A arvore de DEMO (configs/flight_tree_missile_demo.xml): so o ramo novo
// (LaunchEnvelope -> LaunchMissile) importa aqui -- o resto da arvore ja e
// coberto por test_flight_tree.cpp contra o XML de producao.

#include "bt/bt_factory.hpp"
#include "tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

namespace {

constexpr double TOL{1e-9};

using testing_support::FakeDecisionContext;

class ArvoreDeDemo
{
public:
   explicit ArvoreDeDemo(FakeDecisionContext& ctx)
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
   ctx.snap.fuelFraction = 0.90;
   ctx.snap.terrainValid = true;
   ctx.snap.terrainElevM = 800.0;
   ctx.snap.altitudeAglM = 950.0;
   return ctx;
}

void verContato(FakeDecisionContext& ctx, const double rangeM, const double relBearingDeg)
{
   ctx.snap.hasContact = true;
   ctx.snap.contactName = "bandit1";
   ctx.snap.contactRangeM = rangeM;
   ctx.snap.contactRelBearingDeg = relBearingDeg;
   ctx.snap.contactDeltaAltM = 250.0;
   ctx.snap.contactNorthM = ctx.snap.northM + rangeM;
   ctx.snap.contactEastM = ctx.snap.eastM;
   ctx.snap.contactAltitudeM = ctx.snap.altitudeM;
}

TEST(FlightTreeMissileDemo, CarregaSemLancar)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   EXPECT_NO_THROW({ ArvoreDeDemo arvore{ctx}; });
}

TEST(FlightTreeMissileDemo, SemArmaCaiDiretoEmEvade)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeDemo arvore{ctx};
   verContato(ctx, 5000.0, 0.0);
   ctx.snap.weaponReady = false;
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "EVADE");
   EXPECT_FALSE(ctx.dec.launchRequested);
}

TEST(FlightTreeMissileDemo, ForaDoEnvelopeNaoLanca)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeDemo arvore{ctx};
   ctx.launchEnv = domain::LaunchEnvelope{1500.0, 9000.0, 45.0};
   verContato(ctx, 20000.0, 0.0);   // acima do alcance maximo
   ctx.snap.weaponReady = true;
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "EVADE");
   EXPECT_FALSE(ctx.dec.launchRequested);
}

TEST(FlightTreeMissileDemo, ArmaDisponivelEDentroDoEnvelopeLanca)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeDemo arvore{ctx};
   ctx.launchEnv = domain::LaunchEnvelope{1500.0, 9000.0, 45.0};
   verContato(ctx, 5000.0, 10.0);
   ctx.snap.weaponReady = true;
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "LAUNCH");
   EXPECT_TRUE(ctx.dec.launchRequested);
   EXPECT_EQ(ctx.dec.launchTargetName, "bandit1");
   // o comando de voo do tick de lancamento e o mesmo da quebra de evasao --
   // ver o "porque" em bt/nodes/LaunchMissileAction.cpp.
   EXPECT_NEAR(ctx.dec.command.speedKts, 185.0, TOL);
}

TEST(FlightTreeMissileDemo, DepoisDeLancarSemArmaVoltaAEvadir)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeDemo arvore{ctx};
   ctx.launchEnv = domain::LaunchEnvelope{1500.0, 9000.0, 45.0};
   verContato(ctx, 5000.0, 10.0);

   ctx.snap.weaponReady = true;
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();
   ASSERT_EQ(ctx.dec.label, "LAUNCH");

   // so ha 1 missil na demo -- na pratica, weaponReady vira false depois do
   // primeiro lancamento (StoresMgr::available() == 0).
   //
   // dec.reset() e o que BtBehavior::genAction() faz antes de CADA tick
   // (currentDecision.reset(), ver src/ubf/BtBehavior.cpp) -- sem isso este
   // teste ficaria vendo o launchRequested do tick ANTERIOR, que
   // ReportAndEvade nao toca de proposito (ver FlightDecision::take()).
   ctx.dec.reset();
   ctx.snap.weaponReady = false;
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "EVADE");
   EXPECT_FALSE(ctx.dec.launchRequested);
}

} // namespace
