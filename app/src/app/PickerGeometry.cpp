#include "app/PickerGeometry.hpp"

#include <algorithm>

namespace app {
namespace pickergeometry {

Geometry computeGeometry(const int itemCount)
{
   const int menuLines{std::clamp(itemCount, kMinMenuLines, kMaxMenuLines)};
   return Geometry{menuLines, kChromeLines + menuLines};
}

} // namespace pickergeometry
} // namespace app
