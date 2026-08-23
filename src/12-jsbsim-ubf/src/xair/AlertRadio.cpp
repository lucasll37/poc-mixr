#include "xair/AlertRadio.hpp"

#include "xair/TacticalAlert.hpp"
#include "xair/event_tokens.hpp"

#include "domain/geometry.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/List.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/Times.hpp"

namespace mixr {
namespace xair {

IMPLEMENT_SUBCLASS(AlertRadio, "AlertRadio")

BEGIN_SLOTTABLE(AlertRadio)
   "range",      // 1: alcance da transmissao
   "holdTime",   // 2: validade do alerta recebido
END_SLOTTABLE(AlertRadio)

BEGIN_SLOT_MAP(AlertRadio)
   ON_SLOT(1, setSlotRange,    base::Distance)
   ON_SLOT(2, setSlotHoldTime, base::Time)
END_SLOT_MAP()

//------------------------------------------------------------------------------
// Tabela de eventos: e ISTO que declara "esta classe aceita este evento".
// Sem a entrada aqui, o event() cairia no BaseClass e o evento morreria em
// silencio (eventos nao-tecla nao propagam sozinhos).
//------------------------------------------------------------------------------
BEGIN_EVENT_HANDLER(AlertRadio)
   ON_EVENT_OBJ(TACTICAL_ALERT_EVENT, onTacticalAlert, base::Object)
END_EVENT_HANDLER()

EMPTY_DELETEDATA(AlertRadio)

AlertRadio::AlertRadio()
{
   STANDARD_CONSTRUCTOR()
}

void AlertRadio::copyData(const AlertRadio& org, const bool)
{
   BaseClass::copyData(org);

   rangeM = org.rangeM;
   holdTimeSec = org.holdTimeSec;

   broadcastPending = false;
   outgoing = Alert{};
   staged = Alert{};
   current = Alert{};
   holdTimer = 0.0;
   sentCount = 0;
   receivedCount = 0;
}

void AlertRadio::reset()
{
   BaseClass::reset();

   std::lock_guard<std::mutex> lock(inboxMutex);
   broadcastPending = false;
   outgoing = Alert{};
   staged = Alert{};
   current = Alert{};
   holdTimer = 0.0;
   sentCount = 0;
   receivedCount = 0;
}

void AlertRadio::requestBroadcast(const std::string& contactName,
                                  const double northM, const double eastM, const double altM,
                                  const double contactRangeM)
{
   const models::Player* const own{getOwnship()};

   outgoing = Alert{};
   outgoing.valid = true;
   outgoing.senderId = (own != nullptr) ? own->getID() : 0;
   outgoing.senderName = (own != nullptr && own->getName() != nullptr)
                          ? own->getName()->getString() : "?";
   outgoing.contactName = contactName;
   outgoing.northM = northM;
   outgoing.eastM = eastM;
   outgoing.altitudeM = altM;
   outgoing.rangeM = contactRangeM;

   broadcastPending = true;
}

//------------------------------------------------------------------------------
// FASE 1 -- transmite. Ver o cabecalho da classe: e a mesma tecnica do
// Datalink nativo (varrer os players e chamar event() no destino).
//------------------------------------------------------------------------------
void AlertRadio::transmit(const double dt)
{
   if (dt <= 0.0 || !broadcastPending) return;
   broadcastPending = false;

   models::Player* const own{getOwnship()};
   models::WorldModel* const world{getWorldModel()};
   if (own == nullptr || world == nullptr) return;

   const base::Vec3d& ownPos{own->getPosition()};

   // A mensagem e um Object ref-contado: uma unica instancia e entregue a
   // varios destinos (cada event() faz o que precisa com ela) e no fim
   // damos unref() -- o padrao do framework para carga de evento.
   const auto msg = new TacticalAlert();
   msg->setSender(outgoing.senderId, outgoing.senderName);
   msg->setContactName(outgoing.contactName);
   msg->setPosition(outgoing.northM, outgoing.eastM, outgoing.altitudeM);
   msg->setRangeM(outgoing.rangeM);

   long sent{};

   base::PairStream* const players{world->getPlayers()};
   if (players != nullptr) {
      base::List::Item* item{players->getFirstItem()};
      while (item != nullptr) {
         const auto pair = static_cast<base::Pair*>(item->getValue());
         const auto other = dynamic_cast<models::Player*>(pair->object());
         item = item->getNext();

         if (other == nullptr || other == own || !other->isActive()) continue;
         if (other->getSide() != own->getSide()) continue;   // rede propria

         const base::Vec3d& otherPos{other->getPosition()};
         const double d{domain::distanceM(ownPos[models::Player::INORTH],
                                          ownPos[models::Player::IEAST],
                                          otherPos[models::Player::INORTH],
                                          otherPos[models::Player::IEAST])};
         if (d > rangeM) continue;

         base::Pair* const radioPair{other->findByType(typeid(AlertRadio))};
         if (radioPair == nullptr) continue;

         const auto radio = dynamic_cast<AlertRadio*>(radioPair->object());
         if (radio != nullptr) {
            radio->event(TACTICAL_ALERT_EVENT, msg);
            sent += 1;
         }
      }
      players->unref();
   }

   if (sent > 0) {
      std::lock_guard<std::mutex> lock(inboxMutex);
      sentCount += sent;
   }

   msg->unref();
}

//------------------------------------------------------------------------------
// Handler do evento -- roda na THREAD DO EMISSOR. Por isso aqui so se
// ENCENA o alerta (com fusao comutativa), nunca se mexe no alerta corrente.
//------------------------------------------------------------------------------
bool AlertRadio::onTacticalAlert(base::Object* const obj)
{
   const auto msg = dynamic_cast<const TacticalAlert*>(obj);
   if (msg == nullptr) return false;

   Alert alert;
   alert.valid = true;
   alert.senderId = msg->getSenderId();
   alert.senderName = msg->getSenderName();
   alert.contactName = msg->getContactName();
   alert.northM = msg->getNorthM();
   alert.eastM = msg->getEastM();
   alert.altitudeM = msg->getAltitudeM();
   alert.rangeM = msg->getRangeM();

   stageAlert(alert);
   return true;
}

void AlertRadio::stageAlert(const Alert& alert)
{
   std::lock_guard<std::mutex> lock(inboxMutex);

   receivedCount += 1;

   // FUSAO COMUTATIVA (ver cabecalho): vence o contato mais proximo; em
   // empate exato, o emissor de menor id. O resultado nao depende da ordem
   // em que as threads emissoras chegaram aqui.
   if (!staged.valid
       || alert.rangeM < staged.rangeM
       || (alert.rangeM == staged.rangeM && alert.senderId < staged.senderId)) {
      staged = alert;
   }
}

//------------------------------------------------------------------------------
// FASE 2 -- promove o alerta encenado a corrente e envelhece o corrente.
//------------------------------------------------------------------------------
void AlertRadio::receive(const double dt)
{
   if (dt <= 0.0) return;

   std::lock_guard<std::mutex> lock(inboxMutex);

   if (staged.valid) {
      current = staged;
      staged = Alert{};
      holdTimer = holdTimeSec;
   } else if (current.valid) {
      holdTimer -= dt;
      if (holdTimer <= 0.0) current = Alert{};
   }
}

AlertRadio::Alert AlertRadio::getAlert() const
{
   std::lock_guard<std::mutex> lock(inboxMutex);
   return current;
}

bool AlertRadio::hasAlert() const
{
   std::lock_guard<std::mutex> lock(inboxMutex);
   return current.valid;
}

long AlertRadio::getSentCount() const
{
   std::lock_guard<std::mutex> lock(inboxMutex);
   return sentCount;
}

long AlertRadio::getReceivedCount() const
{
   std::lock_guard<std::mutex> lock(inboxMutex);
   return receivedCount;
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool AlertRadio::setSlotRange(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   rangeM = base::Meters::convertStatic(*msg);
   return (rangeM > 0.0);
}

bool AlertRadio::setSlotHoldTime(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   holdTimeSec = base::Seconds::convertStatic(*msg);
   return (holdTimeSec > 0.0);
}

} // namespace xair
} // namespace mixr
