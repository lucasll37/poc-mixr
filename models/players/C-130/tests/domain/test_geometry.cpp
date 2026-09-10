// wrap180/wrap360 -- usados pelo limitador de taxa de guinada de
// bt::NavigateAction. Vale testar apesar de ser "so aritmetica" pelo mesmo
// motivo documentado em models/players/A-4: a borda de wrap180 e fechada em
// -180 (fmod(360,360)==0 => 0-180), nao no +180 que o comentario do header
// sugere -- o teste trava o comportamento OBSERVADO.

#include "domain/geometry.hpp"

#include <gtest/gtest.h>

namespace {

using namespace mixr::models::xC_130;

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

} // namespace
