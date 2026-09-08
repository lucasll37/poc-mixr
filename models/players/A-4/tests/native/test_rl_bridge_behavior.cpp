//
// RLBridgeBehavior -- camada 3 (classes MIXR proprias, SEM levantar Station).
//
// O que ESTE teste cobre: o contrato de genAction() com um estado invalido
// (dynamic_cast falha -> nullptr, sem tocar libs/xrlbridge) e o
// round-trip completo de libs/xrlbridge -- setPendingCommand()/
// getPendingCommand() como par leitura/escrita simples, e setObservation()/
// getObservation() idem.
//
// O que ESTE teste NAO cobre: genAction() publicando uma Observation de
// verdade (snap.valid == true) e consumindo um Command de xrlbridge dentro
// do MESMO ciclo, porque isso exige um models::AirVehicle real por tras de
// FlightState::updateState() -- a mesma linha que separa esta camada da
// camada 'scenario' (ver o cabecalho de test_xnative.cpp). FlightState::snap
// e' privado, sem setter -- nao ha seam de injecao sem um AirVehicle de
// verdade nem sem tocar a classe so' pra testabilidade.
//
// DEBITO DE TESTE RECONHECIDO (autorevisao desta sessao, nao redescobrir):
// isso inclui especificamente o comportamento de
// pending.valid==false -> genAction() devolve nullptr (ver o comentario
// grande em bt/DecisionContext.hpp/xrlbridge::Command) -- so' foi
// confirmado MANUALMENTE nesta sessao (make test-rl, lendo a linha de log
// da primeira decisao antes/depois do fix), nunca por uma asserção
// automatizada. src/rl/tests/test_smoke.py "exercita" o caminho (roda de
// verdade contra a Station), mas nao AFIRMA nada sobre os valores da
// primeira decisao -- uma regressao futura aqui (ex.: inverter a
// condicao, ou esquecer o setPendingCommand(Command{}) em reset())
// passaria por make test/make test-rl sem ser detectada. Registrado como
// divida conhecida, nao "coberto".
//
#include "ubf/RLBridgeBehavior.hpp"

#include "xrlbridge/RLBridge.hpp"

#include <gtest/gtest.h>

namespace {

using namespace mixr;
using namespace mixr::models;

TEST(RLBridgeBehavior, GenActionComEstadoNuloDevolveNulo)
{
   auto* const bridge = new xnative::RLBridgeBehavior();

   // dynamic_cast<const FlightState*>(nullptr) e nullptr -- genAction() tem
   // de recusar sem crashar, o mesmo contrato de BtBehavior/
   // AltitudeSafetyBehavior para um AbstractState de tipo errado.
   EXPECT_EQ(bridge->genAction(nullptr, 0.02), nullptr);

   bridge->unref();
}

TEST(RLBridge, ComandoPendenteEDevolvidoPorGetPendingCommand)
{
   xrlbridge::Command cmd;
   cmd.valid = true;
   cmd.headingDeg = 90.0;
   cmd.altitudeM = 1750.0;
   cmd.speedKts = 160.0;
   xrlbridge::setPendingCommand(cmd);

   const xrlbridge::Command got{xrlbridge::getPendingCommand()};
   EXPECT_TRUE(got.valid);
   EXPECT_DOUBLE_EQ(got.headingDeg, 90.0);
   EXPECT_DOUBLE_EQ(got.altitudeM, 1750.0);
   EXPECT_DOUBLE_EQ(got.speedKts, 160.0);
}

// ACHADO POR AUDITORIA (nao redescobrir, ver o comentario grande em
// xrlbridge::Command): um Command default-construido (o que
// getPendingCommand() devolve antes do primeiro setPendingCommand() do
// episodio) tem de vir com valid=false -- e' o sinal que
// RLBridgeBehavior::genAction() usa pra devolver nullptr em vez de atuar
// heading=0/altitude=0/speed=0 como se fosse uma decisao de verdade.
TEST(RLBridge, ComandoDefaultConstruidoNaoEValido)
{
   const xrlbridge::Command cmd;
   EXPECT_FALSE(cmd.valid);
}

TEST(RLBridge, ObservacaoPublicadaEDevolvidaPorGetObservation)
{
   xrlbridge::Observation obs;
   obs.valid = true;
   obs.northM = 1234.5;
   obs.fuelFraction = 0.42;
   xrlbridge::setObservation(obs);

   const xrlbridge::Observation got{xrlbridge::getObservation()};
   EXPECT_TRUE(got.valid);
   EXPECT_DOUBLE_EQ(got.northM, 1234.5);
   EXPECT_DOUBLE_EQ(got.fuelFraction, 0.42);
}

} // namespace
