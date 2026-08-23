#include "xair/TacticalAlert.hpp"

namespace mixr {
namespace xair {

IMPLEMENT_SUBCLASS(TacticalAlert, "TacticalAlert")
EMPTY_SLOTTABLE(TacticalAlert)
EMPTY_DELETEDATA(TacticalAlert)

TacticalAlert::TacticalAlert()
{
   STANDARD_CONSTRUCTOR()
}

void TacticalAlert::copyData(const TacticalAlert& org, const bool)
{
   BaseClass::copyData(org);

   senderId = org.senderId;
   senderName = org.senderName;
   contactName = org.contactName;
   northM = org.northM;
   eastM = org.eastM;
   altitudeM = org.altitudeM;
   rangeM = org.rangeM;
}

} // namespace xair
} // namespace mixr
