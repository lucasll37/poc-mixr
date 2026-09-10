#include "domain/CircularOrbit.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace mixr::models::xNavstar_3::domain {
namespace {

// Helper: normaliza um angulo em graus para (-180, 180].
double wrap180(double deg)
{
   while (deg <= -180.0) deg += 360.0;
   while (deg > 180.0) deg -= 360.0;
   return deg;
}

TEST(CircularOrbit, SemiMajorAxisSomaOAltitudeAoRaioEquatorial)
{
   const OrbitElements e{/*altitudeM=*/20180000.0, 55.0, 0.0, 0.0};
   EXPECT_DOUBLE_EQ(EARTH_EQUATORIAL_RADIUS_M + 20180000.0, semiMajorAxisM(e));
}

// Defaults sao valores REPRESENTATIVOS de uma orbita GPS/MEO moderna --
// nao reivindica precisao historica do Navstar-3 real (SVN-3, 1978). O
// periodo resultante (~12h) e' a assinatura conhecida de uma orbita GPS: bate
// com o valor calculado a mao (verificado fora deste arquivo, com os mesmos
// numeros de MU_EARTH_M3_S2/EARTH_EQUATORIAL_RADIUS_M).
TEST(CircularOrbit, PeriodoDosDefaultsEhRepresentativoDeUmaOrbitaGpsMeo)
{
   const OrbitElements e{};   // defaults: ~20.180 km, 55 deg, 0, 0
   const double period{orbitalPeriodSec(e)};

   EXPECT_NEAR(43073.225, period, 0.1);           // ~11h57m53s
   EXPECT_NEAR(2.0 * 3.14159265358979323846 / meanMotionRadPerSec(e), period, 1e-6);
}

TEST(CircularOrbit, GroundTrackMantemAAltitudeConstante)
{
   // A circularidade e' por construcao (nao ha excentricidade modelada --
   // ver ARCHITECTURE.md, "fidelidade orbital"), mas este teste protege
   // contra uma futura mudanca acidental que quebrasse esse contrato em
   // silencio.
   const OrbitElements e{20180000.0, 55.0, 0.0, 0.0};
   for (const double t : {0.0, 1000.0, 20000.0, 43073.225, 1.0e6}) {
      EXPECT_DOUBLE_EQ(20180000.0, groundTrack(e, t).altM) << "t=" << t;
   }
}

// A propriedade real e conhecida de uma constelacao GPS: o periodo orbital e'
// muito proximo de METADE de um dia sideral, entao o no ascendente "anda"
// quase 180 graus a cada orbita e o ground-track se REPETE (dentro de uma
// fracao de grau) a cada DUAS orbitas / um dia sideral.
TEST(CircularOrbit, NoAscendenteDerivaQuaseMeioMundoPorOrbita)
{
   const OrbitElements e{20180000.0, 55.0, 0.0, 0.0};
   const double period{orbitalPeriodSec(e)};

   const GroundTrack gt0{groundTrack(e, 0.0)};
   const GroundTrack gt1{groundTrack(e, period)};

   // t=0 e t=period tem o MESMO argumento de latitude (u avancou 2*pi) --
   // latitude tem que bater exatamente.
   EXPECT_NEAR(gt0.latDeg, gt1.latDeg, 1e-9);

   const double drift{wrap180(gt1.lonDeg - gt0.lonDeg)};
   EXPECT_NEAR(-179.963, drift, 0.01);
}

TEST(CircularOrbit, GroundTrackQuaseSeRepeteApos2Orbitas)
{
   const OrbitElements e{20180000.0, 55.0, 0.0, 0.0};
   const double period{orbitalPeriodSec(e)};

   const GroundTrack gt0{groundTrack(e, 0.0)};
   const GroundTrack gt2{groundTrack(e, 2.0 * period)};

   const double drift{wrap180(gt2.lonDeg - gt0.lonDeg)};
   EXPECT_NEAR(0.0737, drift, 0.01);
}

// eciPosition() e groundTrack() derivam da MESMA relacao (sin(i)*sin(u)) --
// se algum dia divergirem por um bug de copia-e-cola, este teste pega.
TEST(CircularOrbit, EciPositionBateComGroundTrackNaLatitude)
{
   const OrbitElements e{20180000.0, 55.0, 12.0, 30.0};
   const double a{semiMajorAxisM(e)};

   for (const double t : {0.0, 1000.0, 12345.0}) {
      const Vec3 eci{eciPosition(e, t)};
      const double latFromEci{std::asin(eci.z / a) * 180.0 / 3.14159265358979323846};
      const double latFromGroundTrack{groundTrack(e, t).latDeg};
      EXPECT_NEAR(latFromGroundTrack, latFromEci, 1e-9) << "t=" << t;
   }
}

} // namespace
} // namespace mixr::models::xNavstar_3::domain
