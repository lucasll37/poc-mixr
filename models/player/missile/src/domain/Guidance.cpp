#include "domain/Guidance.hpp"

#include <cmath>

namespace domain {

namespace {

constexpr double PI{3.14159265358979323846};
constexpr double RAD2DEG{180.0 / PI};

double wrap180(double deg)
{
   deg = std::fmod(deg + 180.0, 360.0);
   if (deg < 0.0) deg += 360.0;
   return deg - 180.0;
}

double clamp(double v, double lo, double hi)
{
   if (v < lo) return lo;
   if (v > hi) return hi;
   return v;
}

}

//------------------------------------------------------------------------------
// pursuit() -- ver o "porque" completo em domain/Guidance.hpp.
//
// Rumo ao alvo = atan2(leste, norte) (convencao NED, 0 = norte, sentido
// horario). Elevacao ao alvo = atan2(-desce, alcance horizontal): 'desce'
// negativo quer dizer alvo ACIMA (menos "para baixo" que nos), entao o sinal
// trocado da elevacao positiva para alvo acima -- que e o que
// setControlStickPitchInput(+1) pede (cabrar).
//------------------------------------------------------------------------------
GuidanceCommand pursuit(const double relNorthM, const double relEastM, const double relDownM,
                        const double ownHeadingDeg, const double ownPitchDeg,
                        const double ownRollRateDps, const double ownPitchRateDps,
                        const GuidanceGains& gains)
{
   GuidanceCommand cmd;

   const double bearingToTargetDeg{std::atan2(relEastM, relNorthM) * RAD2DEG};
   const double headingErrorDeg{wrap180(bearingToTargetDeg - ownHeadingDeg)};
   const double rollP{headingErrorDeg / gains.headingGainDeg};
   const double rollD{ownRollRateDps / gains.rollRateGainDps};
   cmd.rollNorm = clamp(rollP - rollD, -1.0, 1.0);

   const double horizontalRangeM{std::sqrt(relNorthM * relNorthM + relEastM * relEastM)};
   const double elevToTargetDeg{std::atan2(-relDownM, horizontalRangeM) * RAD2DEG};
   const double pitchErrorDeg{elevToTargetDeg - ownPitchDeg};
   const double pitchP{pitchErrorDeg / gains.pitchGainDeg};
   const double pitchD{ownPitchRateDps / gains.pitchRateGainDps};
   cmd.pitchNorm = clamp(pitchP - pitchD, -1.0, 1.0);

   return cmd;
}

double slewTowards(const double current, const double target, const double maxDelta)
{
   const double delta{target - current};
   if (delta > maxDelta) return current + maxDelta;
   if (delta < -maxDelta) return current - maxDelta;
   return target;
}

} // namespace domain
