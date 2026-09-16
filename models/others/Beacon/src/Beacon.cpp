#include "Beacon.hpp"

#include "xlog/Log.hpp"
#include "events/EventTokens.hpp"

#include "mixr/models/WorldModel.hpp"

#include "mixr/base/List.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/Identifier.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/units/Times.hpp"

namespace mixr {
namespace models {
namespace xBeacon {

IMPLEMENT_SUBCLASS(Beacon, "Beacon")

// clang-format off
BEGIN_SLOTTABLE(Beacon)
   "pingInterval",   // 1: intervalo entre um ping e o proximo
   "pingMessage",    // 2: texto levado no payload de cada ping
END_SLOTTABLE(Beacon)

BEGIN_SLOT_MAP(Beacon)
   ON_SLOT(1, setSlotPingInterval, base::Time)
   ON_SLOT(2, setSlotPingMessage,  base::String)
END_SLOT_MAP()

BEGIN_EVENT_HANDLER(Beacon)
   ON_EVENT_OBJ(events::EID_PING, onPingEvent, events::PingMessage)
END_EVENT_HANDLER()

Beacon::Beacon()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
   static base::String generic("Beacon");
   setType(&generic);
}

void Beacon::copyData(const Beacon& org, const bool)
{
   BaseClass::copyData(org);

   pingIntervalSec = org.pingIntervalSec;
   pingMessage = org.pingMessage;

   pingTimer = 0.0;
   sequence = 0;
   sentCount = 0;
   receivedCount = 0;
   lastSequenceReceived = 0;
   lastSenderName.clear();
   lastMessage.clear();
}

EMPTY_DELETEDATA(Beacon)

void Beacon::reset()
{
   BaseClass::reset();

   // Nasce pronto para emitir no primeiro updateData() -- sem esperar o
   // primeiro intervalo inteiro, o cenario de demonstracao mostra o
   // primeiro ping em segundos, nao so' depois de 'pingInterval'.
   pingTimer = 0.0;
   sequence = 0;
   sentCount = 0;
   receivedCount = 0;
   lastSequenceReceived = 0;
   lastSenderName.clear();
   lastMessage.clear();
}

//------------------------------------------------------------------------------
// updateData() -- fase de FUNDO (10 Hz em tempo real, ou o dt que
// '-deterministic' passar). Acumula ate estourar 'pingIntervalSec' e
// dispara -- soma o resto em vez de zerar, para nao acumular deriva quando
// dt nao divide o intervalo exatamente.
//------------------------------------------------------------------------------
void Beacon::updateData(const double dt)
{
   BaseClass::updateData(dt);
   if (dt <= 0.0) return;

   pingTimer -= dt;
   if (pingTimer <= 0.0) {
      broadcastPing();
      pingTimer += pingIntervalSec;
   }
}

void Beacon::broadcastPing()
{
   sequence += 1;

   const auto msg = new events::PingMessage();
   msg->setSender(getID(), (getName() != nullptr) ? getName()->getString() : "?");
   msg->setSequence(sequence);
   msg->setMessage(pingMessage);

   unsigned int delivered{};
   WorldModel* const world{getWorldModel()};
   if (world != nullptr) {
      base::PairStream* players{world->getPlayers()};
      if (players != nullptr) {
         base::List::Item* item{players->getFirstItem()};
         while (item != nullptr) {
            const auto pair = static_cast<base::Pair*>(item->getValue());
            const auto player = static_cast<Player*>(pair->object());
            if (player->isLocalPlayer()) {
               if (player != this && (player->isActive() || player->isMode(Player::PRE_RELEASE))) {
                  player->event(events::EID_PING, msg);
                  delivered += 1;
               }
               item = item->getNext();
            } else {
               item = nullptr;   // networked ficam no fim da lista
            }
         }
         players->unref();
      }
   }

   sentCount += 1;
   LOG(INFO) << "[Beacon] " << ((getName() != nullptr) ? getName()->getString() : "?")
             << " emitiu ping #" << sequence << " (\"" << pingMessage << "\") para "
             << delivered << " player(es)";

   msg->unref();
}

//------------------------------------------------------------------------------
// onPingEvent() -- o TRATAMENTO do evento. Roda na thread do EMISSOR
// (Component::event() e sincrono), nunca em paralelo com o proprio
// updateData() deste Beacon (que so' roda na fase de fundo) -- por isso os
// campos abaixo nao precisam de mutex, ao contrario de
// xnative::AlertDatalink (models/players/air/A-4), que e' alcancada em paralelo
// pelo pool de tempo critico e por isso precisa de um.
//------------------------------------------------------------------------------
bool Beacon::onPingEvent(events::PingMessage* const msg)
{
   if (msg == nullptr) return false;

   receivedCount += 1;
   lastSequenceReceived = msg->getSequence();
   lastSenderName = msg->getSenderName();
   lastMessage = msg->getMessage();

   LOG(INFO) << "[Beacon] " << ((getName() != nullptr) ? getName()->getString() : "?")
             << " recebeu ping #" << lastSequenceReceived << " de " << lastSenderName
             << ": \"" << lastMessage << "\"";
   return true;
}

bool Beacon::setSlotPingInterval(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   pingIntervalSec = base::Seconds::convertStatic(*msg);
   return (pingIntervalSec > 0.0);
}

bool Beacon::setSlotPingMessage(const base::String* const msg)
{
   if (msg == nullptr || msg->getString() == nullptr) return false;
   pingMessage = msg->getString();
   return true;
}

} // namespace xBeacon
} // namespace models
} // namespace mixr
