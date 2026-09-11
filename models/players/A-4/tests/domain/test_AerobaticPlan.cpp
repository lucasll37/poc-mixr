// domain::AerobaticPlan -- QUANDO rolar, e por quanto tempo manter a manobra.
//
// Regra pura: sem MIXR, sem BehaviorTree.CPP, sem Station. A semente entra
// pronta (a hierarquia de derivacao que a produz e' de BtBehavior::
// configurePlans(), testada a parte em tests/domain/test_xrandom.cpp do host).

#include "domain/AerobaticPlan.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

constexpr double kDt{0.02};   // 50 Hz, o passo de decisao de producao

// Avanca o plano com a aeronave em asas niveladas (banco constante): serve
// para testar o RELOGIO sem que o giro progrida.
double avancarSemRolar(domain::AerobaticPlan& plan, const int passos)
{
   double t{};
   for (int i = 0; i < passos; ++i) { plan.update(kDt, 0.0); t += kDt; }
   return t;
}

// Roda ate a manobra COMECAR, devolvendo o tempo decorrido. Devolve -1 se nao
// comecar dentro do limite.
double tempoAteComecar(domain::AerobaticPlan& plan, const double limiteSec)
{
   double t{};
   while (t < limiteSec) {
      plan.update(kDt, 0.0);
      t += kDt;
      if (plan.rolling()) return t;
   }
   return -1.0;
}

// Executa um giro completo alimentando o banco como a aeronave o reportaria:
// (-180, 180], girando a 'taxaDps'. Devolve quantos segundos a manobra durou.
double rolarAteTerminar(domain::AerobaticPlan& plan, const double taxaDps,
                        const double limiteSec)
{
   double t{};
   double banco{};
   while (plan.rolling() && t < limiteSec) {
      banco += taxaDps * kDt;
      // wrap para (-180, 180], que e' como Player::getRollD() reporta
      while (banco > 180.0) banco -= 360.0;
      while (banco <= -180.0) banco += 360.0;
      plan.update(kDt, banco);
      t += kDt;
   }
   return t;
}

domain::AerobaticPlan fazerPlano(const double minSec, const double maxSec,
                                 const std::uint64_t semente,
                                 const double stick = 1.0,
                                 const double timeoutSec = 20.0)
{
   domain::AerobaticPlan plan;
   plan.configure(minSec, maxSec, stick, timeoutSec);
   plan.setSeed(semente);
   return plan;
}

//------------------------------------------------------------------------------
// O recurso nasce DESLIGADO -- e' o que mantem os cenarios existentes com
// dump byte-identico ao de antes desta classe existir.
//------------------------------------------------------------------------------
TEST(AerobaticPlan, StickZeroNuncaSaiDeIdle)
{
   auto plan = fazerPlano(1.0, 1.0, 42, 0.0);
   avancarSemRolar(plan, 5000);   // 100 s, muito alem do intervalo
   EXPECT_FALSE(plan.rolling());
   EXPECT_DOUBLE_EQ(plan.stick(), 0.0);
}

TEST(AerobaticPlan, NaoDisparaAntesDoIntervaloMinimo)
{
   auto plan = fazerPlano(30.0, 60.0, 7);
   avancarSemRolar(plan, 1400);   // 28 s
   EXPECT_FALSE(plan.rolling());
}

TEST(AerobaticPlan, DisparaDentroDaFaixaConfigurada)
{
   auto plan = fazerPlano(30.0, 60.0, 7);
   const double t{tempoAteComecar(plan, 120.0)};
   ASSERT_GT(t, 0.0) << "a manobra nunca comecou";
   EXPECT_GE(t, 30.0);
   EXPECT_LE(t, 60.0 + kDt);
}

//------------------------------------------------------------------------------
// safeToRoll -- a borda de altitude (ver bt_nodes::DecisionContext::
// hasAerobaticAltitudeMargin()). AerobaticPlan nao sabe de terreno nenhum;
// so' obedece o booleano que o chamador ja calculou. So' pesa na borda
// Idle->Rolling: um sorteio vencido sem 'safeToRoll' fica ADIADO -- o
// relogio trava em zero e a proxima chamada tenta de novo, sem redesenhar o
// intervalo.
//------------------------------------------------------------------------------
TEST(AerobaticPlan, SemSafeToRollAManobraFicaAdiada)
{
   auto plan = fazerPlano(0.0, 0.0, 1);   // sorteio ja vencido no 1o update()
   for (int i = 0; i < 500; ++i) {        // 10 s -- bem alem de um unico tick
      plan.update(kDt, 0.0, false);
      ASSERT_FALSE(plan.rolling()) << "rolou sem safeToRoll, no passo " << i;
   }
   EXPECT_DOUBLE_EQ(plan.timeToNextSec(), 0.0) << "o relogio nao deveria ficar negativo nem redesenhar";
}

TEST(AerobaticPlan, ComecaNoTickEmQueSafeToRollViraTrue)
{
   auto plan = fazerPlano(0.0, 0.0, 1);
   for (int i = 0; i < 100; ++i) plan.update(kDt, 0.0, false);
   ASSERT_FALSE(plan.rolling());

   plan.update(kDt, 0.0, true);
   EXPECT_TRUE(plan.rolling()) << "nao comecou assim que a margem apareceu";
}

TEST(AerobaticPlan, SafeToRollNaoAfetaManobraJaEmCurso)
{
   auto plan = fazerPlano(0.0, 0.0, 1);
   plan.update(kDt, 0.0, true);
   ASSERT_TRUE(plan.rolling());

   // 'safeToRoll=false' NAO aborta uma manobra ja em curso -- so' a borda
   // Idle->Rolling consulta o parametro.
   double banco{};
   for (int i = 0; i < 400 && plan.rolling(); ++i) {
      banco += 90.0 * kDt;
      while (banco > 180.0) banco -= 360.0;
      plan.update(kDt, banco, false);
   }

   EXPECT_FALSE(plan.rolling()) << "a manobra deveria ter fechado os 360 graus";
   EXPECT_GE(std::abs(plan.accumulatedRollDeg()), 360.0);
}

//------------------------------------------------------------------------------
// O giro fecha, e o comando volta a zero. O 'stick' importa porque o comando
// e' PEGAJOSO do lado do JSBSim -- quem atua precisa do zero explicito.
//------------------------------------------------------------------------------
TEST(AerobaticPlan, FechaOsTrezentosESessentaGrausEVoltaAIdle)
{
   auto plan = fazerPlano(1.0, 1.0, 1);
   ASSERT_GT(tempoAteComecar(plan, 5.0), 0.0);
   EXPECT_DOUBLE_EQ(plan.stick(), 1.0);

   const double duracao{rolarAteTerminar(plan, 90.0, 30.0)};   // 90 deg/s -> ~4 s

   EXPECT_FALSE(plan.rolling());
   EXPECT_DOUBLE_EQ(plan.stick(), 0.0);
   EXPECT_NEAR(duracao, 4.0, 0.2);
   EXPECT_GE(std::abs(plan.accumulatedRollDeg()), 360.0);
}

//------------------------------------------------------------------------------
// O acumulador integra a DIFERENCA passada por wrap180 -- comparar o banco
// absoluto contra 360 nunca dispararia, porque ele vive em (-180, 180] e
// cruza essa descontinuidade DUAS vezes por giro. Este teste falha se alguem
// trocar a integracao por comparacao direta.
//------------------------------------------------------------------------------
TEST(AerobaticPlan, AcumuladorAtravessaODescontinuoDeCentoEOitenta)
{
   auto plan = fazerPlano(0.0, 0.0, 1);
   plan.update(kDt, 0.0);
   ASSERT_TRUE(plan.rolling());

   // Sobe ate perto de +180, cruza para -180 e continua ate fechar o giro.
   rolarAteTerminar(plan, 90.0, 30.0);
   EXPECT_FALSE(plan.rolling()) << "o giro nao fechou -- o wrap comeu rolagem";
   EXPECT_NEAR(std::abs(plan.accumulatedRollDeg()), 360.0, 5.0);
}

TEST(AerobaticPlan, RolaParaAEsquerdaComStickNegativo)
{
   auto plan = fazerPlano(0.0, 0.0, 1, -1.0);
   plan.update(kDt, 0.0);
   ASSERT_TRUE(plan.rolling());
   EXPECT_DOUBLE_EQ(plan.stick(), -1.0);

   rolarAteTerminar(plan, -90.0, 30.0);
   EXPECT_FALSE(plan.rolling());
   EXPECT_LE(plan.accumulatedRollDeg(), -360.0);
}

//------------------------------------------------------------------------------
// A guarda de tempo. Sem ela, uma aeronave sem autoridade de rolagem ficaria
// presa em manobra para SEMPRE, sem erro nenhum -- que e' exatamente o que
// aconteceria com o nivelador de asas do a4ap.xml nao gateado.
//------------------------------------------------------------------------------
TEST(AerobaticPlan, TimeoutAbortaAManobraQueNaoFecha)
{
   auto plan = fazerPlano(0.0, 0.0, 1, 1.0, 5.0);
   plan.update(kDt, 0.0);
   ASSERT_TRUE(plan.rolling());

   // Banco TRAVADO: e' o sintoma de "sem autoridade". O giro nunca progride.
   const double duracao{rolarAteTerminar(plan, 0.0, 30.0)};

   EXPECT_FALSE(plan.rolling()) << "a manobra ficou presa -- o timeout nao pegou";
   EXPECT_NEAR(duracao, 5.0, 0.2);
   EXPECT_LT(std::abs(plan.accumulatedRollDeg()), 360.0);
}

//------------------------------------------------------------------------------
// Reprodutibilidade -- a propriedade que libs/xrandom existe para garantir.
//------------------------------------------------------------------------------
std::vector<double> primeirosIntervalos(const std::uint64_t semente, const int quantos)
{
   auto plan = fazerPlano(20.0, 80.0, semente);
   std::vector<double> saida;
   for (int i = 0; i < quantos; ++i) {
      const double t{tempoAteComecar(plan, 200.0)};
      saida.push_back(t);
      rolarAteTerminar(plan, 180.0, 30.0);
   }
   return saida;
}

TEST(AerobaticPlan, MesmaSementeDaSequenciaIdentica)
{
   EXPECT_EQ(primeirosIntervalos(20260910, 4), primeirosIntervalos(20260910, 4));
}

TEST(AerobaticPlan, SementesDiferentesDaoSequenciasDiferentes)
{
   EXPECT_NE(primeirosIntervalos(20260910, 4), primeirosIntervalos(1, 4));
}

//------------------------------------------------------------------------------
// max <= min vira intervalo FIXO, sem consumir o gerador -- o modo
// deterministico "sem sorteio", util para um cenario de teste.
//------------------------------------------------------------------------------
TEST(AerobaticPlan, IntervaloFixoNaoDependeDaSemente)
{
   auto a = fazerPlano(10.0, 10.0, 1);
   auto b = fazerPlano(10.0, 10.0, 999);
   const double ta{tempoAteComecar(a, 30.0)};
   const double tb{tempoAteComecar(b, 30.0)};
   EXPECT_NEAR(ta, 10.0, kDt);
   EXPECT_DOUBLE_EQ(ta, tb);
}

TEST(AerobaticPlan, MaxMenorQueMinNaoQuebra)
{
   auto plan = fazerPlano(10.0, 2.0, 1);   // invertido de proposito
   const double t{tempoAteComecar(plan, 30.0)};
   ASSERT_GT(t, 0.0);
   EXPECT_NEAR(t, 10.0, kDt);   // colapsa para o piso, nao para uma faixa invalida
}

//------------------------------------------------------------------------------
// dt grande nao deixa o relogio negativo -- mesma correcao que
// PatrolPlan::advance() ja precisou.
//------------------------------------------------------------------------------
TEST(AerobaticPlan, PassoGrandeNaoDeixaORelogioNegativo)
{
   auto plan = fazerPlano(10.0, 10.0, 1);
   plan.update(300.0, 0.0);   // um dt que cobre 30 intervalos
   EXPECT_TRUE(plan.rolling());
   EXPECT_GE(plan.timeToNextSec(), 0.0);
}

//------------------------------------------------------------------------------
// O stick e' clampado na faixa que Autopilot::setControlStickRollInput()
// aceita: fora de [-1, 1] ele RECUSA o valor em silencio e mantem o anterior.
//------------------------------------------------------------------------------
TEST(AerobaticPlan, StickEhClampadoNaFaixaDoAutopilot)
{
   auto plan = fazerPlano(0.0, 0.0, 1, 3.5);
   plan.update(kDt, 0.0);
   ASSERT_TRUE(plan.rolling());
   EXPECT_DOUBLE_EQ(plan.stick(), 1.0);
}

} // namespace
