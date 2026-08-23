#include "domain/Mission.hpp"

#include <algorithm>

namespace domain {

Mission::Mission()
{
   // waypoint 0 coincide com o estado inicial da aeronave no scenario.epp,
   // para nao dar um salto brusco de comando logo no primeiro tick.
   waypoints_.push_back(Waypoint{0.0, 5000.0, 250.0});
   waypoints_.push_back(Waypoint{90.0, 8000.0, 300.0});
   waypoints_.push_back(Waypoint{200.0, 6000.0, 280.0});
}

void Mission::tick(const double dt)
{
   fuel_ = std::max(0.0, fuel_ - kBurnRatePerSec * dt);

   if (inRtb_) {
      rtbElapsed_ += dt;
   } else {
      waypointElapsed_ += dt;
   }
}

bool Mission::isFuelLow() const
{
   return fuel_ < kLowFuelThreshold;
}

bool Mission::isInRtb() const
{
   return inRtb_;
}

double Mission::getFuel() const
{
   return fuel_;
}

const Waypoint& Mission::currentWaypoint() const
{
   return waypoints_[currentWaypointIndex_];
}

const Waypoint& Mission::baseWaypoint() const
{
   return waypoints_[0];
}

std::size_t Mission::currentWaypointIndex() const
{
   return currentWaypointIndex_;
}

bool Mission::maybeAdvanceWaypoint()
{
   if (waypointElapsed_ < kWaypointDwellSec) return false;

   currentWaypointIndex_ = (currentWaypointIndex_ + 1) % waypoints_.size();
   waypointElapsed_ = 0.0;
   return true;
}

void Mission::enterRtb()
{
   if (inRtb_) return;

   inRtb_ = true;
   rtbElapsed_ = 0.0;
}

bool Mission::maybeCompleteRtb()
{
   if (!inRtb_ || rtbElapsed_ < kRtbDwellSec) return false;

   fuel_ = 100.0;
   inRtb_ = false;
   rtbElapsed_ = 0.0;
   waypointElapsed_ = 0.0;
   return true;
}

} // namespace domain
