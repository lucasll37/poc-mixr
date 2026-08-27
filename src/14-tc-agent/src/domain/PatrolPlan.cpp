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

} // namespace domain
