//
// shared/xrlbridge -- a ponte de comando/observacao entre o host de RL e o
// modelo, na camada mais isolada possivel: sem Station, sem plugin, sem
// pybind11. So a lib.
//
// Nao havia NENHUM teste automatizado direto para este modulo antes deste
// arquivo -- so cobertura indireta via test_xinfer.cpp (que usa apenas a
// constante XRLBRIDGE_OBSERVATION_SIZE) e os testes de ponta a ponta
// scenario-policy-onnx/test-rl. O contrato de dados (ordem dos 28 campos,
// a desnormalizacao de acao, a contagem de campos booleanos) e usado por
// TRES consumidores (RLBridgeBehavior, OnnxPolicyAction, os bindings
// pybind11) e nunca tinha uma trava propria -- so a static_assert em
// RLBridge.cpp, que confere CONTAGEM, nao FORMULA.
//
#include "xrlbridge/ObservationFields.hpp"
#include "xrlbridge/RLBridge.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <set>
#include <thread>
#include <vector>

namespace {

using namespace mixr::xrlbridge;

//------------------------------------------------------------------------------
// O CONTRATO DE NOMES/CONTAGEM -- observationFieldNames()/observationBoolFields()
// sao a mesma macro que domain::WorldView e Observation expandem para virar
// dado; aqui e so a lista, sem struct nenhuma por tras.
//------------------------------------------------------------------------------

TEST(XRLBridge, ObservationFieldNamesTemATamanhoCanonico)
{
   const auto nomes = observationFieldNames();
   EXPECT_EQ(static_cast<int>(nomes.size()), XRLBRIDGE_OBSERVATION_SIZE);
}

TEST(XRLBridge, ObservationFieldNamesNaoTemDuplicata)
{
   const auto nomes = observationFieldNames();
   const std::set<std::string> unicos(nomes.begin(), nomes.end());
   EXPECT_EQ(unicos.size(), nomes.size())
      << "um nome de campo repetido faria dois floats do tensor apontarem"
      << " para o mesmo significado, em silencio";
}

// Os 5 booleanos sao SEMPRE os ultimos 5 -- e o que RLBridgeBehavior/
// OnnxPolicyAction/o env.py assumem ao fatiar a lista.
TEST(XRLBridge, BoolFieldsSaoOsUltimosCincoDaListaCompleta)
{
   const auto nomes = observationFieldNames();
   const auto bools = observationBoolFields();
   ASSERT_EQ(bools.size(), 5U);
   ASSERT_GE(nomes.size(), bools.size());

   const auto inicioDosBool = nomes.end() - static_cast<long>(bools.size());
   EXPECT_TRUE(std::equal(inicioDosBool, nomes.end(), bools.begin()))
      << "os campos booleanos deixaram de ser os ultimos da lista canonica";
}

TEST(XRLBridge, BoolFieldsSaoOsNomesDocumentados)
{
   const auto bools = observationBoolFields();
   const std::vector<std::string> esperado{
      "valid", "terrainValid", "hasContact", "hasAlert", "weaponReady"};
   EXPECT_EQ(bools, esperado);
}

//------------------------------------------------------------------------------
// packObservation() -- Observation -> vetor de floats na ordem canonica.
// Testado campo a campo (nao so a contagem) porque um campo fora de ordem
// aqui nao quebra a compilacao (ao contrario de um nome errado): o .onnx
// receberia numeros trocados e voaria errado, em silencio.
//------------------------------------------------------------------------------

TEST(XRLBridge, PackObservationRespeitaAOrdemCanonica)
{
   Observation obs;
   obs.northM = 1.0;
   obs.eastM = 2.0;
   obs.altitudeM = 3.0;
   obs.headingDeg = 4.0;
   obs.speedKts = 5.0;
   obs.rollDeg = 6.0;
   obs.pitchDeg = 7.0;
   obs.fuelFraction = 8.0;
   obs.mach = 9.0;
   obs.gLoad = 10.0;
   obs.alphaDeg = 11.0;
   obs.terrainElevM = 12.0;
   obs.altitudeAglM = 13.0;
   obs.contactRangeM = 14.0;
   obs.contactRelBearingDeg = 15.0;
   obs.contactDeltaAltM = 16.0;
   obs.contactNorthM = 17.0;
   obs.contactEastM = 18.0;
   obs.contactAltitudeM = 19.0;
   obs.alertNorthM = 20.0;
   obs.alertEastM = 21.0;
   obs.alertAltitudeM = 22.0;
   obs.alertRangeM = 23.0;
   obs.valid = true;
   obs.terrainValid = false;
   obs.hasContact = true;
   obs.hasAlert = false;
   obs.weaponReady = true;

   std::array<float, XRLBRIDGE_OBSERVATION_SIZE> saida{};
   packObservation(obs, saida.data());

   // Os 23 floats na ordem em que ObservationFields.hpp os lista.
   for (int i = 0; i < 23; ++i) {
      EXPECT_FLOAT_EQ(saida[static_cast<std::size_t>(i)], static_cast<float>(i + 1))
         << "campo numerico na posicao " << i << " nao bate com a ordem canonica";
   }
   // Os 5 bools, na ordem: valid, terrainValid, hasContact, hasAlert, weaponReady.
   EXPECT_FLOAT_EQ(saida[23], 1.0F);
   EXPECT_FLOAT_EQ(saida[24], 0.0F);
   EXPECT_FLOAT_EQ(saida[25], 1.0F);
   EXPECT_FLOAT_EQ(saida[26], 0.0F);
   EXPECT_FLOAT_EQ(saida[27], 1.0F);
}

TEST(XRLBridge, PackObservationComPonteiroNuloNaoAborta)
{
   const Observation obs;
   packObservation(obs, nullptr);   // nao deve travar
   SUCCEED();
}

//------------------------------------------------------------------------------
// unscaleCommand() -- [-1,1] normalizado (saida de um .onnx exportado do
// SB3) -> unidades fisicas. A formula e t=(n+1)/2, cmd=low+t*(high-low); e
// testada nos extremos, no centro, E no recorte de saturacao (a politica nao
// pode comandar fora da faixa so porque a rede saiu de escala).
//------------------------------------------------------------------------------

TEST(XRLBridge, UnscaleCommandNosExtremosENoCentro)
{
   // heading: [0,360]; altitude: [0,8000]; speed: [0,400].
   {
      const std::array<float, 3> n{-1.0F, -1.0F, -1.0F};
      const Command c{unscaleCommand(n.data())};
      EXPECT_DOUBLE_EQ(c.headingDeg, 0.0);
      EXPECT_DOUBLE_EQ(c.altitudeM, 0.0);
      EXPECT_DOUBLE_EQ(c.speedKts, 0.0);
   }
   {
      const std::array<float, 3> n{1.0F, 1.0F, 1.0F};
      const Command c{unscaleCommand(n.data())};
      EXPECT_DOUBLE_EQ(c.headingDeg, 360.0);
      EXPECT_DOUBLE_EQ(c.altitudeM, 8000.0);
      EXPECT_DOUBLE_EQ(c.speedKts, 400.0);
   }
   {
      const std::array<float, 3> n{0.0F, 0.0F, 0.0F};
      const Command c{unscaleCommand(n.data())};
      EXPECT_DOUBLE_EQ(c.headingDeg, 180.0);
      EXPECT_DOUBLE_EQ(c.altitudeM, 4000.0);
      EXPECT_DOUBLE_EQ(c.speedKts, 200.0);
   }
}

// Fora de [-1,1]: o RECORTE acontece ANTES da desnormalizacao (ver o
// comentario de RLBridge.cpp) -- uma rede fora de escala nao pode virar um
// comando fisicamente absurdo.
TEST(XRLBridge, UnscaleCommandRecortaForaDeMenosUmAUm)
{
   const std::array<float, 3> alemDoLimite{5.0F, -7.5F, 100.0F};
   const Command c{unscaleCommand(alemDoLimite.data())};
   EXPECT_DOUBLE_EQ(c.headingDeg, 360.0)   << "n=5.0 tem de saturar em +1.0 antes de escalar";
   EXPECT_DOUBLE_EQ(c.altitudeM, 0.0)      << "n=-7.5 tem de saturar em -1.0 antes de escalar";
   EXPECT_DOUBLE_EQ(c.speedKts, 400.0)     << "n=100.0 tem de saturar em +1.0 antes de escalar";
}

TEST(XRLBridge, UnscaleCommandComPonteiroNuloDevolveComandoZerado)
{
   const Command c{unscaleCommand(nullptr)};
   EXPECT_DOUBLE_EQ(c.headingDeg, 0.0);
   EXPECT_DOUBLE_EQ(c.altitudeM, 0.0);
   EXPECT_DOUBLE_EQ(c.speedKts, 0.0);
}

//------------------------------------------------------------------------------
// setPendingCommand/getPendingCommand e setObservation/getObservation -- o
// unico mutex protegendo os dois campos globais (ver o "porque" no cabecalho
// de RLBridge.hpp: v1 e um unico agente RL por processo, sem chave). Nunca
// testado sob concorrencia real antes deste arquivo.
//------------------------------------------------------------------------------

TEST(XRLBridge, CommandEObservationFazemRoundtrip)
{
   Command cmd;
   cmd.headingDeg = 90.0;
   cmd.altitudeM = 1500.0;
   cmd.speedKts = 250.0;
   setPendingCommand(cmd);
   const Command lido{getPendingCommand()};
   EXPECT_DOUBLE_EQ(lido.headingDeg, 90.0);
   EXPECT_DOUBLE_EQ(lido.altitudeM, 1500.0);
   EXPECT_DOUBLE_EQ(lido.speedKts, 250.0);

   Observation obs;
   obs.valid = true;
   obs.northM = 123.0;
   obs.contactName = "bandit1";
   setObservation(obs);
   const Observation lida{getObservation()};
   EXPECT_TRUE(lida.valid);
   EXPECT_DOUBLE_EQ(lida.northM, 123.0);
   EXPECT_EQ(lida.contactName, "bandit1");
}

// Escritor unico (o host) e leitor unico (o modelo) de cada lado, mas os
// DOIS lados leem/escrevem em threads diferentes -- o mutex tem de bastar
// para nao entregar um struct parcialmente escrito (um northM de uma escrita
// com o eastM de outra).
TEST(XRLBridge, EscritasConcorrentesNuncaEntregamStructMisturado)
{
   std::atomic<bool> parar{false};
   std::atomic<int> inconsistencias{};

   // g_observation e um global do PROCESSO (shared_library, sem reset entre
   // TEST()s) -- um teste anterior pode deixar northM/eastM/altitudeM
   // desiguais (ex.: CommandEObservationFazemRoundtrip so mexe em northM).
   // Sem esta priming, o loop de leitura abaixo pode comecar a ler ANTES da
   // primeira escrita da thread 'escritor' e contar esse estado herdado como
   // "struct misturado" -- um falso positivo de teste, nao um bug de
   // concorrencia de verdade. Uma escrita consistente aqui, ainda em serie,
   // estabelece a invariante que o loop concorrente vai checar.
   Observation baseline;
   baseline.northM = 0.0;
   baseline.eastM = 0.0;
   baseline.altitudeM = 0.0;
   setObservation(baseline);

   std::thread escritor([&] {
      double v{1.0};
      while (!parar.load()) {
         Observation obs;
         obs.northM = v;
         obs.eastM = v;
         obs.altitudeM = v;
         setObservation(obs);
         v += 1.0;
      }
   });

   for (int i = 0; i < 20000; ++i) {
      const Observation lida{getObservation()};
      if (lida.northM != lida.eastM || lida.eastM != lida.altitudeM) {
         inconsistencias.fetch_add(1);
      }
   }
   parar.store(true);
   escritor.join();

   EXPECT_EQ(inconsistencias.load(), 0)
      << "getObservation() devolveu um struct com campos de escritas diferentes misturados";
}

} // namespace
