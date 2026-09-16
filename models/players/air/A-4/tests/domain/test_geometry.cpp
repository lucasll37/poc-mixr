// Geometria do plano tangente local (NED da gaming area).
//
// Vale testar apesar de ser "so trigonometria" porque a convencao aqui NAO e
// a do atan2 padrao: zero grau aponta para o NORTE e cresce para LESTE, o que
// significa atan2(dE, dN) e nao atan2(dN, dE). Trocar os argumentos produz um
// erro que so aparece como aeronave voando para o lado errado.

#include "domain/geometry.hpp"

#include <gtest/gtest.h>

namespace domain = mixr::models::xA_4::domain;

namespace {

constexpr double TOL{1e-9};

TEST(Geometry, Wrap360NormalizaParaZeroTrezentosESessenta)
{
   EXPECT_NEAR(domain::wrap360(0.0), 0.0, TOL);
   EXPECT_NEAR(domain::wrap360(360.0), 0.0, TOL);
   EXPECT_NEAR(domain::wrap360(370.0), 10.0, TOL);
   EXPECT_NEAR(domain::wrap360(-10.0), 350.0, TOL);
   EXPECT_NEAR(domain::wrap360(-370.0), 350.0, TOL);
   EXPECT_NEAR(domain::wrap360(720.0 + 45.0), 45.0, TOL);
}

// ATENCAO ao valor de borda: o header documenta "(-180, 180]", mas a
// implementacao devolve -180 tanto para +180 quanto para -180
// (fmod(360,360)==0 => 0-180). O intervalo real e [-180, 180). O teste trava o
// comportamento OBSERVADO, nao o comentario -- e a diferenca importa porque
// ThreatPolicy escolhe o lado da quebra por 'relBearingDeg >= 0'.
TEST(Geometry, Wrap180TemBordaFechadaEmMenos180)
{
   EXPECT_NEAR(domain::wrap180(0.0), 0.0, TOL);
   EXPECT_NEAR(domain::wrap180(90.0), 90.0, TOL);
   EXPECT_NEAR(domain::wrap180(-90.0), -90.0, TOL);
   EXPECT_NEAR(domain::wrap180(190.0), -170.0, TOL);
   EXPECT_NEAR(domain::wrap180(-190.0), 170.0, TOL);
   EXPECT_NEAR(domain::wrap180(180.0), -180.0, TOL);
   EXPECT_NEAR(domain::wrap180(-180.0), -180.0, TOL);
}

TEST(Geometry, HeadingToDegZeroApontaParaONorte)
{
   EXPECT_NEAR(domain::headingToDeg(0.0, 0.0, 100.0, 0.0), 0.0, TOL);     // norte
   EXPECT_NEAR(domain::headingToDeg(0.0, 0.0, 0.0, 100.0), 90.0, TOL);    // leste
   EXPECT_NEAR(domain::headingToDeg(0.0, 0.0, -100.0, 0.0), 180.0, TOL);  // sul
   EXPECT_NEAR(domain::headingToDeg(0.0, 0.0, 0.0, -100.0), 270.0, TOL);  // oeste
   EXPECT_NEAR(domain::headingToDeg(0.0, 0.0, 100.0, 100.0), 45.0, TOL);  // nordeste
}

TEST(Geometry, HeadingToDegDegradaParaZeroSobrePontoCoincidente)
{
   EXPECT_NEAR(domain::headingToDeg(500.0, -300.0, 500.0, -300.0), 0.0, TOL);
}

TEST(Geometry, DistanciaEhEuclidianaNoPlano)
{
   EXPECT_NEAR(domain::distanceM(0.0, 0.0, 3000.0, 4000.0), 5000.0, TOL);
   EXPECT_NEAR(domain::distanceM(1000.0, 1000.0, 1000.0, 1000.0), 0.0, TOL);
}

} // namespace
