#include "app/MapGeometry.hpp"

#include <algorithm>
#include <cmath>

namespace app {
namespace mapgeometry {

Projected project(const double northM, const double eastM, const double altitudeM,
                  const MapViewState& view)
{
   const double relN{northM - view.panNorthM};
   const double relE{eastM - view.panEastM};
   const double yaw{view.viewYawDeg * kDeg2Rad};

   const double rotE{relE * std::cos(yaw) - relN * std::sin(yaw)};
   const double rotN{relE * std::sin(yaw) + relN * std::cos(yaw)};

   const int cx{kCanvasW / 2};
   const int cy{kCanvasH / 2};

   Projected p;
   p.px = cx + static_cast<int>(std::lround(rotE / view.metersPerCell));
   if (view.perspective == Perspective::TopDown) {
      p.py = cy - static_cast<int>(std::lround(rotN / view.metersPerCell));
   } else {
      p.py = cy - static_cast<int>(std::lround((altitudeM - view.panAltM) / view.metersPerCell));
   }
   p.onCanvas = (p.px >= 0 && p.px < kCanvasW && p.py >= 0 && p.py < kCanvasH);
   return p;
}

void worldFromCanvasTopDown(const int px, const int py, const MapViewState& view,
                            double& northM, double& eastM)
{
   const int cx{kCanvasW / 2};
   const int cy{kCanvasH / 2};
   const double rotE{(px - cx) * view.metersPerCell};
   const double rotN{(cy - py) * view.metersPerCell};
   const double yaw{view.viewYawDeg * kDeg2Rad};
   const double relE{rotE * std::cos(yaw) + rotN * std::sin(yaw)};
   const double relN{-rotE * std::sin(yaw) + rotN * std::cos(yaw)};
   northM = view.panNorthM + relN;
   eastM = view.panEastM + relE;
}

void worldFromCanvasLateral(const int px, const MapViewState& view, double& northM, double& eastM)
{
   const int cx{kCanvasW / 2};
   const double rotE{(px - cx) * view.metersPerCell};
   const double yaw{view.viewYawDeg * kDeg2Rad};
   northM = view.panNorthM - rotE * std::sin(yaw);
   eastM = view.panEastM + rotE * std::cos(yaw);
}

double contourIntervalFor(const double minM, const double maxM)
{
   const double range{std::max(1.0, maxM - minM)};
   const double roughStep{range / 4.0};
   static const double niceSteps[]{5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000};
   for (const double s : niceSteps) {
      if (s >= roughStep) return s;
   }
   return 5000.0;
}

} // namespace mapgeometry
} // namespace app
