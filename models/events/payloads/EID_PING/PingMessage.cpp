#include "events/payloads/EID_PING/PingMessage.hpp"

namespace mixr {
namespace events {

IMPLEMENT_SUBCLASS(PingMessage, "PingMessage")
EMPTY_SLOTTABLE(PingMessage)
EMPTY_DELETEDATA(PingMessage)

PingMessage::PingMessage()
{
   STANDARD_CONSTRUCTOR()
}

void PingMessage::copyData(const PingMessage& org, const bool)
{
   BaseClass::copyData(org);

   senderId = org.senderId;
   senderName = org.senderName;
   sequence = org.sequence;
   message = org.message;
}

} // namespace events
} // namespace mixr
