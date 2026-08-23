#ifndef __xnative_AlertDatalink_H__
#define __xnative_AlertDatalink_H__

#include "mixr/models/system/Datalink.hpp"

#include <mutex>
#include <string>

namespace mixr {
namespace base { class Time; }

namespace xnative {

//------------------------------------------------------------------------------
// Class: AlertDatalink
//
// Description: Interacao entre avioes REAPROVEITANDO o datalink nativo.
//              Herda models::Datalink e acrescenta SO o que o framework nao
//              tem como saber: o que fazer com a mensagem recebida.
//
// Factory name: AlertDatalink
//
// Slots: (herdados de Datalink -- radioId, maxRange, radioName,
//         trackManagerName) + holdTime
//    holdTime  <Time>  ! Validade do alerta recebido (default: 25 s)
//
// O QUE VEM DE GRACA DA CLASSE BASE (compare com xair::AlertRadio da poc/12,
// que fazia tudo isso a mao):
//
//   * sendMessage(Object*) -- varre os players do WorldModel, respeita o
//     alcance ('maxRange' ou o radio associado), entrega com
//     event(DATALINK_MESSAGE, msg) e ainda enfileira para a rede (DIS/HLA)
//     se houver NetIO;
//   * o handler do evento e a fila de ENTRADA ja existem
//     (queueIncomingMessage/receiveMessage), e a fila e uma
//     base::safe_queue -- ou seja, a corrida de dados entre a thread do
//     emissor e a do receptor ja esta resolvida pelo framework;
//   * Player::getDatalink() acha esta classe sozinho (busca por tipo), sem
//     nenhum updateSystemPointers() nosso.
//
// O QUE CONTINUA SENDO NOSSO -- e a licao que a poc/12 ja tinha ensinado:
// a fila resolve a CORRIDA, nao a ORDEM. Se dois avioes avisam no mesmo
// frame, a ordem de chegada na fila depende do escalonador. Por isso
// receive() (fase 2) drena a fila INTEIRA e funde os alertas de forma
// COMUTATIVA (fica o contato mais proximo; empate exato -> menor id de
// emissor), e o resultado so passa a valer no frame seguinte.
//------------------------------------------------------------------------------
class AlertDatalink : public models::Datalink
{
   DECLARE_SUBCLASS(AlertDatalink, models::Datalink)

public:
   AlertDatalink();

   struct Alert
   {
      bool valid{};
      int senderId{};
      std::string senderName;
      std::string contactName;
      double northM{};
      double eastM{};
      double altitudeM{};
      double rangeM{};
   };

   void reset() override;

   // Monta o TacticalAlert e manda pelo transporte NATIVO (sendMessage).
   void broadcastAlert(const std::string& contactName,
                       const double northM, const double eastM, const double altM,
                       const double rangeM);

   Alert getAlert() const;
   bool hasAlert() const;
   long getSentCount() const;
   long getReceivedCount() const;

protected:
   // FASE 2 -- promove o alerta encenado e envelhece o corrente
   void receive(const double dt) override;

   // Ponto de ENTRADA da mensagem, na thread do EMISSOR.
   //
   // DUAS ARMADILHAS CONFIRMADAS RODANDO ate chegar neste gancho:
   //
   //   1) drenar a fila com receiveMessage() na fase 2 nao ve mensagem
   //      nenhuma (medido: 0 alertas em 90 s, com 1113 transmissoes);
   //   2) sobrescrever queueIncomingMessage() TAMBEM nao: ele nunca e
   //      chamado na entrega local (medido: 2016 chamadas de
   //      onDatalinkMessageEvent contra 0 de queueIncomingMessage).
   //
   // O motivo esta no proprio cabecalho do Datalink: o handler default de
   // DATALINK_MESSAGE "passa as mensagens aos subcomponentes" -- ele NAO
   // enfileira. A inQueue existe para o caminho de RADIO/REDE, nao para a
   // entrega local. O gancho certo e portanto o handler do evento.
   bool onDatalinkMessageEvent(base::Object* const) override;

private:
   double holdTimeSec{25.0};

   mutable std::mutex alertMutex;
   Alert staged;
   Alert current;
   double holdTimer{};
   long sentCount{};
   long receivedCount{};

   // slot table helper methods
   bool setSlotHoldTime(const base::Time* const);
};

} // namespace xnative
} // namespace mixr

#endif
