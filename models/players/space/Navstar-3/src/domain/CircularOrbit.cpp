#include "domain/CircularOrbit.hpp"

#include <cmath>

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace domain {

namespace {
constexpr double kDegToRad{3.14159265358979323846 / 180.0};
}

double semiMajorAxisM(const OrbitElements& e)
{
   return EARTH_EQUATORIAL_RADIUS_M + e.altitudeM;
}

double meanMotionRadPerSec(const OrbitElements& e)
{
   const double a{semiMajorAxisM(e)};
   return std::sqrt(MU_EARTH_M3_S2 / (a * a * a));
}

double orbitalPeriodSec(const OrbitElements& e)
{
   return 2.0 * 3.14159265358979323846 / meanMotionRadPerSec(e);
}

GroundTrack groundTrack(const OrbitElements& e, const double simTimeS)
{
   const double n{meanMotionRadPerSec(e)};
   const double u{(e.argLatDeg0 * kDegToRad) + n * simTimeS};
   const double i{e.inclinationDeg * kDegToRad};
   const double raan{e.raanDeg * kDegToRad};

   const double sinLat{std::sin(i) * std::sin(u)};
   const double latRad{std::asin(sinLat)};
   const double lonInertialRad{raan + std::atan2(std::cos(i) * std::sin(u), std::cos(u))};

   // Referencial fixo na Terra: subtrai o quanto a Terra girou desde t=0.
   // Nao repetir esta correcao em nenhum outro lugar (ver o cabecalho do
   // .hpp e ARCHITECTURE.md, armadilha 2).
   const double lonEarthFixedRad{lonInertialRad - EARTH_ROTATION_RATE_RAD_S * simTimeS};

   GroundTrack gt{};
   gt.latDeg = latRad / kDegToRad;
   gt.lonDeg = std::atan2(std::sin(lonEarthFixedRad), std::cos(lonEarthFixedRad)) / kDegToRad;
   gt.altM = e.altitudeM;
   return gt;
}

Vec3 eciPosition(const OrbitElements& e, const double simTimeS)
{
   const double a{semiMajorAxisM(e)};
   const double n{meanMotionRadPerSec(e)};
   const double u{(e.argLatDeg0 * kDegToRad) + n * simTimeS};
   const double i{e.inclinationDeg * kDegToRad};
   const double raan{e.raanDeg * kDegToRad};

   const double cosU{std::cos(u)};
   const double sinU{std::sin(u)};
   const double cosRaan{std::cos(raan)};
   const double sinRaan{std::sin(raan)};
   const double cosI{std::cos(i)};
   const double sinI{std::sin(i)};

   Vec3 pos{};
   pos.x = a * (cosRaan * cosU - sinRaan * sinU * cosI);
   pos.y = a * (sinRaan * cosU + cosRaan * sinU * cosI);
   pos.z = a * sinU * sinI;
   return pos;
}

} // namespace domain
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
