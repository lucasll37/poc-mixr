// Circuito de patrulha. A propriedade menos obvia -- e a que mais importa
// para o modelo -- e que o relogio da perna SO corre quando alguem chama
// advance(): e assim que a patrulha e retomada de onde parou depois de um
// RTB ou de uma evasao, em vez de reiniciar.

#include "domain/PatrolPlan.hpp"

#include <cmath>

#include <gtest/gtest.h>

namespace {

constexpr double TOL{1e-9};

domain::PatrolPlan quadrado(const double rumoInicial = 90.0)
{
   domain::PatrolPlan p;
   p.configure(rumoInicial, 60.0, 90.0, 1750.0, 160.0);
   return p;
}

TEST(PatrolPlan, ComecaNaPrimeiraPernaComOsValoresConfigurados)
{
   auto p{quadrado()};
   EXPECT_EQ(p.legIndex(), 0);
   EXPECT_NEAR(p.command().headingDeg, 90.0, TOL);
   EXPECT_NEAR(p.command().altitudeM, 1750.0, TOL);
   EXPECT_NEAR(p.command().speedKts, 160.0, TOL);
   EXPECT_NEAR(p.legTimeRemaining(), 60.0, TOL);
}

TEST(PatrolPlan, TrocaDePernaExatamenteNoFimDoTempo)
{
   auto p{quadrado()};
   for (int s = 1; s <= 59; ++s) {
      EXPECT_FALSE(p.advance(1.0)) << "trocou de perna cedo, em t=" << s;
      EXPECT_EQ(p.legIndex(), 0);
   }
   EXPECT_TRUE(p.advance(1.0)) << "a troca tinha de acontecer em t=60";
   EXPECT_EQ(p.legIndex(), 1);
   EXPECT_NEAR(p.command().headingDeg, 180.0, TOL);
}

TEST(PatrolPlan, QuatroPernasDeNoventaFecamOCircuito)
{
   auto p{quadrado(90.0)};
   const double rumos[]{180.0, 270.0, 0.0, 90.0};
   for (const double esperado : rumos) {
      for (int s = 0; s < 60; ++s) p.advance(1.0);
      EXPECT_NEAR(p.command().headingDeg, esperado, TOL);
   }
   EXPECT_EQ(p.legIndex(), 4);
}

// A regra de retomada: enquanto RTB ou evasao mandam, ninguem chama advance()
// e a perna nao envelhece.
TEST(PatrolPlan, SemAdvanceARodaNaoGira)
{
   auto p{quadrado()};
   for (int s = 0; s < 30; ++s) p.advance(1.0);
   const int perna{p.legIndex()};
   const double restante{p.legTimeRemaining()};

   // 500 "frames" em que a arvore escolheu outro ramo
   for (int s = 0; s < 500; ++s) { /* nada */ }

   EXPECT_EQ(p.legIndex(), perna);
   EXPECT_NEAR(p.legTimeRemaining(), restante, TOL);
   EXPECT_NEAR(p.command().headingDeg, 90.0, TOL);
}

TEST(PatrolPlan, ResetVoltaParaAPrimeiraPerna)
{
   auto p{quadrado()};
   for (int s = 0; s < 150; ++s) p.advance(1.0);
   ASSERT_EQ(p.legIndex(), 2);

   p.reset();
   EXPECT_EQ(p.legIndex(), 0);
   EXPECT_NEAR(p.command().headingDeg, 90.0, TOL);
}

TEST(PatrolPlan, TempoDePernaNaoPositivoCaiNoDefaultDeSessentaSegundos)
{
   domain::PatrolPlan p;
   p.configure(0.0, 0.0, 90.0, 1500.0, 120.0);
   for (int s = 0; s < 59; ++s) EXPECT_FALSE(p.advance(1.0));
   EXPECT_TRUE(p.advance(1.0));
}

// Um passo maior que a perna nao pode "pular" a troca nem perder o resto.
TEST(PatrolPlan, PassoGrandeTrocaUmaVezEGuardaOResto)
{
   auto p{quadrado()};
   EXPECT_TRUE(p.advance(75.0));
   EXPECT_EQ(p.legIndex(), 1);
   EXPECT_NEAR(p.legTimeRemaining(), 45.0, TOL) << "o excedente tem de contar na perna nova";
}

//------------------------------------------------------------------------------
// Jitter de rumo -- desligado por padrao (amplitude 0), sorteado uma vez por
// troca de perna, nunca por dt (o que preservaria o determinismo entre 1/2/4
// threads: o numero de trocas de perna por player independe de threads).
//------------------------------------------------------------------------------

TEST(PatrolPlanJitter, AmplitudeZeroNaoMudaOCircuito)
{
   auto p{quadrado()};
   p.setHeadingJitter(0.0, 42);
   EXPECT_NEAR(p.command().headingDeg, 90.0, TOL);
   for (int leg = 0; leg < 4; ++leg) {
      for (int s = 0; s < 60; ++s) p.advance(1.0);
   }
   EXPECT_NEAR(p.command().headingDeg, 90.0, TOL);
}

TEST(PatrolPlanJitter, MesmaSementeReproduzAMesmaSequencia)
{
   auto a{quadrado()};
   auto b{quadrado()};
   a.setHeadingJitter(10.0, 123456789ULL);
   b.setHeadingJitter(10.0, 123456789ULL);

   for (int leg = 0; leg < 8; ++leg) {
      for (int s = 0; s < 60; ++s) { a.advance(1.0); b.advance(1.0); }
      EXPECT_NEAR(a.command().headingDeg, b.command().headingDeg, TOL)
         << "divergiu na perna " << leg;
   }
}

TEST(PatrolPlanJitter, SementesDiferentesDaoSequenciasDiferentes)
{
   auto a{quadrado()};
   auto b{quadrado()};
   a.setHeadingJitter(10.0, 1);
   b.setHeadingJitter(10.0, 2);

   bool divergiuEmAlgumaPerna{false};
   for (int leg = 0; leg < 8; ++leg) {
      for (int s = 0; s < 60; ++s) { a.advance(1.0); b.advance(1.0); }
      if (std::abs(a.command().headingDeg - b.command().headingDeg) > TOL) {
         divergiuEmAlgumaPerna = true;
      }
   }
   EXPECT_TRUE(divergiuEmAlgumaPerna);
}

TEST(PatrolPlanJitter, JitterNuncaExtrapolaAAmplitude)
{
   auto p{quadrado(0.0)};
   constexpr double amplitude{6.0};
   p.setHeadingJitter(amplitude, 999);

   for (int leg = 0; leg < 20; ++leg) {
      for (int s = 0; s < 60; ++s) p.advance(1.0);
      // rumo sem jitter seria sempre um multiplo de 90 (0/90/180/270);
      // a distancia angular ate o multiplo mais proximo nao pode passar
      // da amplitude, respeitando o wrap em 360.
      const double semJitter{std::fmod(90.0 * (leg + 1), 360.0)};
      double delta{p.command().headingDeg - semJitter};
      while (delta > 180.0) delta -= 360.0;
      while (delta < -180.0) delta += 360.0;
      EXPECT_LE(std::abs(delta), amplitude + TOL) << "estourou na perna " << leg;
   }
}

TEST(PatrolPlanJitter, ResetReproduzOMesmoPrimeiroJitter)
{
   auto p{quadrado()};
   p.setHeadingJitter(8.0, 77);
   const double primeiroRumo{p.command().headingDeg};

   for (int s = 0; s < 200; ++s) p.advance(1.0);
   ASSERT_NE(p.legIndex(), 0);

   p.reset();
   EXPECT_NEAR(p.command().headingDeg, primeiroRumo, TOL);
}

} // namespace
