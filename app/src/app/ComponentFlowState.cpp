#include "app/ComponentFlowState.hpp"

#include <algorithm>

namespace app {

EstimatedPhase currentFlowPhase(const ComponentFlowState& flow)
{
   return kComponentFlowCycle[flow.cycleIndex % kComponentFlowCycleLen];
}

void advanceComponentFlowStep(ComponentFlowState& flow)
{
   flow.cycleIndex = (flow.cycleIndex + 1) % kComponentFlowCycleLen;
   flow.redrawsSincePlay = 0;
}

void setComponentFlowPlaying(ComponentFlowState& flow, const bool playing)
{
   if (flow.playing == playing) return;   // sem isto, o contador zeraria a cada redesenho
   flow.playing = playing;
   flow.redrawsSincePlay = 0;
}

void cycleComponentFlowSpeed(ComponentFlowState& flow)
{
   if (flow.stepsPerSecond >= 4)      flow.stepsPerSecond = 1;
   else if (flow.stepsPerSecond >= 2) flow.stepsPerSecond = 4;
   else                                flow.stepsPerSecond = 2;
}

void tickComponentFlowAnimation(ComponentFlowState& flow, const int redrawsPerSecond)
{
   if (!flow.playing) return;

   const int speed{std::clamp(flow.stepsPerSecond, 1, 4)};
   const int framesPerStep{std::max(1, redrawsPerSecond / speed)};

   flow.redrawsSincePlay += 1;
   if (flow.redrawsSincePlay >= framesPerStep) {
      advanceComponentFlowStep(flow);   // ja zera 'redrawsSincePlay'
   }
}

} // namespace app
