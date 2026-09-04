#include "domain/geometry.hpp"

#include <cmath>

namespace domain {

namespace {
const double RAD2DEG{57.295779513082320876798154814105};
}

double wrap180(const double deg)
{
   double d{std::fmod(deg + 180.0, 360.0)};
   if (d < 0.0) d += 360.0;
   return d - 180.0;
}

double wrap360(const double deg)
{
   double d{std::fmod(deg, 360.0)};
   if (d < 0.0) d += 360.0;
   return d;
}

double headingToDeg(const double fromN, const double fromE, const double toN, const double toE)
{
   const double dN{toN - fromN};
   const double dE{toE - fromE};
   if (dN == 0.0 && dE == 0.0) return 0.0;
   // atan2(Leste, Norte): zero grau aponta para o norte e cresce para leste
   return wrap360(std::atan2(dE, dN) * RAD2DEG);
}

double distanceM(const double fromN, const double fromE, const double toN, const double toE)
{
   const double dN{toN - fromN};
   const double dE{toE - fromE};
   return std::sqrt(dN * dN + dE * dE);
}

RelativeGeometry relativeTo(const double ownN, const double ownE, const double ownAltM,
                            const double ownHeadingDeg,
                            const double tgtN, const double tgtE, const double tgtAltM)
{
   RelativeGeometry g;
   g.rangeM = distanceM(ownN, ownE, tgtN, tgtE);
   g.bearingDeg = headingToDeg(ownN, ownE, tgtN, tgtE);
   g.relBearingDeg = wrap180(g.bearingDeg - ownHeadingDeg);
   g.deltaAltM = tgtAltM - ownAltM;
   return g;
}

} // namespace domain
