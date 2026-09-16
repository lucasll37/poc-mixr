#include "domain/ParachuteFsm.hpp"

#include <cstring>

namespace mixr {
namespace models {
namespace xparatrooper {
namespace domain {

Stage next(const Stage current, const double aglM, const JumpProfile& profile)
{
   if (current == Stage::LANDED) return Stage::LANDED;

   if (current == Stage::CANOPY) {
      return (aglM <= profile.groundAglM) ? Stage::LANDED : Stage::CANOPY;
   }

   // current == Stage::FREEFALL
   return (aglM <= profile.deployAglM) ? Stage::CANOPY : Stage::FREEFALL;
}

const char* labelOf(const Stage stage)
{
   switch (stage) {
      case Stage::FREEFALL: return "FREEFALL";
      case Stage::CANOPY:   return "CANOPY";
      case Stage::LANDED:   return "LANDED";
   }
   return "FREEFALL";
}

bool stageFromLabel(const char* const label, Stage& out)
{
   if (label == nullptr) return false;

   if (std::strcmp(label, "FREEFALL") == 0) { out = Stage::FREEFALL; return true; }
   if (std::strcmp(label, "CANOPY") == 0)   { out = Stage::CANOPY;   return true; }
   if (std::strcmp(label, "LANDED") == 0)   { out = Stage::LANDED;   return true; }

   return false;
}

} // namespace domain
} // namespace xparatrooper
} // namespace models
} // namespace mixr
