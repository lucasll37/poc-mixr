#ifndef __xair_AlertRadio_H__
#define __xair_AlertRadio_H__

#include "mixr/models/system/System.hpp"

#include <mutex>
#include <string>

namespace mixr {
namespace base { class Distance; class Time; }

namespace xair {
class TacticalAlert;

//------------------------------------------------------------------------------
// Class: AlertRadio
//
// Description: O canal pelo qual UM aviao influencia OS OUTROS. Transmite
//              um TacticalAlert (evento nativo) para os demais players no
//              alcance, e guarda o alerta recebido para a arvore de
//              comportamento consultar.
//
// Factory name: AlertRadio
//
// Slots:
//    range      <Distance>  ! Alcance da transmissao (default: 60 NM)
//    holdTime   <Time>      ! Validade do alerta recebido (default: 25 s)
//
//==============================================================================
// COMO MODELAR UM EVENTO DE UM PLAYER QUE INFLUENCIA OS DEMAIS
//==============================================================================
//
// 1) TRANSPORTE (mecanismo nativo, igual ao Datalink do framework):
//    nao existe "pub/sub" no MIXR. Quem quer alcancar outro player varre
//    WorldModel::getPlayers(), acha o componente destino e chama
//    event(TOKEN, msg) nele -- exatamente o que Datalink::sendMessage()
//    faz por dentro. O receptor declara o que aceita com
//    BEGIN_EVENT_HANDLER/ON_EVENT_OBJ. Eventos nao-tecla NAO sobem sozinhos
//    na arvore: se nao houver handler, o evento simplesmente morre.
//
// 2) QUANDO transmitir: FASE 1 do frame ("sensores transmitem"). E a fase
//    que o framework reserva para um player POR SAIDA no mundo. Transmitir
//    na fase 3 (logica) funcionaria, mas ver o item 3.
//
// 3) O PROBLEMA REAL -- determinismo. Com o pool de threads ligado, o
//    emissor escreve no receptor enquanto o receptor pode estar rodando a
//    MESMA fase em outra thread. Duas consequencias:
//
//      a) corrida de dados: resolvida com um mutex curto na entrada
//         (stageAlert), o unico lock desta poc no caminho quente;
//      b) ORDEM: se dois avioes alertam o mesmo receptor no mesmo frame, a
//         ordem de chegada depende do escalonador. Por isso a entrada NAO
//         e uma fila FIFO: e uma FUSAO COMUTATIVA -- fica o alerta de
//         menor distancia e, em empate exato, o de menor id de emissor.
//         Assim o resultado independe da ordem de chegada.
//
// 4) QUANDO passa a valer: o alerta encenado ("staged") so vira o alerta
//    corrente na FASE 2 (receive) do frame SEGUINTE. Isso da uma latencia
//    fixa de um frame para todo mundo, em vez de "as vezes no mesmo frame,
//    as vezes no proximo, dependendo da thread". Latencia constante e
//    determinismo valem mais do que um frame de rapidez.
//
// E a mesma disciplina que o framework usa entre as suas 4 fases: escreve
// numa fase, publica na fronteira, le na fase seguinte.
//==============================================================================
class AlertRadio : public models::System
{
   DECLARE_SUBCLASS(AlertRadio, models::System)

public:
   AlertRadio();

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
   bool event(const int event, base::Object* const obj = nullptr) override;

   // Pedido de transmissao -- vem da acao da UBF (fase 3); a transmissao em
   // si acontece na fase 1 do frame seguinte.
   void requestBroadcast(const std::string& contactName,
                         const double northM, const double eastM, const double altM,
                         const double rangeM);

   Alert getAlert() const;
   bool hasAlert() const;
   long getSentCount() const;
   long getReceivedCount() const;

protected:
   // FASE 1 -- transmite para os demais players
   void transmit(const double dt) override;

   // FASE 2 -- promove o alerta encenado e envelhece o corrente
   void receive(const double dt) override;

private:
   bool onTacticalAlert(base::Object* const);
   void stageAlert(const Alert&);

   double rangeM{60.0 * 1852.0};
   double holdTimeSec{25.0};

   // pedido de transmissao (so a propria thread do player escreve)
   bool broadcastPending{};
   Alert outgoing;

   // entrada: escrita por OUTRAS threads (emissores), lida na fase 2
   mutable std::mutex inboxMutex;
   Alert staged;
   Alert current;
   double holdTimer{};
   long sentCount{};
   long receivedCount{};

   // slot table helper methods
   bool setSlotRange(const base::Distance* const);
   bool setSlotHoldTime(const base::Time* const);
};

} // namespace xair
} // namespace mixr

#endif
