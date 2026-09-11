// A arvore de DEMONSTRACAO de evasao disparada por RWR (configs/
// flight_tree_rwr_evade_demo.xml) -- a mesma prioridade de flight_tree.xml
// com um ramo novo ("RWR_EVADE") entre RTB e a evasao por contato de radar
// proprio. Nao ha copia do XML aqui de proposito, mesmo raciocinio de
// test_flight_tree.cpp: o caminho vem do meson (FLIGHT_TREE_RWR_EVADE_DEMO_XML).
//
// Nenhum destes testes sobe Station, player, radar ou Rwr -- so' a arvore
// contra um FakeDecisionContext, tickada repetidamente para deixar o atraso
// ESTOCASTICO (domain::EvasionReactionPlan) vencer deterministicamente (a
// semente e' fixa, ver FakeDecisionContext::configurarComoNoCenario()).

#include "bt/bt_factory.hpp"
#include "tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

#include <string>

namespace {

using testing_support::FakeDecisionContext;

class ArvoreDeVooComRwrEvasao
{
public:
   explicit ArvoreDeVooComRwrEvasao(FakeDecisionContext& ctx)
   {
      bt_nodes::NodeContext node_ctx;
      node_ctx.behavior = &ctx;
      bt_nodes::registerNodes(factory_, node_ctx);
      tree_ = factory_.createTreeFromFile(FLIGHT_TREE_RWR_EVADE_DEMO_XML, BT::Blackboard::create());
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
   // Longe o bastante da base (arrivalRadius = 2 NM, ver
   // configurarComoNoCenario()) para o ramo RTB decidir "RTB" (en route),
   // nao "HOME" (ja chegou) -- mesma posicao usada por
   // test_flight_tree_missile.cpp pelo mesmo motivo.
   ctx.snap.northM = 9260.0;
   ctx.snap.eastM = 0.0;
   ctx.snap.altitudeM = 2000.0;
   ctx.snap.headingDeg = 90.0;
   ctx.snap.speedKts = 250.0;
   ctx.snap.fuelFraction = 0.90;
   ctx.snap.terrainValid = false;
   return ctx;
}

void verAmeacaRwr(FakeDecisionContext& ctx)
{
   ctx.snap.hasRwrThreat = true;
   ctx.snap.rwrThreatName = "aaa_site";
   ctx.snap.rwrThreatRangeM = 2000.0;
   ctx.snap.rwrThreatRelBearingDeg = -30.0;
   ctx.snap.rwrThreatDeltaAltM = -2000.0;   // antiaerea no chao, abaixo da aeronave
}

// Tica a arvore N vezes, alimentando rwrReaction/rwrThreat a cada passo --
// copia fiel do laco que BtBehavior::genAction() executa por frame
// (feedRwrEvasion() antes de tree.tickRoot()).
std::string decidirNVezes(FakeDecisionContext& ctx, ArvoreDeVooComRwrEvasao& arvore, const int n)
{
   std::string label;
   for (int i = 0; i < n; ++i) {
      ctx.alimentarRwrPolitica(ctx.frameDt);
      ctx.dec.reset();
      arvore.tick();
      label = ctx.dec.label;
   }
   return label;
}

//------------------------------------------------------------------------------
// A arvore carrega, e todo no do XML tem construtor registrado
//------------------------------------------------------------------------------

TEST(FlightTreeRwrEvadeDemo, CarregaAArvoreDeDemoSemAmeaca)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   EXPECT_NO_THROW({ ArvoreDeVooComRwrEvasao arvore{ctx}; });
}

TEST(FlightTreeRwrEvadeDemo, SemAmeacaContinuaPatrulhando)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComRwrEvasao arvore{ctx};

   const std::string label{decidirNVezes(ctx, arvore, 5)};
   EXPECT_EQ(label, "PATROL");
}

//------------------------------------------------------------------------------
// O atraso estocastico -- o coracao do exercicio.
//------------------------------------------------------------------------------

TEST(FlightTreeRwrEvadeDemo, AmeacaPresenteMasAtrasoNaoVencidoAindaPatrulha)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComRwrEvasao arvore{ctx};
   verAmeacaRwr(ctx);

   // Atraso de 2 s, dt de producao (0.02s, 50 Hz) -- 1 tick (0.02s) e' bem
   // menos que o atraso configurado, entao rwrReaction ainda nao reagiu.
   ctx.rwrReaction.configure(2.0, 2.0);
   ctx.rwrReaction.setSeed(7);

   const std::string label{decidirNVezes(ctx, arvore, 1)};
   EXPECT_EQ(label, "PATROL") << "reagiu antes do atraso vencer";
   EXPECT_FALSE(ctx.rwrThreat.engaged()) << "rwrThreat nao deveria estar engajada ainda";
}

TEST(FlightTreeRwrEvadeDemo, AmeacaAposAtrasoVencerDisparaEvasao)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComRwrEvasao arvore{ctx};
   verAmeacaRwr(ctx);

   ctx.rwrReaction.configure(2.0, 2.0);
   ctx.rwrReaction.setSeed(7);

   // 2.0s / 0.02s = 100 ticks para vencer o atraso, exatamente (o sorteio e
   // o decremento acontecem no MESMO update(), sem tick extra de graca --
   // ver domain/EvasionReactionPlan.hpp); 110 da folga de sobra.
   const std::string label{decidirNVezes(ctx, arvore, 110)};
   EXPECT_EQ(label, "RWR_EVADE");
   EXPECT_TRUE(ctx.rwrThreat.engaged());
}

TEST(FlightTreeRwrEvadeDemo, AmeacaComAtrasoZeroReageNoProprioTick)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComRwrEvasao arvore{ctx};
   verAmeacaRwr(ctx);
   // configurarComoNoCenario() ja deixa rwrReaction em 0.0/0.0 -- reage no
   // PROPRIO tick em que a ameaca aparece (ver domain/EvasionReactionPlan.hpp).
   const std::string label{decidirNVezes(ctx, arvore, 1)};
   EXPECT_EQ(label, "RWR_EVADE");
}

//------------------------------------------------------------------------------
// A ameaca some antes de reagir -- volta a patrulhar, sem crédito parcial.
//------------------------------------------------------------------------------

TEST(FlightTreeRwrEvadeDemo, AmeacaSomeAntesDeReagirVoltaAPatrulhar)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComRwrEvasao arvore{ctx};
   verAmeacaRwr(ctx);
   ctx.rwrReaction.configure(5.0, 5.0);
   ctx.rwrReaction.setSeed(7);

   decidirNVezes(ctx, arvore, 3);   // ainda esperando

   ctx.snap.hasRwrThreat = false;   // a antiaerea saiu do alcance do RWR
   const std::string label{decidirNVezes(ctx, arvore, 1)};
   EXPECT_EQ(label, "PATROL");
   EXPECT_FALSE(ctx.rwrThreat.engaged());
}

//------------------------------------------------------------------------------
// PRIORIDADE -- combustivel baixo vence a evasao por RWR, igual venceria a
// evasao por contato proprio.
//------------------------------------------------------------------------------

TEST(FlightTreeRwrEvadeDemo, CombustivelBaixoVenceAEvasaoPorRwr)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVooComRwrEvasao arvore{ctx};
   ctx.snap.fuelFraction = 0.20;
   verAmeacaRwr(ctx);

   const std::string label{decidirNVezes(ctx, arvore, 2)};
   EXPECT_EQ(label, "RTB") << "sem combustivel nao se evade";
}

} // namespace
