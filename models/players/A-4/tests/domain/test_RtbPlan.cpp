// Retorno a base. Regra curta, com um detalhe facil de quebrar: chegando na
// base o plano PARA de comandar rumo para o ponto (senao a aeronave orbita
// tentando alcancar um alvo que ja esta debaixo dela) e reduz a velocidade.

#include "domain/RtbPlan.hpp"

#include <gtest/gtest.h>

namespace {

constexpr double TOL{1e-9};
constexpr double NM{1852.0};

domain::RtbPlan planoNaOrigem()
{
   domain::RtbPlan p;
   p.configure(0.0, 0.0, 2.0 * NM, 2050.0, 170.0);
   return p;
}

TEST(RtbPlan, ApontaParaABaseQuandoEstaLonge)
{
   const auto p{planoNaOrigem()};
   // 10 NM ao norte da base => tem de voar para o sul
   const auto cmd{p.command(10.0 * NM, 0.0, 45.0)};
   EXPECT_NEAR(cmd.headingDeg, 180.0, TOL);
   EXPECT_NEAR(cmd.altitudeM, 2050.0, TOL);
   EXPECT_NEAR(cmd.speedKts, 170.0, TOL);
   EXPECT_FALSE(p.arrived(10.0 * NM, 0.0));
}

TEST(RtbPlan, DistanciaEhAEuclidianaAteABase)
{
   const auto p{planoNaOrigem()};
   EXPECT_NEAR(p.distanceToBaseM(3000.0, 4000.0), 5000.0, TOL);
}

TEST(RtbPlan, RaioDeChegadaEhInclusivo)
{
   const auto p{planoNaOrigem()};
   EXPECT_TRUE(p.arrived(2.0 * NM, 0.0)) << "exatamente no raio ja conta como chegou";
   EXPECT_TRUE(p.arrived(1.0 * NM, 0.0));
   EXPECT_FALSE(p.arrived(2.0 * NM + 1.0, 0.0));
}

TEST(RtbPlan, ChegandoMantemORumoAtualEFreia)
{
   const auto p{planoNaOrigem()};
   const auto cmd{p.command(500.0, 0.0, 123.0)};
   EXPECT_NEAR(cmd.headingDeg, 123.0, TOL) << "sobre a base, mantem o rumo em vez de girar";
   EXPECT_NEAR(cmd.speedKts, 170.0 * 0.6, TOL);
   EXPECT_NEAR(cmd.altitudeM, 2050.0, TOL);
}

TEST(RtbPlan, RumoMantidoNaChegadaEhNormalizado)
{
   const auto p{planoNaOrigem()};
   EXPECT_NEAR(p.command(0.0, 0.0, -30.0).headingDeg, 330.0, TOL);
   EXPECT_NEAR(p.command(0.0, 0.0, 400.0).headingDeg, 40.0, TOL);
}

TEST(RtbPlan, RaioNaoPositivoCaiNoDefaultDeQuinhentosMetros)
{
   domain::RtbPlan p;
   p.configure(0.0, 0.0, 0.0, 1000.0, 150.0);
   EXPECT_TRUE(p.arrived(499.0, 0.0));
   EXPECT_FALSE(p.arrived(501.0, 0.0));
}

TEST(RtbPlan, BaseForaDaOrigemTambemFunciona)
{
   domain::RtbPlan p;
   p.configure(5000.0, -3000.0, 500.0, 1200.0, 160.0);
   EXPECT_NEAR(p.distanceToBaseM(5000.0, -3000.0), 0.0, TOL);
   // a leste da base => voa para oeste
   EXPECT_NEAR(p.command(5000.0, 7000.0, 0.0).headingDeg, 270.0, TOL);
}

} // namespace
