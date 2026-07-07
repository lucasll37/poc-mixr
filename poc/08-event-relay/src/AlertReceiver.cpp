#include "AlertReceiver.hpp"
#include "events.hpp"

#include "mixr/models/player/Player.hpp"

#include <iostream>
#include <sstream>

namespace events {

IMPLEMENT_SUBCLASS(AlertReceiver, "AlertReceiver")
EMPTY_SLOTTABLE(AlertReceiver)
EMPTY_DELETEDATA(AlertReceiver)

AlertReceiver::AlertReceiver()
{
   STANDARD_CONSTRUCTOR()
}

void AlertReceiver::copyData(const AlertReceiver& org, const bool)
{
   BaseClass::copyData(org);
   // pendingAlert/numAlertsReceived sao estado de execucao, nao copiados.
}

BEGIN_EVENT_HANDLER(AlertReceiver)
   ON_EVENT_OBJ(CONTACT_EVENT, onContactEvent, RadarContactMessage)
END_EVENT_HANDLER()

bool AlertReceiver::onContactEvent(RadarContactMessage* const msg)
{
   numAlertsReceived += 1;

   const mixr::models::Player* const me{getOwnship()};
   const std::string myName{me != nullptr && me->getName() != nullptr ? me->getName()->getString() : "?"};

   std::ostringstream oss;
   oss << "Alert @" << myName << ": contact #" << msg->trackId << " (" << msg->targetName << ")"
       << " range=" << msg->rangeNM << "NM bearing=" << msg->bearingDeg << "deg"
       << " -- relayed by " << msg->sourcePlayerName;
   pendingAlert = oss.str();

   std::cout << "[AlertReceiver] " << pendingAlert << std::endl;
   return true;
}

std::string AlertReceiver::consumePendingAlert()
{
   std::string a;
   a.swap(pendingAlert);
   return a;
}

} // namespace events
