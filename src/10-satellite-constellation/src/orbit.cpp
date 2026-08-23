#include "orbit.hpp"

#include "mixr/base/util/nav_utils.hpp"

#include <cmath>

namespace orbit {

namespace {

// Parametro gravitacional padrao da Terra (m^3/s^2) -- valor WGS84/IERS
// padrao, nao existe no MIXR (nao ha nenhum modulo de mecanica orbital
// vendorizado pra reaproveitar essa constante).
constexpr double MU_EARTH{398600.4418e9};

// Velocidade de rotacao sideral da Terra (rad/s) -- mesma razao acima,
// constante nossa.
constexpr double EARTH_ROTATION_RATE{7.2921150e-5};

constexpr double PI{3.14159265358979323846};
constexpr double DEG2RAD{PI / 180.0};
constexpr double RAD2DEG{180.0 / PI};

double semiMajorAxisM(const CircularOrbit& o)
{
   // Raio equatorial WGS84 (mixr::base::nav::ERADM) + altitude -- unico
   // pedaco realmente nativo desta conta (a constante de raio da Terra).
   return mixr::base::nav::ERADM + o.altitudeM;
}

double meanMotion(const CircularOrbit& o)
{
   const double a{semiMajorAxisM(o)};
   return std::sqrt(MU_EARTH / (a * a * a));
}

} // namespace

double orbitalPeriodSec(const CircularOrbit& o)
{
   return 2.0 * PI / meanMotion(o);
}

void groundTrack(const CircularOrbit& o, const double t, double* latDeg, double* lonDeg, double* altM)
{
   const double n{meanMotion(o)};
   const double i{o.inclinationDeg * DEG2RAD};
   const double u{(o.argLatDeg0 * DEG2RAD) + n * t};

   const double lat{std::asin(std::sin(i) * std::sin(u))};
   const double deltaLon{std::atan2(std::cos(i) * std::sin(u), std::cos(u))};
   const double lon{(o.raanDeg * DEG2RAD) + deltaLon - EARTH_ROTATION_RATE * t};

   *latDeg = lat * RAD2DEG;
   // normaliza para [-180, 180)
   *lonDeg = std::fmod(lon * RAD2DEG + 540.0, 360.0) - 180.0;
   *altM = o.altitudeM;
}

} // namespace orbit
