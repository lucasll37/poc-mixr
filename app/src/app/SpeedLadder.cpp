#include "app/SpeedLadder.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace app {

std::vector<double> defaultSpeedLadder()
{
   return {0.1, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0};
}

std::string formatSpeedScale(const double scale)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(scale < 1.0 ? 2 : 0) << scale << "x";
   return oss.str();
}

SpeedDisplay speedDisplay(const bool fastBreakpointRun, const bool paused, const double timeScale,
                          const double actualTimeScale)
{
   if (fastBreakpointRun) {
      std::ostringstream oss;
      oss << "MAX (~" << std::fixed << std::setprecision(0) << actualTimeScale << "x real)";
      return SpeedDisplay{oss.str(), SpeedTone::Magenta};
   }
   if (paused) {
      return SpeedDisplay{"PAUSADO (" + formatSpeedScale(timeScale) + ")", SpeedTone::Red};
   }
   const SpeedTone tone{timeScale > 1.0 ? SpeedTone::Yellow
                                        : (timeScale < 1.0 ? SpeedTone::Cyan : SpeedTone::Green)};
   return SpeedDisplay{formatSpeedScale(timeScale), tone};
}

SpeedLadder::SpeedLadder(std::vector<double> ladder, const int realTimeIndex)
   : ladder_(std::move(ladder)), realTimeIndex_(realTimeIndex), index_(realTimeIndex)
{
}

void SpeedLadder::seedFromScale(const double scale)
{
   int best{realTimeIndex_};
   double bestDist{-1.0};
   for (std::size_t i = 0; i < ladder_.size(); i++) {
      const double dist{std::fabs(ladder_[i] - scale)};
      if (bestDist < 0.0 || dist < bestDist) { bestDist = dist; best = static_cast<int>(i); }
   }
   index_ = best;
}

bool SpeedLadder::accelerate()
{
   if (index_ >= size() - 1) return false;
   index_++;
   return true;
}

bool SpeedLadder::decelerate()
{
   if (index_ <= 0) return false;
   index_--;
   return true;
}

void SpeedLadder::toRealTime()
{
   index_ = realTimeIndex_;
}

} // namespace app
