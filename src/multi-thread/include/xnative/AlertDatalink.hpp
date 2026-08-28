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
//   * sendMessage(Object*) -- varre os players do WorldModel e entrega a
//     mensagem com event(DATALINK_MESSAGE, msg) em cada player local ativo
//     (Datalink.cpp:332-360). Com 'radioName' declarado, a mensagem vira
//     payload de uma Emission e desce a cadeia de RF inteira;
//   * a fila de SAIDA para a rede (DIS/HLA) sai pronta, se houver NetIO;
//   * Player::getDatalink() acha esta classe sozinho (busca por tipo), sem
//     nenhum updateSystemPointers() nosso.
//
// O QUE NAO VEM -- dois enganos faceis, ambos conferidos no fonte:
//
//   1) ALCANCE E LADO NAO SAO FILTRADOS neste caminho. O slot 'maxRange'
//      existe e o setter grava em noRadioMaxRange, mas sendMessage() NUNCA
//      le essa variavel: sem 'radioName', a entrega e broadcast global para
//      todo player local ativo. A poc/12 fazia os dois filtros a mao
//      (xair::AlertRadio.cpp:135 para o lado, :143 para o alcance) e eles
//      NAO foram herdados aqui. O TacticalAlert chega ate no bandit1, que e
//      'red' -- morre la so porque o intruso nao declara nenhum 'datalink:'
//      (Player::onDatalinkMessageEventPlayer so repassa se getDatalink()
//      nao for nulo). Dar um datalink ao inimigo bastaria para ele escutar
//      a esquadrilha inteira.
//   2) A FILA DE ENTRADA NAO PARTICIPA da entrega local -- ver o comentario
//      de onDatalinkMessageEvent() la embaixo, com os numeros medidos.
//
// O QUE CONTINUA SENDO NOSSO -- e a licao que a poc/12 ja tinha ensinado:
// nao ha fila no caminho local, e ainda que houvesse ela resolveria a
// CORRIDA, nao a ORDEM. Se dois avioes avisam no mesmo frame, a ordem de
// chegada depende do escalonador. Por isso onDatalinkMessageEvent() -- que
// roda na thread do EMISSOR -- apenas ENCENA o alerta, fundindo de forma
// COMUTATIVA (fica o contato mais proximo; empate exato -> menor id de
// emissor); receive() (fase 2) promove o encenado para corrente. O
// resultado independe da ordem de chegada e so passa a valer no frame
// seguinte.
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
