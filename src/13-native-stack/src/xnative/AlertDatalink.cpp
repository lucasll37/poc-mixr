#include "xnative/AlertDatalink.hpp"

#include "xnative/TacticalAlert.hpp"

#include "mixr/models/player/Player.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/units/Times.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(AlertDatalink, "AlertDatalink")

BEGIN_SLOTTABLE(AlertDatalink)
   "holdTime",   // 1: validade do alerta recebido
END_SLOTTABLE(AlertDatalink)

BEGIN_SLOT_MAP(AlertDatalink)
   ON_SLOT(1, setSlotHoldTime, base::Time)
END_SLOT_MAP()

EMPTY_DELETEDATA(AlertDatalink)

AlertDatalink::AlertDatalink()
{
   STANDARD_CONSTRUCTOR()
}

void AlertDatalink::copyData(const AlertDatalink& org, const bool)
{
   BaseClass::copyData(org);

   holdTimeSec = org.holdTimeSec;
   staged = Alert{};
   current = Alert{};
   holdTimer = 0.0;
   sentCount = 0;
   receivedCount = 0;
}

void AlertDatalink::reset()
{
   BaseClass::reset();   // limpa as filas nativas

   // Sem NetIO neste cenario: nao ha por que empilhar mensagens numa fila
   // de rede que ninguem drena (a entrega local e o que importa aqui).
   setNetworkQueueEnabled(false);
   setLocalSendEnabled(true);

   std::lock_guard<std::mutex> lock(alertMutex);
   staged = Alert{};
   current = Alert{};
   holdTimer = 0.0;
   sentCount = 0;
   receivedCount = 0;
}

//------------------------------------------------------------------------------
// Transmissao -- so monta a carga; quem entrega e o Datalink nativo.
//------------------------------------------------------------------------------
void AlertDatalink::broadcastAlert(const std::string& contactName,
                                   const double northM, const double eastM, const double altM,
                                   const double rangeM)
{
   const models::Player* const own{getOwnship()};

   const auto msg = new TacticalAlert();
   msg->setSender((own != nullptr) ? own->getID() : 0,
                  (own != nullptr && own->getName() != nullptr) ? own->getName()->getString() : "?");
   msg->setContactName(contactName);
   msg->setPosition(northM, eastM, altM);
   msg->setRangeM(rangeM);

   // sendMessage() e do framework: alcance, entrega local por evento e fila
   // de rede saem prontos daqui.
   if (sendMessage(msg)) {
      std::lock_guard<std::mutex> lock(alertMutex);
      sentCount += 1;
   }

   msg->unref();
}

//------------------------------------------------------------------------------
// Entrada da mensagem -- roda na THREAD DO EMISSOR (o Datalink nativo chama
// event() diretamente no destino). Por isso aqui so se ENCENA o alerta, com
// FUSAO COMUTATIVA: vence o contato mais proximo e, em empate exato, o
// emissor de menor id. Assim o resultado nao depende da ordem de chegada,
// que e do escalonador.
//------------------------------------------------------------------------------
bool AlertDatalink::onDatalinkMessageEvent(base::Object* const msg)
{
   const auto alertMsg = dynamic_cast<const TacticalAlert*>(msg);
   if (alertMsg != nullptr) {
      std::lock_guard<std::mutex> lock(alertMutex);
      receivedCount += 1;

      if (!staged.valid
          || alertMsg->getRangeM() < staged.rangeM
          || (alertMsg->getRangeM() == staged.rangeM && alertMsg->getSenderId() < staged.senderId)) {
         staged.valid = true;
         staged.senderId = alertMsg->getSenderId();
         staged.senderName = alertMsg->getSenderName();
         staged.contactName = alertMsg->getContactName();
         staged.northM = alertMsg->getNorthM();
         staged.eastM = alertMsg->getEastM();
         staged.altitudeM = alertMsg->getAltitudeM();
         staged.rangeM = alertMsg->getRangeM();
      }
   }

   // Segue o curso normal da classe base (repasse aos subcomponentes).
   return BaseClass::onDatalinkMessageEvent(msg);
}

//------------------------------------------------------------------------------
// FASE 2 -- promove o alerta encenado e envelhece o corrente.
//
// A promocao numa FRONTEIRA DE FASE e o que da latencia FIXA de um frame
// para todos os receptores, em vez de "as vezes no mesmo frame, as vezes no
// proximo, dependendo da thread" (mesma disciplina da poc/12).
//------------------------------------------------------------------------------
void AlertDatalink::receive(const double dt)
{
   BaseClass::receive(dt);
   if (dt <= 0.0) return;

   std::lock_guard<std::mutex> lock(alertMutex);

   if (staged.valid) {
      current = staged;
      staged = Alert{};
      holdTimer = holdTimeSec;
   } else if (current.valid) {
      holdTimer -= dt;
      if (holdTimer <= 0.0) current = Alert{};
   }
}

AlertDatalink::Alert AlertDatalink::getAlert() const
{
   std::lock_guard<std::mutex> lock(alertMutex);
   return current;
}

bool AlertDatalink::hasAlert() const
{
   std::lock_guard<std::mutex> lock(alertMutex);
   return current.valid;
}

long AlertDatalink::getSentCount() const
{
   std::lock_guard<std::mutex> lock(alertMutex);
   return sentCount;
}

long AlertDatalink::getReceivedCount() const
{
   std::lock_guard<std::mutex> lock(alertMutex);
   return receivedCount;
}

bool AlertDatalink::setSlotHoldTime(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   holdTimeSec = base::Seconds::convertStatic(*msg);
   return (holdTimeSec > 0.0);
}

} // namespace xnative
} // namespace mixr
