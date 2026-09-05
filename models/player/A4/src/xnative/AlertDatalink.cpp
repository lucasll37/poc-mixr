#include "xnative/AlertDatalink.hpp"

#include "xboard/Board.hpp"
#include "events/EventTokens.hpp"
#include "events/payloads/EID_ALERT/TacticalAlert.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/units/Times.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(AlertDatalink, "AlertDatalink")

// clang-format off
BEGIN_SLOTTABLE(AlertDatalink)
   "holdTime",   // 1: validade do alerta recebido
END_SLOTTABLE(AlertDatalink)

BEGIN_SLOT_MAP(AlertDatalink)
   ON_SLOT(1, setSlotHoldTime, base::Time)
END_SLOT_MAP()

EMPTY_DELETEDATA(AlertDatalink)

AlertDatalink::AlertDatalink()
// clang-format on
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
// Transmissao -- monta a carga e entrega por DOIS caminhos independentes.
//------------------------------------------------------------------------------
void AlertDatalink::broadcastAlert(const std::string& contactName,
                                   const double northM, const double eastM, const double altM,
                                   const double rangeM)
{
   models::Player* const own{getOwnship()};

   const auto msg = new events::TacticalAlert();
   msg->setSender((own != nullptr) ? own->getID() : 0,
                  (own != nullptr && own->getName() != nullptr) ? own->getName()->getString() : "?");
   msg->setContactName(contactName);
   msg->setPosition(northM, eastM, altM);
   msg->setRangeM(rangeM);

   // Caminho (a) -- sendMessage() e do framework: a varredura da lista de
   // players, a entrega local por event(DATALINK_MESSAGE) e a fila de rede
   // saem prontas daqui. ALCANCE NAO -- sem 'radioName' o envio e broadcast
   // global (ver o cabecalho da classe). So alcanca player com Datalink.
   if (sendMessage(msg)) {
      std::lock_guard<std::mutex> lock(alertMutex);
      sentCount += 1;
   }

   // Caminho (b) -- broadcast DIRETO com o token proprio de events/,
   // no MESMO idioma de Datalink::sendMessage() (varre getWorldModel()->
   // getPlayers(), so player local ativo, nunca o proprio emissor). Alcanca
   // QUALQUER player, com ou sem Datalink -- e o caminho que um plugin sem
   // relacao nenhuma de compilacao com este (ex.: models/missile) usa para
   // reagir ao mesmo TacticalAlert. Ver events/README.md.
   models::WorldModel* const worldModel{getWorldModel()};
   if (worldModel != nullptr) {
      base::PairStream* players{worldModel->getPlayers()};
      if (players != nullptr) {
         base::List::Item* playerItem{players->getFirstItem()};
         while (playerItem != nullptr) {
            const auto playerPair = static_cast<base::Pair*>(playerItem->getValue());
            const auto player = static_cast<models::Player*>(playerPair->object());
            if (player->isLocalPlayer()) {
               if ((player->isActive() || player->isMode(models::Player::PRE_RELEASE)) && player != own) {
                  player->event(events::EID_ALERT, msg);
               }
               playerItem = playerItem->getNext();
            } else {
               playerItem = nullptr;   // networked ficam no fim da lista
            }
         }
         players->unref();
      }
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
   const auto alertMsg = dynamic_cast<const events::TacticalAlert*>(msg);
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
// proximo, dependendo da thread".
//------------------------------------------------------------------------------
void AlertDatalink::receive(const double dt)
{
   BaseClass::receive(dt);
   if (dt <= 0.0) return;

   Alert publicar{};
   long publicarSent{};
   long publicarRecv{};
   {
      std::lock_guard<std::mutex> lock(alertMutex);

      if (staged.valid) {
         current = staged;
         staged = Alert{};
         holdTimer = holdTimeSec;
      } else if (current.valid) {
         holdTimer -= dt;
         if (holdTimer <= 0.0) current = Alert{};
      }

      publicar = current;
      publicarSent = sentCount;
      publicarRecv = receivedCount;
   }

   // Publica no quadro de leitura, FORA do nosso mutex -- o xboard tem o dele,
   // e segurar os dois ao mesmo tempo criaria uma ordem de travamento para
   // ninguem manter.
   //
   // Aqui, na fronteira de fase, e nao nos getters: o host nao alcanca mais
   // esta classe (ela mora no plugin), e publicar uma vez por frame mantem a
   // latencia fixa que o comentario acima descreve.
   const models::Player* const owner{static_cast<const models::Player*>(findContainerByType(typeid(models::Player)))};
   if (owner != nullptr) {
      const int id{owner->getID()};
      xboard::setAlert(id, publicar.valid, publicar.senderName, publicar.contactName);
      xboard::setDatalinkCounters(id, publicarSent, publicarRecv);
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
