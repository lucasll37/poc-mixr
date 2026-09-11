#include "domain/Guidance.hpp"

#include <cmath>

namespace mixr {
namespace models {
namespace xmissile {
namespace domain {

namespace {
constexpr double kMinRangeForLosRateM{1.0}; // abaixo disso a taxa da LOS degenera (divide por ~0)
}

GuidanceCommand proportionalNavigation(const Vec3& relPos, const Vec3& relVel, const GuidanceGains& gains)
{
   const double range2d = std::sqrt(relPos.n * relPos.n + relPos.e * relPos.e);
   const double range3d = std::sqrt(relPos.n * relPos.n + relPos.e * relPos.e + relPos.d * relPos.d);

   const double losAz = std::atan2(relPos.e, relPos.n);
   const double losEl = std::atan2(-relPos.d, range2d);

   // taxa de azimute da LOS: d(atan2(e,n))/dt = (n*ve - e*vn) / (n^2+e^2).
   double losAzRate{0.0};
   if (range2d > kMinRangeForLosRateM) {
      losAzRate = (relPos.n * relVel.e - relPos.e * relVel.n) / (range2d * range2d);
   }

   // taxa de elevacao da LOS: el = atan2(h, r), h=-relPos.d, r=range2d.
   // d(el)/dt = (r*dh/dt - h*dr/dt) / (r^2+h^2), dr/dt = (n*vn+e*ve)/r.
   double losElRate{0.0};
   if (range3d > kMinRangeForLosRateM && range2d > kMinRangeForLosRateM) {
      const double h{-relPos.d};
      const double dRange2dDt{(relPos.n * relVel.n + relPos.e * relVel.e) / range2d};
      const double dhDt{-relVel.d};
      losElRate = (range2d * dhDt - h * dRange2dDt) / (range3d * range3d);
   }

   GuidanceCommand cmd{};
   cmd.cmdHeadingRad = losAz + gains.navRatio * losAzRate;
   cmd.cmdPitchRad = losEl + gains.navRatio * losElRate;
   cmd.cmdSpeedMps = gains.cruiseSpeedMps;
   return cmd;
}

FuzeOutcome proximityFuze(const Vec3& relPos, const Vec3& relVel, const double burstRangeM, const FuzeState& prev)
{
   const double range{std::sqrt(relPos.n * relPos.n + relPos.e * relPos.e + relPos.d * relPos.d)};

   // taxa de alcance: d(range)/dt = (relPos . relVel) / range. Negativa =
   // aproximando (alcance diminuindo).
   double rangeRate{0.0};
   if (range > kMinRangeForLosRateM) {
      rangeRate = (relPos.n * relVel.n + relPos.e * relVel.e + relPos.d * relVel.d) / range;
   }
   const bool approaching{rangeRate < 0.0};

   FuzeOutcome out{};
   out.nextState = FuzeState{/*hasSample=*/true, approaching};

   if (prev.hasSample && prev.wasApproaching && !approaching) {
      out.closestApproachReached = true;
      out.rangeAtEventM = range;
      out.hit = (range <= burstRangeM);
   }

   return out;
}

} // namespace domain
} // namespace xmissile
} // namespace models
} // namespace mixr
