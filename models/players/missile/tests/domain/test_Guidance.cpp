#include "domain/Guidance.hpp"

#include <cmath>

#include <gtest/gtest.h>

namespace mixr::models::xmissile::domain {
namespace {

constexpr double kEps{1.0e-9};

TEST(ProportionalNavigation, MiraDiretoNoAlvoParadoSemComponenteDeCorrecao)
{
   const Vec3 relPos{1000.0, 500.0, -200.0};
   const Vec3 relVel{0.0, 0.0, 0.0}; // alvo parado (relativo a nos): taxa da LOS e zero
   const GuidanceGains gains{4.0, 260.0};

   const auto cmd = proportionalNavigation(relPos, relVel, gains);

   const double range2d = std::sqrt(relPos.n * relPos.n + relPos.e * relPos.e);
   EXPECT_NEAR(cmd.cmdHeadingRad, std::atan2(relPos.e, relPos.n), kEps);
   EXPECT_NEAR(cmd.cmdPitchRad, std::atan2(-relPos.d, range2d), kEps);
}

TEST(ProportionalNavigation, ComandaSempreAVelocidadeDeCruzeiroDosGains)
{
   const GuidanceGains gains{4.0, 312.5};
   const auto cmd = proportionalNavigation(Vec3{5000.0, -1000.0, 0.0}, Vec3{10.0, -30.0, 5.0}, gains);
   EXPECT_DOUBLE_EQ(cmd.cmdSpeedMps, 312.5);
}

TEST(ProportionalNavigation, CorrigeAFrenteQuandoAlvoCruzaLateralmente)
{
   // Alvo a frente (norte), cruzando para leste: a LOS gira no sentido
   // positivo (azimute crescendo) -- a correcao proporcional tem que
   // adiantar o comando ALEM do angulo de perseguicao pura (0 rad aqui).
   const Vec3 relPos{1000.0, 0.0, 0.0};
   const Vec3 relVel{0.0, 100.0, 0.0};
   const GuidanceGains gains{4.0, 260.0};

   const auto cmd = proportionalNavigation(relPos, relVel, gains);

   const double pureHeading{std::atan2(relPos.e, relPos.n)}; // 0.0
   EXPECT_GT(cmd.cmdHeadingRad, pureHeading);
}

TEST(ProportionalNavigation, TaxaDaLosDegeneraParaZeroPertoDeAlcanceZero)
{
   // A menos de 1 m (kMinRangeForLosRateM), a formula da taxa da LOS
   // divide por um alcance quase zero -- a funcao guarda contra isso e
   // devolve so' a perseguicao pura, sem NaN/inf.
   const Vec3 relPos{0.3, 0.0, 0.0};
   const Vec3 relVel{0.0, 500.0, 0.0}; // taxa angular enorme se nao fosse guardado
   const GuidanceGains gains{4.0, 260.0};

   const auto cmd = proportionalNavigation(relPos, relVel, gains);

   EXPECT_TRUE(std::isfinite(cmd.cmdHeadingRad));
   EXPECT_NEAR(cmd.cmdHeadingRad, std::atan2(relPos.e, relPos.n), kEps);
}

TEST(ProportionalNavigation, ConvergeContraAlvoEmCruzamento)
{
   // Cenario de PN classico: alvo cruzando a 60 m/s a ~8,5 km, missil a
   // 280 m/s (mais rapido que o alvo). "Conformidade instantanea" (o
   // proprio comando vira o heading/pitch atual a cada passo) isola a LEI
   // de guiagem do limitador de taxa de giro -- esse limitador e' do
   // xnative::GuidedMissile, testado junto com o resto da dinamica no
   // cenario sandbox, nao aqui.
   //
   // Valores conferidos rodando esta MESMA funcao num programa standalone
   // antes de travar o limiar: alcance minimo medido ~198 m (de ~8544 m
   // inicial) -- o limiar abaixo tem folga generosa sobre o valor medido,
   // sem ficar frouxo a ponto de aceitar "nao convergiu".
   double ownN{0.0}, ownE{0.0}, ownD{0.0};
   double heading{0.0}, pitch{0.0}, speed{0.0};

   double tgtN{8000.0}, tgtE{3000.0}, tgtD{0.0};
   const double tgtVn{0.0}, tgtVe{-60.0}, tgtVd{0.0};

   const GuidanceGains gains{4.0, 280.0};
   const double dt{0.05};
   double minRangeM{1.0e9};

   for (int step = 0; step < 800; ++step) {
      const double ownVn{speed * std::cos(pitch) * std::cos(heading)};
      const double ownVe{speed * std::cos(pitch) * std::sin(heading)};
      const double ownVd{-speed * std::sin(pitch)};

      const Vec3 relPos{tgtN - ownN, tgtE - ownE, tgtD - ownD};
      const Vec3 relVel{tgtVn - ownVn, tgtVe - ownVe, tgtVd - ownVd};

      const double range{std::sqrt(relPos.n * relPos.n + relPos.e * relPos.e + relPos.d * relPos.d)};
      minRangeM = std::min(minRangeM, range);
      if (range < 5.0) break;

      const auto cmd = proportionalNavigation(relPos, relVel, gains);
      heading = cmd.cmdHeadingRad;
      pitch = cmd.cmdPitchRad;
      speed = cmd.cmdSpeedMps;

      ownN += ownVn * dt;
      ownE += ownVe * dt;
      ownD += ownVd * dt;
      tgtN += tgtVn * dt;
      tgtE += tgtVe * dt;
      tgtD += tgtVd * dt;
   }

   EXPECT_LT(minRangeM, 400.0);
}

TEST(ProximityFuze, NuncaDisparaEnquantoAproximando)
{
   FuzeState state{};
   const Vec3 relVel{-100.0, 0.0, 0.0};

   const auto o1 = proximityFuze(Vec3{1000.0, 0.0, 0.0}, relVel, 30.0, state);
   EXPECT_FALSE(o1.closestApproachReached);
   state = o1.nextState;

   const auto o2 = proximityFuze(Vec3{500.0, 0.0, 0.0}, relVel, 30.0, state);
   EXPECT_FALSE(o2.closestApproachReached);
}

TEST(ProximityFuze, DisparaNoFrameDeTransicaoComAcertoDireto)
{
   // Alvo passando exatamente pela posicao do missil (offset lateral 0),
   // fechando a 100 m/s: alcance chega a 0 na transicao -- acerto certo,
   // qualquer burstRangeM >= 0.
   FuzeState state{};
   const Vec3 relVel{-100.0, 0.0, 0.0};

   const auto before = proximityFuze(Vec3{100.0, 0.0, 0.0}, relVel, 30.0, state);
   EXPECT_FALSE(before.closestApproachReached);
   state = before.nextState;

   const auto atClosest = proximityFuze(Vec3{0.0, 0.0, 0.0}, relVel, 30.0, state);
   EXPECT_TRUE(atClosest.closestApproachReached);
   EXPECT_TRUE(atClosest.hit);
   EXPECT_NEAR(atClosest.rangeAtEventM, 0.0, kEps);
}

TEST(ProximityFuze, PerdeQuandoDistanciaDeMenorAproximacaoExcedeORaioDeExplosao)
{
   // Mesma geometria, mas com 50 m de offset lateral permanente -- o
   // alcance de menor aproximacao e' 50 m.
   FuzeState state{};
   const Vec3 relVel{-100.0, 0.0, 0.0};

   const auto before = proximityFuze(Vec3{100.0, 50.0, 0.0}, relVel, 30.0, state);
   state = before.nextState;

   const auto atClosest = proximityFuze(Vec3{0.0, 50.0, 0.0}, relVel, /*burstRangeM=*/30.0, state);
   EXPECT_TRUE(atClosest.closestApproachReached);
   EXPECT_FALSE(atClosest.hit);
   EXPECT_NEAR(atClosest.rangeAtEventM, 50.0, kEps);
}

TEST(ProximityFuze, AcertaQuandoDistanciaDeMenorAproximacaoCabeNoRaioDeExplosao)
{
   // Mesma geometria do teste anterior (50 m de offset), so que com um
   // burstRangeM maior que a distancia de menor aproximacao.
   FuzeState state{};
   const Vec3 relVel{-100.0, 0.0, 0.0};

   const auto before = proximityFuze(Vec3{100.0, 50.0, 0.0}, relVel, 60.0, state);
   state = before.nextState;

   const auto atClosest = proximityFuze(Vec3{0.0, 50.0, 0.0}, relVel, /*burstRangeM=*/60.0, state);
   EXPECT_TRUE(atClosest.closestApproachReached);
   EXPECT_TRUE(atClosest.hit);
   EXPECT_NEAR(atClosest.rangeAtEventM, 50.0, kEps);
}

TEST(ProximityFuze, PrimeiraAmostraNuncaDispara)
{
   // Sem historico (FuzeState{} default), nao ha "transicao" pra detectar
   // -- mesmo com taxa de alcance ja positiva (afastando) na primeira
   // leitura.
   const auto o = proximityFuze(Vec3{10.0, 0.0, 0.0}, Vec3{100.0, 0.0, 0.0}, 30.0, FuzeState{});
   EXPECT_FALSE(o.closestApproachReached);
   EXPECT_TRUE(o.nextState.hasSample);
}

} // namespace
} // namespace mixr::models::xmissile::domain
