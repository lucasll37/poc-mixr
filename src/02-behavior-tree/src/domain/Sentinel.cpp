#include "domain/Sentinel.hpp"

#include <algorithm>

namespace domain {

void Sentinel::patrol()
{
   battery_ = std::max(0, battery_ - drainPerLap_);
   patrolLaps_ += 1;
}

void Sentinel::recharge()
{
   battery_ = fullBattery_;
}

}
