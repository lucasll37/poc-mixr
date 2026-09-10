#include "domain/geometry.hpp"

#include <cmath>

namespace mixr {
namespace models {
namespace xC_130 {
namespace domain {

double wrap180(const double deg)
{
   double d{std::fmod(deg + 180.0, 360.0)};
   if (d < 0.0) d += 360.0;
   return d - 180.0;
}

double wrap360(const double deg)
{
   double d{std::fmod(deg, 360.0)};
   if (d < 0.0) d += 360.0;
   return d;
}

} // namespace domain
} // namespace xC_130
} // namespace models
} // namespace mixr
