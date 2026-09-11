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
   // Este teste comeca deliberadamente ABAIXO do piso (900 < 1300) para
   // exercitar o clamp -- negativo DESLIGA a borda de "margem para COMECAR"
   // (testada a parte), inclusive abaixo do proprio piso anti-CFIT, para nao
   // confundir as duas propriedades.
   ctx.slowRollMinMarginM = -1.0;
   ctx.aerobatic.configure(0.0, 0.0, 1.0, 20.0);
   ctx.aerobatic.setSeed(1);
   ArvoreDeAcrobacia arvore{ctx};

   arvore.tick();
   ASSERT_EQ(ctx.dec.label, "ROLL");
   EXPECT_NEAR(ctx.dec.command.altitudeM, 1300.0, 1e-9);
}

//------------------------------------------------------------------------------
// Sem folga de altitude, o sorteio vencido fica ADIADO: a arvore continua
// navegando (SlowRoll falha, Navigate assume), nunca cancelado -- assim que
// a folga aparece, a manobra comeca no PROXIMO tick, sem esperar um novo
// sorteio.
//------------------------------------------------------------------------------
TEST(ArvoreRandom, SemMargemDeAltitudeAdiaAManobra)
{
   FakeDecisionContext ctx{contextoEmRota()};
   ctx.snap.altitudeM = 900.0;
   ctx.snap.terrainValid = true;
   ctx.snap.terrainElevM = 0.0;
   ctx.terrainClearanceM = 500.0;          // piso = 500 m
   ctx.slowRollMinMarginM = 900.0;         // exige 1400 m; aeronave esta a 900 m
   ctx.aerobatic.configure(0.0, 0.0, 1.0, 20.0);   // sorteio ja vencido no 1o tick
   ctx.aerobatic.setSeed(1);
   ArvoreDeAcrobacia arvore{ctx};

   for (int i = 0; i < 100; ++i) {   // 2 s -- bem alem de um unico tick
      arvore.tick();
      ASSERT_EQ(ctx.dec.label, "NAV") << "rolou sem margem, no tick " << i;
   }

   // A folga aparece (a aeronave "subiu"): a manobra ja pendente comeca no
   // PROXIMO tick, sem novo sorteio.
   ctx.snap.altitudeM = 3000.0;
   arvore.tick();
   EXPECT_EQ(ctx.dec.label, "ROLL");
   EXPECT_TRUE(ctx.dec.command.rollOverride);
}

//------------------------------------------------------------------------------
// Uma manobra JA EM CURSO nunca aborta no meio, mesmo que a margem suma
// depois de comecar -- terminar a um banco arbitrario (possivelmente
// invertido) seria mais perigoso do que fechar o giro. So' a borda
// Idle->Rolling consulta a margem.
//------------------------------------------------------------------------------
TEST(ArvoreRandom, ManobraEmCursoNaoAbortaSeAMargemSumirDepois)
{
   FakeDecisionContext ctx{contextoEmRota()};
   ctx.snap.altitudeM = 3000.0;   // margem de sobra ao COMECAR
   ctx.aerobatic.configure(1.0, 1.0, 1.0, 20.0);
   ctx.aerobatic.setSeed(1);
   ArvoreDeAcrobacia arvore{ctx};

   for (int i = 0; i < 60; ++i) arvore.tick();   // 1.2 s -> comeca a rolar
   ASSERT_EQ(ctx.dec.label, "ROLL");

   // A margem some (a propria manobra derruba a altitude, como no mundo
   // real) -- a manobra continua e fecha os 360 graus de qualquer jeito.
   ctx.snap.altitudeM = 0.0;
   double banco{};
   for (int i = 0; i < 400 && ctx.dec.label == "ROLL"; ++i) {
      banco += 90.0 * ctx.frameDt;
      while (banco > 180.0) banco -= 360.0;
      ctx.snap.rollDeg = banco;
      arvore.tick();
   }

   EXPECT_EQ(ctx.dec.label, "NAV") << "a manobra deveria ter fechado os 360 graus";
   // Fechou por COMPLETAR o giro, nao por timeout nem por aborto de margem:
   // o acumulador chega perto de 360, o mesmo criterio de
   // FechaOsTrezentosESessentaGrausEVoltaAIdle em test_AerobaticPlan.cpp.
   EXPECT_GE(std::abs(ctx.aerobatic.accumulatedRollDeg()), 360.0);
}

} // namespace
