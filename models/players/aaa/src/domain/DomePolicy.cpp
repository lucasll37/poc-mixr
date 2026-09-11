#include "domain/DomePolicy.hpp"

namespace mixr {
namespace models {
namespace xaaa {
namespace domain {

bool inDome(const Dome& dome, const double rangeM)
{
   return rangeM >= dome.minRangeM && rangeM <= dome.maxRangeM;
}

} // namespace domain
} // namespace xaaa
} // namespace models
} // namespace mixr
