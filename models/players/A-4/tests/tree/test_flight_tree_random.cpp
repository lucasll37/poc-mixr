// A arvore de ACROBACIA (configs/flight_tree_random.xml): ( SlowRoll ) por
// cima de ( Navigate ) num Fallback -- ver o cabecalho do proprio XML.
//
// O que estes testes travam e a PROPRIEDADE que faz o Fallback funcionar sem
// uma condicao separada: ( SlowRoll ) FALHA enquanto nao e' hora de rolar,
// entao a navegacao segue normal; quando o sorteio vence, ele assume e passa
// a comandar o AILERON (rollOverride), nao o rumo.
//
// Nenhum destes testes sobe Station, player ou terreno -- o caminho do XML
// vem do meson (FLIGHT_TREE_RANDOM_XML), nao de uma copia colada aqui.

#include "bt/bt_factory.hpp"
#include "tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

#include <cmath>

namespace {

using testing_support::FakeDecisionContext;

class ArvoreDeAcrobacia
{
public:
   explicit ArvoreDeAcrobacia(FakeDecisionContext& ctx)
   {
      bt_nodes::NodeContext node_ctx;
      node_ctx.behavior = &ctx;
      bt_nodes::registerNodes(factory_, node_ctx);
      tree_ = factory_.createTreeFromFile(FLIGHT_TREE_RANDOM_XML, BT::Blackboard::create());
   }

   BT::NodeStatus tick() { return tree_.tickRoot(); }

private:
   BT::BehaviorTreeFactory factory_;
   BT::Tree tree_;
};

FakeDecisionContext contextoEmRota()
{
   FakeDecisionContext ctx;
   ctx.snap.valid = true;
   ctx.snap.altitudeM = 3000.0;
   ctx.snap.headingDeg = 90.0;
   ctx.snap.speedKts = 300.0;
   ctx.snap.rollDeg = 0.0;
   ctx.snap.hasNavSteering = true;
   ctx.snap.navTrueBrgDeg = 42.0;
   ctx.snap.hasNavCmdAlt = true;
   ctx.snap.navCmdAltM = 3050.0;
   ctx.snap.hasNavCmdSpeed = true;
   ctx.snap.navCmdSpeedKts = 310.0;
   return ctx;
}

//------------------------------------------------------------------------------
// Com a acrobacia DESLIGADA (o default do slot), a arvore e' indistinguivel
// de flight_tree_nav.xml. E' o que garante que trocar o treeFile por este
// arquivo, sem declarar os slots, nao muda nada.
//------------------------------------------------------------------------------
TEST(ArvoreRandom, SemAcrobaciaConfiguradaVenceANavegacao)
{
   FakeDecisionContext ctx{contextoEmRota()};
   // aerobatic fica no default: stick 0 = desligado
   ArvoreDeAcrobacia arvore{ctx};

   for (int i = 0; i < 500; ++i) {
      ASSERT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   }
   EXPECT_EQ(ctx.dec.label, "NAV");
   EXPECT_FALSE(ctx.dec.command.rollOverride);
}

//------------------------------------------------------------------------------
// Ligada, a acrobacia assume no instante sorteado e devolve o comando ao
// ( Navigate ) quando o giro fecha.
//------------------------------------------------------------------------------
TEST(ArvoreRandom, AcrobaciaAssumeEDepoisDevolveANavegacao)
{
   FakeDecisionContext ctx{contextoEmRota()};
   ctx.aerobatic.configure(1.0, 1.0, 1.0, 20.0);   // intervalo fixo de 1 s
   ctx.aerobatic.setSeed(1);
   ArvoreDeAcrobacia arvore{ctx};

   // Antes de 1 s: navegando.
   for (int i = 0; i < 40; ++i) arvore.tick();     // 0.8 s
   EXPECT_EQ(ctx.dec.label, "NAV");

   // Passa 1 s -> a acrobacia assume.
   for (int i = 0; i < 20; ++i) arvore.tick();
   ASSERT_EQ(ctx.dec.label, "ROLL");
   EXPECT_TRUE(ctx.dec.command.rollOverride);
   EXPECT_DOUBLE_EQ(ctx.dec.command.rollStick, 1.0);

   // Gira a 90 deg/s ate fechar; o banco chega wrapped, como o player reporta.
   double banco{};
   for (int i = 0; i < 400 && ctx.dec.label == "ROLL"; ++i) {
      banco += 90.0 * ctx.frameDt;
      while (banco > 180.0) banco -= 360.0;
      ctx.snap.rollDeg = banco;
      arvore.tick();
   }

   EXPECT_EQ(ctx.dec.label, "NAV") << "a acrobacia nao devolveu o comando";
   EXPECT_FALSE(ctx.dec.command.rollOverride);
}

//------------------------------------------------------------------------------
// Durante a manobra o comando de ALTITUDE continua passando pelo piso
// anti-CFIT: liberar o eixo de rolagem nao e motivo para largar a rede de
// seguranca dos outros dois.
//------------------------------------------------------------------------------
TEST(ArvoreRandom, AltitudeDaAcrobaciaRespeitaOPisoDeTerreno)
{
   FakeDecisionContext ctx{contextoEmRota()};
   ctx.snap.altitudeM = 900.0;
   ctx.snap.terrainValid = true;
   ctx.snap.terrainElevM = 800.0;
   ctx.terrainClearanceM = 500.0;          // piso = 800 + 500 = 1300 m
   ctx.aerobatic.configure(0.0, 0.0, 1.0, 20.0);
   ctx.aerobatic.setSeed(1);
   ArvoreDeAcrobacia arvore{ctx};

   arvore.tick();
   ASSERT_EQ(ctx.dec.label, "ROLL");
   EXPECT_NEAR(ctx.dec.command.altitudeM, 1300.0, 1e-9);
}

} // namespace
