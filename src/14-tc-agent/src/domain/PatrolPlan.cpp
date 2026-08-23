#include "domain/PatrolPlan.hpp"

#include "domain/geometry.hpp"

namespace domain {

void PatrolPlan::configure(const double startHeadingDeg, const double legSeconds,
                           const double turnPerLegDeg, const double altitudeM,
                           const double speedKts)
{
   startHeadingDeg_ = startHeadingDeg;
   legSeconds_ = (legSeconds > 0.0) ? legSeconds : 60.0;
   turnPerLegDeg_ = turnPerLegDeg;
   altitudeM_ = altitudeM;
   speedKts_ = speedKts;
   reset();
}

void PatrolPlan::reset()
{
   legTimer_ = 0.0;
   leg_ = 0;
}

bool PatrolPlan::advance(const double dt)
{
   legTimer_ += dt;
   if (legTimer_ < legSeconds_) return false;

   legTimer_ -= legSeconds_;
   leg_ += 1;
   return true;
}

FlightCommand PatrolPlan::command() const
{
   FlightCommand cmd;
   cmd.headingDeg = wrap360(startHeadingDeg_ + turnPerLegDeg_ * static_cast<double>(leg_));
   cmd.altitudeM = altitudeM_;
   cmd.speedKts = speedKts_;
   return cmd;
}

void RtbPlan::configure(const double baseN, const double baseE, const double arrivalRadiusM,
                        const double altitudeM, const double speedKts)
{
   baseN_ = baseN;
   baseE_ = baseE;
   arrivalRadiusM_ = (arrivalRadiusM > 0.0) ? arrivalRadiusM : 500.0;
   altitudeM_ = altitudeM;
   speedKts_ = speedKts;
}

FlightCommand RtbPlan::command(const double ownN, const double ownE, const double ownHeadingDeg) const
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
