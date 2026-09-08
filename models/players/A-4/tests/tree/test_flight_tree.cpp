// A maquina de estados do caca, carregada do flight_tree.xml DE PRODUCAO.
//
// Nao ha copia do XML aqui de proposito: o caminho vem do meson
// (FLIGHT_TREE_XML) e aponta para configs/flight_tree.xml, deste proprio projeto.
// Um teste contra uma copia provaria que a copia esta certa, o que nao
// interessa a ninguem -- o que se quer travar e a arvore que voa.
//
// Nenhum destes testes sobe Station, player, radar ou terreno.

#include "bt/bt_factory.hpp"
#include "tree/FakeDecisionContext.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <gtest/gtest.h>

#include <string>

namespace {

constexpr double TOL{1e-9};

using testing_support::FakeDecisionContext;

// Monta a arvore de producao em cima de um contexto do teste.
class ArvoreDeVoo
{
public:
   explicit ArvoreDeVoo(FakeDecisionContext& ctx)
   {
      bt_nodes::NodeContext node_ctx;
      node_ctx.behavior = &ctx;
      bt_nodes::registerNodes(factory_, node_ctx);
      tree_ = factory_.createTreeFromFile(FLIGHT_TREE_XML, BT::Blackboard::create());
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
   ctx.snap.northM = 9260.0;      // 5 NM ao norte do centro
   ctx.snap.eastM = 0.0;
   ctx.snap.altitudeM = 1750.0;
   ctx.snap.headingDeg = 90.0;
   ctx.snap.fuelFraction = 0.90;
   ctx.snap.terrainValid = true;
   ctx.snap.terrainElevM = 800.0;
   ctx.snap.altitudeAglM = 950.0;
   return ctx;
}

void verContato(FakeDecisionContext& ctx, const double relBearingDeg = 20.0)
{
   ctx.snap.hasContact = true;
   ctx.snap.contactName = "bandit1";
   ctx.snap.contactRangeM = 13.6 * 1852.0;
   ctx.snap.contactRelBearingDeg = relBearingDeg;
   ctx.snap.contactDeltaAltM = 250.0;
   ctx.snap.contactNorthM = 20000.0;
   ctx.snap.contactEastM = 5000.0;
   ctx.snap.contactAltitudeM = 2000.0;
}

void receberAlerta(FakeDecisionContext& ctx)
{
   ctx.snap.hasAlert = true;
   ctx.snap.alertSender = "falcon1";
   ctx.snap.alertContactName = "bandit1";
   ctx.snap.alertNorthM = 20000.0;
   ctx.snap.alertEastM = 5000.0;
   ctx.snap.alertAltitudeM = 2000.0;
   ctx.snap.alertRangeM = 13.6 * 1852.0;
}

//------------------------------------------------------------------------------
// A arvore carrega, e todo no do XML tem construtor registrado
//------------------------------------------------------------------------------

// Pega o erro mais chato de todos: no novo no XML sem registerBuilder. Hoje
// isso so aparece em voo, como uma arvore que silenciosamente nao decide.
TEST(FlightTree, CarregaOXmlDeProducaoSemLancar)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   EXPECT_NO_THROW({ ArvoreDeVoo arvore{ctx}; });
}

TEST(FlightTree, UmTickSempreProduzUmaDecisao)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   EXPECT_EQ(arvore.tick(), BT::NodeStatus::SUCCESS);
   EXPECT_TRUE(ctx.dec.taken) << "o Fallback tem de terminar em algum ramo";
}

//------------------------------------------------------------------------------
// Os quatro ramos, isolados
//------------------------------------------------------------------------------

TEST(FlightTree, SemNadaAcontecendoPatrulha)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "PATROL");
   EXPECT_NEAR(ctx.dec.command.altitudeM, 1750.0, TOL);
   EXPECT_NEAR(ctx.dec.command.speedKts, 160.0, TOL);
}

// Mesma rede de seguranca das duas acima, no ramo de degradacao (nenhum
// contato/alerta/combustivel baixo) -- ver o comentario grande logo apos
// CombustivelBaixoLevaAoRetorno.
TEST(FlightTree, PatrulhaRespeitaOPisoDeTerrenoSeAAltitudeConfiguradaFicarBaixaDemais)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   ctx.snap.terrainElevM = 1200.0;   // acima da altitude de patrulha configurada (1750 m)
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "PATROL");
   // piso = 1200+800 = 2000 m, acima dos 1750 m configurados.
   EXPECT_NEAR(ctx.dec.command.altitudeM, 2000.0, TOL);
}

TEST(FlightTree, ContatoLevaAEvasaoEPedeOAlerta)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   verContato(ctx);
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "EVADE");
   EXPECT_TRUE(ctx.dec.broadcastAlert);
   EXPECT_EQ(ctx.dec.alertContactName, "bandit1");
   EXPECT_NEAR(ctx.dec.command.speedKts, 185.0, TOL);
}

TEST(FlightTree, AlertaSozinhoLevaAoApoio)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   receberAlerta(ctx);
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "SUPPORT");
   EXPECT_NEAR(ctx.dec.command.speedKts, 180.0, TOL);
   EXPECT_NEAR(ctx.dec.command.altitudeM, 2000.0, TOL);
   EXPECT_FALSE(ctx.dec.broadcastAlert) << "quem apoia nao retransmite";
}

TEST(FlightTree, CombustivelBaixoLevaAoRetorno)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   ctx.snap.fuelFraction = 0.30;
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "RTB");
   EXPECT_NEAR(ctx.dec.command.altitudeM, 2050.0, TOL);
}

//------------------------------------------------------------------------------
// Piso anti-CFIT em RTB/SUPPORT -- ACHADO POR AUDITORIA (ver o comentario
// grande em bt/DecisionContext.hpp): antes desta rodada, so' o ramo EVADE
// respeitava domain/TerrainFloor.hpp. Os dois testes acima
// (CombustivelBaixoLevaAoRetorno/AlertaSozinhoLevaAoApoio) NUNCA exercitavam
// o clamp -- o terreno configurado la (800 m) mais a folga (800 m) da um
// piso de 1600 m, abaixo dos 2050/2000 m que os dois ja comandam sem
// precisar de ajuda. Aqui o terreno e' deliberadamente mais alto que a
// altitude configurada, para provar que o piso de fato ENTRA em acao, nao
// so' que ele nao atrapalha o caso feliz.
//------------------------------------------------------------------------------

TEST(FlightTree, RetornoABaseRespeitaOPisoDeTerrenoQuandoOCaminhoCruzaRelevoAlto)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   ctx.snap.fuelFraction = 0.30;                 // dispara RTB
   ctx.snap.terrainElevM = 1500.0;               // relevo no caminho de volta, acima do circuito
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   ASSERT_EQ(ctx.dec.label, "RTB");
   // rtbAltitude configurado (2050 m) fica ABAIXO do piso (1500+800=2300 m)
   // -- sem o clamp, a aeronave voaria reto pro relevo.
   EXPECT_NEAR(ctx.dec.command.altitudeM, 2300.0, TOL);
}

TEST(FlightTree, ApoioRespeitaOPisoDeTerrenoQuandoOContatoReportadoEstaBaixo)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   receberAlerta(ctx);
   ctx.snap.alertAltitudeM = 300.0;               // contato reportado voando baixo/terrain-following
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   ASSERT_EQ(ctx.dec.label, "SUPPORT");
   // 300 m fica bem abaixo do piso (terrainElevM=800 + terrainClearanceM=800
   // = 1600 m) -- sem o clamp, o apoio desceria ate' a MESMA altitude baixa
   // do contato, so' porque foi isso que o alerta de outro player reportou.
   EXPECT_NEAR(ctx.dec.command.altitudeM, 1600.0, TOL);
}

TEST(FlightTree, SobreABaseORotuloEhHome)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   ctx.snap.fuelFraction = 0.30;
   ctx.snap.northM = 0.0;
   ctx.snap.eastM = 0.0;
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "HOME");
}

//------------------------------------------------------------------------------
// O limiar do combustivel: reserva (0.35) + margem do XML (0.05) = 0.40
//------------------------------------------------------------------------------

TEST(FlightTree, LimiarDeCombustivelSomaAMargemDoXml)
{
   {
      FakeDecisionContext ctx{contextoEmPatrulha()};
      ArvoreDeVoo arvore{ctx};
      ctx.snap.fuelFraction = 0.39;
      ctx.alimentarPolitica(ctx.frameDt);
      arvore.tick();
      EXPECT_EQ(ctx.dec.label, "RTB") << "0.39 esta abaixo de 0.35+0.05";
   }
   {
      FakeDecisionContext ctx{contextoEmPatrulha()};
      ArvoreDeVoo arvore{ctx};
      ctx.snap.fuelFraction = 0.41;
      ctx.alimentarPolitica(ctx.frameDt);
      arvore.tick();
      EXPECT_EQ(ctx.dec.label, "PATROL") << "0.41 ainda esta acima do limiar";
   }
   {
      // Borda exata: fuelFraction == reserva + margem, calculado com a MESMA
      // soma em ponto flutuante que FuelLowCondition faz (0.35 + 0.05 !=
      // 0.40 exato em double -- ver o teste abaixo). A comparacao e "<"
      // estrita, entao exatamente no limiar AINDA NAO e "baixo": travando
      // isso para nao virar "<=" por engano depois.
      FakeDecisionContext ctx{contextoEmPatrulha()};
      ArvoreDeVoo arvore{ctx};
      ctx.snap.fuelFraction = ctx.fuelReserve + 0.05;
      ctx.alimentarPolitica(ctx.frameDt);
      arvore.tick();
      EXPECT_EQ(ctx.dec.label, "PATROL") << "exatamente no limiar ainda nao e combustivel baixo";
   }
}

//------------------------------------------------------------------------------
// PRIORIDADE -- a propriedade que so a arvore inteira responde
//------------------------------------------------------------------------------

TEST(FlightTree, CombustivelBaixoVenceOContato)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   ctx.snap.fuelFraction = 0.20;
   verContato(ctx);
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "RTB") << "sem combustivel nao se combate";
   EXPECT_FALSE(ctx.dec.broadcastAlert);
}

TEST(FlightTree, ContatoVenceOAlerta)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   verContato(ctx);
   receberAlerta(ctx);
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "EVADE") << "o proprio intruso vem antes do de outro";
}

TEST(FlightTree, AlertaVenceAPatrulha)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   receberAlerta(ctx);
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();
   EXPECT_EQ(ctx.dec.label, "SUPPORT");
}

TEST(FlightTree, CombustivelBaixoVenceTudo)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   ctx.snap.fuelFraction = 0.10;
   verContato(ctx);
   receberAlerta(ctx);
   ctx.alimentarPolitica(ctx.frameDt);
   arvore.tick();
   EXPECT_EQ(ctx.dec.label, "RTB");
}

//------------------------------------------------------------------------------
// EVADE vs BREAK, e a regra de nao retransmitir posicao velha
//------------------------------------------------------------------------------

TEST(FlightTree, PistaPerdidaViraBreakSemRetransmitir)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};

   verContato(ctx);
   ctx.alimentarPolitica(1.0);
   arvore.tick();
   ASSERT_EQ(ctx.dec.label, "EVADE");
   ASSERT_TRUE(ctx.dec.broadcastAlert);

   // o intruso sai do setor do radar: a manobra continua, o alerta nao
   ctx.snap.hasContact = false;
   ctx.dec.reset();
   ctx.alimentarPolitica(1.0);
   arvore.tick();

   EXPECT_EQ(ctx.dec.label, "BREAK");
   EXPECT_FALSE(ctx.dec.broadcastAlert) << "no arrasto a posicao ja e velha";
}

// A razao de ser da histerese: sem ela, EVADE e SUPPORT alternariam.
TEST(FlightTree, DuranteOArrastoOApoioNaoRoubaADecisao)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};

   verContato(ctx);
   ctx.alimentarPolitica(1.0);
   arvore.tick();
   ASSERT_EQ(ctx.dec.label, "EVADE");

   ctx.snap.hasContact = false;
   receberAlerta(ctx);            // outro aviao avisa no meio da quebra

   for (int s = 1; s <= 29; ++s) {
      ctx.dec.reset();
      ctx.alimentarPolitica(1.0);
      arvore.tick();
      EXPECT_EQ(ctx.dec.label, "BREAK") << "o apoio assumiu no segundo " << s;
   }

   ctx.dec.reset();
   ctx.alimentarPolitica(1.0);    // histerese expira
   arvore.tick();
   EXPECT_EQ(ctx.dec.label, "SUPPORT") << "expirada a manobra, o apoio assume";
}

//------------------------------------------------------------------------------
// A patrulha so envelhece quando e ela quem decide
//------------------------------------------------------------------------------

TEST(FlightTree, RamosDeCimaCongelamORelogioDaPatrulha)
{
   FakeDecisionContext ctx{contextoEmPatrulha()};
   ArvoreDeVoo arvore{ctx};
   ctx.frameDt = 1.0;

   for (int s = 0; s < 30; ++s) { ctx.alimentarPolitica(1.0); arvore.tick(); }
   ASSERT_EQ(ctx.dec.label, "PATROL");
   const double restante{ctx.patrol.legTimeRemaining()};
   const int perna{ctx.patrol.legIndex()};

   // 100 frames em evasao: a perna nao pode envelhecer
   verContato(ctx);
   for (int s = 0; s < 100; ++s) { ctx.alimentarPolitica(1.0); arvore.tick(); }
   ASSERT_EQ(ctx.dec.label, "EVADE");

   EXPECT_EQ(ctx.patrol.legIndex(), perna);
   EXPECT_NEAR(ctx.patrol.legTimeRemaining(), restante, TOL)
      << "a patrulha tem de ser retomada de onde parou";
}

} // namespace
