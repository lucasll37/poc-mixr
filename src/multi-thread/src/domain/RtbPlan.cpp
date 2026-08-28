#include "domain/RtbPlan.hpp"

#include "domain/geometry.hpp"

namespace domain {

void RtbPlan::configure(const double baseN, const double baseE, const double arrivalRadiusM,
                        const double altitudeM, const double speedKts)
{
   baseN_ = baseN;
   baseE_ = baseE;
   arrivalRadiusM_ = (arrivalRadiusM > 0.0) ? arrivalRadiusM : 500.0;
   altitudeM_ = altitudeM;
   speedKts_ = speedKts;
}

FlightCommand RtbPlan::command(const double ownN, const double ownE,
                               const double ownHeadingDeg) const
{
   FlightCommand cmd;
   cmd.altitudeM = altitudeM_;
   cmd.speedKts = speedKts_;

   if (arrived(ownN, ownE)) {
      // Em cima da base: mantem o rumo atual e espera o reabastecimento
      // (quem controla o combustivel e o FuelSystem, nao este plano).
      cmd.headingDeg = wrap360(ownHeadingDeg);
      cmd.speedKts = speedKts_ * 0.6;
      return cmd;
   }

   cmd.headingDeg = headingToDeg(ownN, ownE, baseN_, baseE_);
   return cmd;
}

bool RtbPlan::arrived(const double ownN, const double ownE) const
{
   return distanceToBaseM(ownN, ownE) <= arrivalRadiusM_;
}

double RtbPlan::distanceToBaseM(const double ownN, const double ownE) const
{
   return distanceM(ownN, ownE, baseN_, baseE_);
}

} // namespace domain
