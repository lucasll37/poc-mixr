#include "domain/EclipseGeometry.hpp"

#include <gtest/gtest.h>

namespace mixr::models::xNavstar_3::domain {
namespace {

constexpr double kA{26558137.0};   // um raio orbital MEO plausivel, so para os vetores de teste
constexpr double kEarthR{EARTH_EQUATORIAL_RADIUS_M};

TEST(EclipseGeometry, SatelitePreciseAmenteNoEixoAntiSolEstaNaSombra)
{
   const Vec3 sat{-kA, 0.0, 0.0};
   const Vec3 sun{1.0, 0.0, 0.0};
   EXPECT_EQ(SunState::ECLIPSE, sunState(sat, sun));
}

TEST(EclipseGeometry, SateliteDoLadoDoSolEstaIluminado)
{
   const Vec3 sat{kA, 0.0, 0.0};
   const Vec3 sun{1.0, 0.0, 0.0};
   EXPECT_EQ(SunState::SUNLIT, sunState(sat, sun));
}

TEST(EclipseGeometry, SateliteAtrasDaTerraMasForaDoCilindroDeSombraEstaIluminado)
{
   const Vec3 sat{-kA, 1.1 * kEarthR, 0.0};   // lado noturno, mas deslocado lateralmente
   const Vec3 sun{1.0, 0.0, 0.0};
   EXPECT_EQ(SunState::SUNLIT, sunState(sat, sun));
}

TEST(EclipseGeometry, NaBordaExataDoCilindroDeSombraContaComoIluminado)
{
   // Convencao inclusiva (perpDist >= earthRadiusM -> SUNLIT), mesmo espirito
   // do >= usado em domain::ExampleThreshold/ParachuteFsm deste repositorio.
   const Vec3 sat{-kA, kEarthR, 0.0};
   const Vec3 sun{1.0, 0.0, 0.0};
   EXPECT_EQ(SunState::SUNLIT, sunState(sat, sun));
}

TEST(EclipseGeometry, LabelOfEhOInversoDeSunStateFromLabel)
{
   EXPECT_STREQ("ECLIPSE", labelOf(SunState::ECLIPSE));
   EXPECT_STREQ("SUNLIT", labelOf(SunState::SUNLIT));

   SunState out{};
   EXPECT_TRUE(sunStateFromLabel("ECLIPSE", out));
   EXPECT_EQ(SunState::ECLIPSE, out);

   EXPECT_TRUE(sunStateFromLabel("SUNLIT", out));
   EXPECT_EQ(SunState::SUNLIT, out);

   EXPECT_FALSE(sunStateFromLabel("QUALQUER-OUTRA-COISA", out));
   EXPECT_FALSE(sunStateFromLabel(nullptr, out));
}

} // namespace
} // namespace mixr::models::xNavstar_3::domain
