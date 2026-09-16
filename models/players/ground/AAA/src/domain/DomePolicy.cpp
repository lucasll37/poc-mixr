#include "domain/DomePolicy.hpp"

namespace mixr {
namespace models {
namespace xAAA {
namespace domain {

bool inDome(const Dome& dome, const double rangeM)
{
   return rangeM >= dome.minRangeM && rangeM <= dome.maxRangeM;
}

} // namespace domain
} // namespace xAAA
} // namespace models
} // namespace mixr
