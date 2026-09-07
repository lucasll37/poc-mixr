#ifndef __xmsg_MsgFeed_H__
#define __xmsg_MsgFeed_H__

#include "mixr/base/Component.hpp"

#include "xmsg/RecordWriter.hpp"
#include "xmsg/Snapshot.hpp"

#include <map>
#include <string>
#include <vector>

namespace mixr {
namespace base { class Identifier; class Number; class PairStream; class String; }
namespace simulation { class Station; }

namespace xmsg {

class MsgReport;
class MsgSink;

//------------------------------------------------------------------------------
// Class: MsgFeed
//
// Description: O amostrador. Componente da Station -- vai no slot generico
//              'components:', o mesmo onde os ( SimAgent ) ja moram.
//
// Factory name: MsgFeed
//
// Slots:
//    trackManager <Identifier|String> ! nome do TrackManager (grupo TRACK)
//    maxPlayers   <Number>            ! teto de blocos de estado (default: 64)
//    healthEvery  <Time>              ! intervalo da mensagem de saude (default: 10 s)
//    sinks        <PairStream>        ! destinos
//    messages     <PairStream>        ! as mensagens configuradas
//
// EM QUE THREAD RODA, E POR QUE ISSO E O DESENHO INTEIRO.
//
// Esta classe sobrescreve reset(), updateData() e shutdownNotification() --
// NAO sobrescreve updateTC(). Logo nao ha uma linha deste subsistema dentro do
// frame de tempo critico: o custo la e estruturalmente zero, verificavel lendo
// o codigo, e nao uma promessa.
//
// updateData() sai de Station::updateData(), na thread de quem chama
// station->updateData() -- o laco de app/RealTimeRun.cpp (10 Hz) em tempo
// real, e a thread unica de app/DeterministicRun.cpp (50 Hz) em
// -deterministic. E depois de processNetworkInputTasks(), entao a lista de
// players ja inclui quem chegou por DIS neste ciclo.
//
// POR QUE NAO PENDURADO NO PLAYER. Seria o padrao do models::CollisionDetect e
// daria 50 Hz. Mas Player::updateTC() e Player::updateData() sao os DOIS
// guardados por 'mode == ACTIVE || PRE_RELEASE', e crashNotification() faz
// setMode(CRASHED) -- um observador no player emudece exatamente na borda que
// ele existe para reportar. Preco aceito: resolucao limitada ao passo do laco.
//
// 'messages:' e 'sinks:' ficam em SLOT, nunca viram componentes: e isso que
// garante que Component::updateTC(), que so desce para a lista de componentes,
// nao tenha caminho ate eles. O preco e encaminhar reset() e shutdown a mao.
//------------------------------------------------------------------------------
class MsgFeed : public base::Component
{
   DECLARE_SUBCLASS(MsgFeed, base::Component)

public:
   MsgFeed();

   void reset() override;
   void updateData(double dt) override;

protected:
   bool shutdownNotification() override;

private:
   bool setSlotTrackManager(const base::Identifier* const);
   bool setSlotTrackManagerStr(const base::String* const);
   bool setSlotMaxPlayers(const base::Number* const);
   bool setSlotHealthEvery(const base::Number* const);
   bool setSlotSinks(base::PairStream* const);
   bool setSlotMessages(base::PairStream* const);

   simulation::Station* findStation();
   int slotFor(const std::string& playerName);
   void dispatch(const std::string& msgName);
   void emitHealth(double t);

   std::string trackManagerName_;
   int maxPlayers_{64};
   double healthEvery_{10.0};

   base::PairStream* sinksSlot_{};
   base::PairStream* messagesSlot_{};

   std::vector<MsgSink*> sinks_;
   std::vector<MsgReport*> messages_;

   std::map<std::string, int> playerSlots_;
   int nextSlot_{};
   long overCap_{};

   RecordWriter writer_;
   Snapshot snap_;

   bool ready_{};
   double sinceHealth_{};
   long emitted_{};
   long overflows_{};
};

} // namespace xmsg
} // namespace mixr

#endif
