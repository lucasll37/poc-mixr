#include "domain/LaunchPolicy.hpp"

#include <cmath>

namespace domain {

bool inLaunchEnvelope(const LaunchEnvelope& env, const double rangeM, const double relBearingDeg)
{
   if (rangeM < env.minRangeM || rangeM > env.maxRangeM) return false;
   return std::fabs(relBearingDeg) <= env.coneDeg;
}

} // namespace domain
