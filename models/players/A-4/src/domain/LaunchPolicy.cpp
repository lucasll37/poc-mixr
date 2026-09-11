#include "domain/LaunchPolicy.hpp"

#include "domain/geometry.hpp"

#include <cmath>

namespace domain {

bool inLaunchEnvelope(const LaunchEnvelope& env, const double rangeM, const double relBearingDeg)
{
   if (rangeM < env.minRangeM || rangeM > env.maxRangeM) return false;

   // wrap180() por defesa -- contactRelBearingDeg ja chega em (-180,180] da
   // percepcao, mas a funcao nao deveria depender disso pra estar correta.
   return std::fabs(wrap180(relBearingDeg)) <= env.coneDeg;
}

} // namespace domain
