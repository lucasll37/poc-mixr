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
#include "xnative/RadarScan.hpp"

#include "domain/FlightCommand.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/Autopilot.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/units/Distances.hpp"

#include <gtest/gtest.h>

namespace {

using namespace mixr;

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
   xnative::FlightState state;
   base::Component naoEhAviao;
   state.updateState(&naoEhAviao);
   EXPECT_FALSE(state.snapshot().valid);
}

TEST(FlightState, PopulaCamposBasicosDeUmAirVehicleReal)
{
   Bench bench(9001);
   bench.air->setAltitude(1800.0);
   bench.air->setTerrainElevation(1200.0);

   xnative::FlightState state;
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
   xnative::AltitudeSafetyBehavior behavior;
   EXPECT_EQ(behavior.genAction(nullptr, 0.02), nullptr);
}

TEST(AltitudeSafetyBehavior, AcimaDoPisoNaoGeraAcao)
{
   Bench bench(9002);
   bench.air->setAltitude(2000.0);   // acima do minAltitude default (1500 m)

   xnative::FlightState state;
   state.updateState(bench.air);

   xnative::AltitudeSafetyBehavior behavior;
   EXPECT_EQ(behavior.genAction(&state, 0.02), nullptr);
}

TEST(AltitudeSafetyBehavior, AbaixoDoPisoAbsolutoComandaRecuperacaoMantendoRumo)
{
   Bench bench(9003, /*withAutopilot=*/true);
   bench.air->setAltitude(500.0);   // abaixo do minAltitude default (1500 m)

   xnative::FlightState state;
   state.updateState(bench.air);
   ASSERT_TRUE(state.snapshot().valid);

   xnative::AltitudeSafetyBehavior behavior;
   const auto action = dynamic_cast<xnative::FlightAction*>(behavior.genAction(&state, 0.02));
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

   xnative::FlightState state;
   state.updateState(bench.air);
   ASSERT_TRUE(state.snapshot().terrainValid);
   EXPECT_NEAR(state.snapshot().altitudeAglM, 50.0, 1e-6);

   xnative::AltitudeSafetyBehavior behavior;

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

   const auto action = dynamic_cast<xnative::FlightAction*>(behavior.genAction(&state, 0.02));
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
   xnative::FlightAction action;
   base::Component naoEhPlayer;
   EXPECT_FALSE(action.execute(&naoEhPlayer));
}

TEST(FlightAction, ExecuteSemAutopilotDevolveFalseESeguraOComando)
{
   Bench bench(9005, /*withAutopilot=*/false);

   xnative::FlightAction action;
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

   xnative::FlightAction action1;
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
   xnative::FlightAction action2;
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
   const xnative::RadarScanInfo info{xnative::radarScanOf(nullptr)};
   EXPECT_FALSE(info.found);
}

TEST(RadarScan, AirSemGimbalChamadoRadarNaoEncontraNada)
{
   Bench bench(9007);
   const xnative::RadarScanInfo info{xnative::radarScanOf(bench.air)};
   EXPECT_FALSE(info.found);
}

} // namespace
