#include "domain/ExampleThreshold.hpp"

namespace mixr {
namespace models {
namespace xtemplate {
namespace domain {

bool ExampleThreshold::next(const double value, const bool engaged) const
{
   if (engaged) return value >= offValue;
   return value >= onValue;
}

} // namespace domain
} // namespace xtemplate
} // namespace models
} // namespace mixr
