//
// FlightState / AltitudeSafetyBehavior / FlightAction / RadarScan -- camada 3
// (classes MIXR proprias, SEM levantar Station) -- ver o cabecalho de
// test_xnative.cpp para o raciocinio geral desta camada.
//
// Ate agora estas quatro classes so eram exercitadas TRANSITIVAMENTE, pelos
// testes de fabrica (Factory.ConstroiTudoQueDeclara/...) -- nunca por uma
// chamada de verdade a updateState()/genAction()/execute()/radarScanOf(). A
// suite 'scenario' cobre o comportamento fim a fim, mas so afirma sobre as
// linhas 'frame=' do dump -- um regressao aqui so apareceria la de forma
// indireta (ou nem apareceria, se o campo nao influenciar o rotulo).
//
// A peca que falta para testar isso aqui, sem Station: um 'ator' de
// verdade. FlightState::updateState()/FlightAction::execute() fazem
// dynamic_cast para AirVehicle/Player e chamam getters/setters do framework
// que um dublê nao replicaria com fidelidade -- por isso o 'Bench' abaixo
// constroi um mixr::models::AirVehicle de bancada. O UNICO motivo de
// tambem precisar de um mixr::models::WorldModel e mecanico:
// Player::setPosition()/setAltitude() chamam getWorldModel()->
// getMaxRefRange()/getEarthModel()/... SEM checar nulo -- sem um WorldModel
// amarrado como container, a primeira chamada a setAltitude() segfaultaria.
// Nao ha EDL, nem JSBSim, nem terreno de verdade: so os getters/setters
// publicos que os proprios arquivos sob teste chamam.
//
#include "ubf/AltitudeSafetyBehavior.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"
#include "xnative/AlertDatalink.hpp"
#include "xnative/RadarScan.hpp"

#include "events/payloads/EID_ALERT/TacticalAlert.hpp"

#include "domain/FlightCommand.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/Autopilot.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/units/Distances.hpp"

#include <gtest/gtest.h>

namespace domain = mixr::models::xA_4::domain;

namespace {

using namespace mixr;
using namespace mixr::models;

//------------------------------------------------------------------------------
// Bench -- um AirVehicle "de bancada". air->reset() e chamado uma unica vez,
// na construcao: e o que muda 'useCoordSys' de CS_NONE para CS_LOCAL (sem
// isso, setAltitude() abaixo e um no-op silencioso -- ver Player::
// setAltitude()) e, quando ha piloto, e o que popula Player::pilot via
// updateSystemPointers() (protected; so alcancavel de fora por reset()).
//------------------------------------------------------------------------------
struct Bench
{
   models::WorldModel* const world;
   models::AirVehicle* const air;
   models::Autopilot* autopilot{};

   explicit Bench(const int id, const bool withAutopilot = false)
      : world(new models::WorldModel()), air(new models::AirVehicle())
   {
      air->container(world);
      air->setID(static_cast<unsigned short>(id));

      if (withAutopilot) {
         autopilot = new models::Autopilot();
         const auto pair = new base::Pair("pilot", autopilot);
         autopilot->unref();   // Pair::Pair() ja deu ref() -- devolve a nossa
         air->addComponent(pair);
         pair->unref();        // addComponent() ja deu ref() -- devolve a nossa
      }

      air->reset();
   }

   ~Bench()
   {
      air->unref();
      world->unref();
   }
};

//------------------------------------------------------------------------------
// FlightState -- percepcao.
//------------------------------------------------------------------------------
TEST(FlightState, ActorQueNaoEhAirVehicleInvalidaSnapshot)
{
   xA_4::FlightState state;
   base::Component naoEhAviao;
   state.updateState(&naoEhAviao);
   EXPECT_FALSE(state.snapshot().valid);
}

TEST(FlightState, PopulaCamposBasicosDeUmAirVehicleReal)
{
   Bench bench(9001);
   bench.air->setAltitude(1800.0);
   bench.air->setTerrainElevation(1200.0);

   xA_4::FlightState state;
   state.updateState(bench.air);

   const auto& snap = state.snapshot();
   EXPECT_TRUE(snap.valid);
   EXPECT_NEAR(snap.altitudeM, 1800.0, 1e-6);
   EXPECT_TRUE(snap.terrainValid);
   EXPECT_NEAR(snap.terrainElevM, 1200.0, 1e-6);
   EXPECT_NEAR(snap.altitudeAglM, 600.0, 1e-6);

   // Sem AerodynamicsModel, getFuelWtMax() e 0 -- o fallback de
   // FlightState::updateState() e fracao cheia (1.0), nao divisao por zero.
   EXPECT_DOUBLE_EQ(snap.fuelFraction, 1.0);

   // Sem OnboardComputer/Datalink/StoresMgr, os tres opcionais tem de sair
   // desligados, nao indeterminados.
   EXPECT_FALSE(snap.hasContact);
   EXPECT_FALSE(snap.hasAlert);
   EXPECT_FALSE(snap.weaponReady);
}

//------------------------------------------------------------------------------
// AltitudeSafetyBehavior -- o arbitro nativo de seguranca.
//------------------------------------------------------------------------------
TEST(AltitudeSafetyBehavior, EstadoDeTipoErradoDevolveNulo)
{
   xA_4::AltitudeSafetyBehavior behavior;
   EXPECT_EQ(behavior.genAction(nullptr, 0.02), nullptr);
}

TEST(AltitudeSafetyBehavior, AcimaDoPisoNaoGeraAcao)
{
   Bench bench(9002);
   bench.air->setAltitude(2000.0);   // acima do minAltitude default (1500 m)

   xA_4::FlightState state;
   state.updateState(bench.air);

   xA_4::AltitudeSafetyBehavior behavior;
   EXPECT_EQ(behavior.genAction(&state, 0.02), nullptr);
}

TEST(AltitudeSafetyBehavior, AbaixoDoPisoAbsolutoComandaRecuperacaoMantendoRumo)
{
   Bench bench(9003, /*withAutopilot=*/true);
   bench.air->setAltitude(500.0);   // abaixo do minAltitude default (1500 m)

   xA_4::FlightState state;
   state.updateState(bench.air);
   ASSERT_TRUE(state.snapshot().valid);

   xA_4::AltitudeSafetyBehavior behavior;
   const auto action = dynamic_cast<xA_4::FlightAction*>(behavior.genAction(&state, 0.02));
   ASSERT_NE(action, nullptr) << "abaixo do piso absoluto tinha que gerar acao de recuperacao";

   EXPECT_TRUE(action->execute(bench.air));

   // Defaults de AltitudeSafetyBehavior: recoverAltitude 3500 m, recoverSpeed 400 kts.
   EXPECT_NEAR(bench.autopilot->getCommandedAltitudeFt(), 3500.0 * base::distance::M2FT, 1e-3);
   EXPECT_DOUBLE_EQ(bench.autopilot->getCommandedVelocityKts(), 400.0);
   // "cmd.headingDeg = snap.headingDeg -- mantem o rumo, so recupera altitude"
   EXPECT_DOUBLE_EQ(bench.autopilot->getCommandedHeadingD(), bench.air->getHeadingD());
   EXPECT_TRUE(bench.autopilot->isHeadingHoldOn());
   EXPECT_TRUE(bench.autopilot->isAltitudeHoldOn());
   EXPECT_TRUE(bench.autopilot->isVelocityHoldOn());

   const xboard::Readout r{xboard::get(bench.air->getID())};
   EXPECT_EQ(r.label, "SAFETY");
   EXPECT_EQ(r.decisions, 1);

   action->unref();
}

TEST(AltitudeSafetyBehavior, AbaixoDoPisoAglRecuperaParaOMaiorEntreNominalETerrenoMaisFolga)
{
   Bench bench(9004, /*withAutopilot=*/true);
   bench.air->setTerrainElevation(1400.0);
   bench.air->setAltitude(1450.0);   // AGL = 50 m

   xA_4::FlightState state;
   state.updateState(bench.air);
   ASSERT_TRUE(state.snapshot().terrainValid);
   EXPECT_NEAR(state.snapshot().altitudeAglM, 50.0, 1e-6);

   xA_4::AltitudeSafetyBehavior behavior;

   // minAltitude bem baixo para isolar SO a camada AGL (senao o piso
   // absoluto default, 1500 m, tambem dispararia com altitude 1450 m e o
   // teste nao provaria qual camada decidiu). recoverAltitude tambem baixo,
   // para que quem vença o max() seja terreno+folga (1400+200=1600), nao o
   // nominal -- e exatamente o comentario de AltitudeSafetyBehavior.cpp:
   // "recuperar para o mais alto entre a altitude nominal e o terreno mais folga".
   base::Meters minAlt{10.0};
   base::Meters recoverAlt{1000.0};
   base::Meters minClearance{100.0};
   base::Meters recoverClearance{200.0};
   ASSERT_TRUE(behavior.setSlotByName("minAltitude", &minAlt));
   ASSERT_TRUE(behavior.setSlotByName("recoverAltitude", &recoverAlt));
   ASSERT_TRUE(behavior.setSlotByName("minClearance", &minClearance));
   ASSERT_TRUE(behavior.setSlotByName("recoverClearance", &recoverClearance));

   const auto action = dynamic_cast<xA_4::FlightAction*>(behavior.genAction(&state, 0.02));
   ASSERT_NE(action, nullptr) << "AGL abaixo de minClearance tinha que gerar acao";

   EXPECT_TRUE(action->execute(bench.air));
   EXPECT_NEAR(bench.autopilot->getCommandedAltitudeFt(), 1600.0 * base::distance::M2FT, 1e-3);

   action->unref();
}

//------------------------------------------------------------------------------
// FlightAction -- a atuacao.
//------------------------------------------------------------------------------
TEST(FlightAction, ExecuteComAtorQueNaoEhPlayerDevolveFalse)
{
   xA_4::FlightAction action;
   base::Component naoEhPlayer;
   EXPECT_FALSE(action.execute(&naoEhPlayer));
}

TEST(FlightAction, ExecuteSemAutopilotDevolveFalseESeguraOComando)
{
   Bench bench(9005, /*withAutopilot=*/false);

   xA_4::FlightAction action;
   domain::FlightCommand cmd;
   cmd.headingDeg = 90.0;
   cmd.altitudeM = 2000.0;
   cmd.speedKts = 300.0;
   action.setCommand(cmd);
   action.setLabel("PATROL");

   EXPECT_FALSE(action.execute(bench.air))
      << "sem Autopilot a decisao nao pode ser atuada -- ver o comentario sobre a falha muda";

   // Sem atuacao nao ha por que o quadro de leitura mudar.
   const xboard::Readout r{xboard::get(bench.air->getID())};
   EXPECT_EQ(r.label, "--");
   EXPECT_EQ(r.decisions, 0);
}

TEST(FlightAction, ExecuteComAutopilotAplicaComandoEAtualizaQuadro)
{
   Bench bench(9006, /*withAutopilot=*/true);

   xA_4::FlightAction action1;
   domain::FlightCommand cmd1;
   cmd1.headingDeg = 180.0;
   cmd1.altitudeM = 2500.0;
   cmd1.speedKts = 350.0;
   action1.setCommand(cmd1);
   action1.setLabel("PATROL");

   EXPECT_TRUE(action1.execute(bench.air));
   EXPECT_TRUE(bench.autopilot->isHeadingHoldOn());
   EXPECT_TRUE(bench.autopilot->isAltitudeHoldOn());
   EXPECT_TRUE(bench.autopilot->isVelocityHoldOn());
   EXPECT_DOUBLE_EQ(bench.autopilot->getCommandedHeadingD(), 180.0);
   EXPECT_NEAR(bench.autopilot->getCommandedAltitudeFt(), 2500.0 * base::distance::M2FT, 1e-3);
   EXPECT_DOUBLE_EQ(bench.autopilot->getCommandedVelocityKts(), 350.0);

   xboard::Readout r{xboard::get(bench.air->getID())};
   EXPECT_EQ(r.label, "PATROL");
   EXPECT_EQ(r.decisions, 1);

   // Segunda decisao, rotulo diferente -- exercita a transicao (before.label
   // != label) e confere que 'decisions' acompanha a taxa de atuacao.
   xA_4::FlightAction action2;
   domain::FlightCommand cmd2;
   cmd2.headingDeg = 200.0;
   cmd2.altitudeM = 2600.0;
   cmd2.speedKts = 360.0;
   action2.setCommand(cmd2);
   action2.setLabel("EVADE");

   EXPECT_TRUE(action2.execute(bench.air));
   EXPECT_DOUBLE_EQ(bench.autopilot->getCommandedHeadingD(), 200.0);

   r = xboard::get(bench.air->getID());
   EXPECT_EQ(r.label, "EVADE");
   EXPECT_EQ(r.decisions, 2);
}

//------------------------------------------------------------------------------
// RadarScan -- leitura direta do Gimbal (sem simular nada).
//------------------------------------------------------------------------------
TEST(RadarScan, AirNuloNaoEncontraNada)
{
   const xA_4::RadarScanInfo info{xA_4::radarScanOf(nullptr)};
   EXPECT_FALSE(info.found);
}

TEST(RadarScan, AirSemGimbalChamadoRadarNaoEncontraNada)
{
   Bench bench(9007);
   const xA_4::RadarScanInfo info{xA_4::radarScanOf(bench.air)};
   EXPECT_FALSE(info.found);
}

//------------------------------------------------------------------------------
// AlertDatalink -- filtro de lado/alcance na RECEPCAO.
//
// test_xnative.cpp cobre a fusao comutativa/latencia de fase com uma
// AlertDatalink SOLTA (sem Player container) -- ali o filtro fica desligado
// de proposito (getOwnship() devolve nulo, ver o comentario da classe).
// Aqui ele fica LIGADO: precisa de um Player de verdade, dai morar neste
// arquivo (o que 'Bench' ja existe pra resolver) e nao em test_xnative.cpp.
//------------------------------------------------------------------------------
class SondaAlertDatalink : public xA_4::AlertDatalink
{
public:
   using AlertDatalink::onDatalinkMessageEvent;
   using AlertDatalink::receive;
};

// Mesma ideia do 'Bench' acima, com uma AlertDatalink de verdade acoplada --
// side/posicao sao os dois dados que o filtro novo le via getOwnship().
struct AlertBench
{
   models::WorldModel* const world;
   models::AirVehicle* const air;
   SondaAlertDatalink* const datalink;

   AlertBench(const models::Player::Side side, const double northM, const double eastM, const double altM)
      : world(new models::WorldModel()), air(new models::AirVehicle()), datalink(new SondaAlertDatalink())
   {
      air->container(world);
      air->setSide(side);

      const auto pair = new base::Pair("datalink", datalink);
      datalink->unref();   // Pair::Pair() ja deu ref() -- devolve a nossa
      air->addComponent(pair);
      pair->unref();        // addComponent() ja deu ref() -- devolve a nossa

      air->reset();   // useCoordSys CS_NONE -> CS_LOCAL (ver Bench acima)
      air->setPosition(northM, eastM);
      air->setAltitude(altM);
   }

   ~AlertBench()
   {
      air->unref();
      world->unref();
   }
};

events::TacticalAlert* makeAlert(const unsigned int senderSide, const double senderNorthM,
                                  const double senderEastM, const double senderAltM)
{
   const auto alert = new events::TacticalAlert();
   alert->setSender(99, "intruso");
   alert->setContactName("bandit1");
   alert->setRangeM(5000.0);
   alert->setSenderSide(senderSide);
   alert->setSenderPosition(senderNorthM, senderEastM, senderAltM);
   return alert;
}

TEST(AlertDatalink, RejeitaAlertaDeLadoDiferente)
{
   AlertBench bench(models::Player::BLUE, 0.0, 0.0, 2000.0);

   auto* const alerta = makeAlert(models::Player::RED, 100.0, 0.0, 2000.0);
   bench.datalink->onDatalinkMessageEvent(alerta);
   bench.datalink->receive(0.02);

   EXPECT_FALSE(bench.datalink->hasAlert()) << "aceitou alerta de lado diferente";
   EXPECT_EQ(bench.datalink->getReceivedCount(), 0) << "contou como recebido mesmo rejeitado";

   alerta->unref();
}

TEST(AlertDatalink, RejeitaAlertaAlemDoAlcance)
{
   AlertBench bench(models::Player::BLUE, 0.0, 0.0, 2000.0);
   ASSERT_TRUE(bench.datalink->setMaxRange(5.0));   // 5 NM ~ 9260 m

   // 20 km ao norte -- muito alem dos 5 NM configurados.
   auto* const alerta = makeAlert(models::Player::BLUE, 20000.0, 0.0, 2000.0);
   bench.datalink->onDatalinkMessageEvent(alerta);
   bench.datalink->receive(0.02);

   EXPECT_FALSE(bench.datalink->hasAlert()) << "aceitou alerta alem do alcance do datalink";

   alerta->unref();
}

TEST(AlertDatalink, AceitaAlertaDoMesmoLadoDentroDoAlcance)
{
   AlertBench bench(models::Player::BLUE, 0.0, 0.0, 2000.0);
   ASSERT_TRUE(bench.datalink->setMaxRange(50.0));   // 50 NM ~ 92600 m

   // 9260 m ao leste -- dentro dos 50 NM configurados.
   auto* const alerta = makeAlert(models::Player::BLUE, 0.0, 9260.0, 2000.0);
   bench.datalink->onDatalinkMessageEvent(alerta);
   bench.datalink->receive(0.02);

   ASSERT_TRUE(bench.datalink->hasAlert()) << "rejeitou alerta legitimo, do mesmo lado e dentro do alcance";
   EXPECT_EQ(bench.datalink->getReceivedCount(), 1);

   alerta->unref();
}

} // namespace
