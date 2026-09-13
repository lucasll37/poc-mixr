#ifndef __models_xBeacon_Beacon_H__
#define __models_xBeacon_Beacon_H__

#include "mixr/models/player/Player.hpp"

#include "events/payloads/EID_PING/PingMessage.hpp"

#include <string>

namespace mixr {
namespace base { class String; class Time; }

namespace models {
namespace xBeacon {

//------------------------------------------------------------------------------
// Class: Beacon
//
// Description: modelo de EXERCICIO -- um mixr::models::Player MINIMO
//              (mesmo padrao de mixr::models::Building: Player direto, so'
//              getMajorType() sobrescrito) que EMITE e TRATA o proprio
//              evento (events::EID_PING/events::PingMessage -- ver
//              models/events/README.md). Sem dynamicsModel, sem pilot, sem
//              sensor: o proposito nao e' voar, e' exercitar a convencao de
//              evento do MIXR de ponta a ponta.
//
// Factory name: Beacon
//
// Slots:
//    pingInterval  <Time>    ! intervalo entre um ping e o proximo (default: 5 s)
//    pingMessage   <String>  ! texto levado no payload de cada ping (default: "ping")
//
// OS DOIS PASSOS DE models/events/README.md, na MESMA classe:
//
//   1. EMITIR -- broadcastPing(), chamado de dentro de updateData() (fase de
//      FUNDO, 10 Hz -- Simulation::updateData() e' sequencial, uma thread
//      so', nunca corre em paralelo com outro player: ao contrario de
//      xnative::AlertDatalink::broadcastAlert() (models/players/A-4, chamado
//      de dentro da decisao na fase 3, um player por thread do pool de
//      tempo critico), Beacon nao precisa de mutex nenhum para os proprios
//      contadores -- so' ha UM emissor por vez no processo inteiro).
//   2. TRATAR -- onPingEvent(), o handler que BEGIN_EVENT_HANDLER registra
//      para events::EID_PING (ver Beacon.cpp). Roda na THREAD DO EMISSOR (o
//      mesmo Component::event() sincrono que todo evento nativo do MIXR usa
//      -- RESET_EVENT, KILL_EVENT, etc.), nao na do receptor.
//
// Alcance/lado NAO sao filtrados (mesmo caminho (b) de
// xnative::AlertDatalink::broadcastAlert() -- ver o cabecalho dela): todo
// Beacon LOCAL ativo, exceto o proprio emissor, recebe todo ping.
//------------------------------------------------------------------------------
class Beacon final : public Player
{
   DECLARE_SUBCLASS(Beacon, Player)

public:
   Beacon();

   unsigned int getMajorType() const override    { return BUILDING; }

   unsigned int getSentCount() const              { return sentCount; }
   unsigned int getReceivedCount() const          { return receivedCount; }
   unsigned int getLastSequenceReceived() const   { return lastSequenceReceived; }
   const std::string& getLastSenderName() const   { return lastSenderName; }
   const std::string& getLastMessage() const      { return lastMessage; }

   void reset() override;
   void updateData(const double dt = 0.0) override;
   bool event(const int event, base::Object* const obj = nullptr) override;

protected:
   void broadcastPing();
   bool onPingEvent(events::PingMessage* const);

private:
   // slot table helper methods
   bool setSlotPingInterval(const base::Time* const);
   bool setSlotPingMessage(const base::String* const);

   double pingIntervalSec{5.0};
   std::string pingMessage{"ping"};

   double pingTimer{};
   unsigned int sequence{};
   unsigned int sentCount{};

   unsigned int receivedCount{};
   unsigned int lastSequenceReceived{};
   std::string lastSenderName;
   std::string lastMessage;
};

} // namespace xBeacon
} // namespace models
} // namespace mixr

#endif
