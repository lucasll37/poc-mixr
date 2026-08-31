#ifndef __xmsg_MsgChanged_H__
#define __xmsg_MsgChanged_H__

#include "xmsg/Condition.hpp"
#include "xmsg/SlotUnits.hpp"
#include "xmsg/rules/Deadband.hpp"

#include <vector>

namespace mixr {
namespace xmsg {

//------------------------------------------------------------------------------
// Class: MsgChanged
//
// Description: "avise quando o campo mudar mais que X desde a ULTIMA VEZ QUE
//              VOCE ME AVISOU". Casca fina sobre rules::Deadband.
//
// Factory name: MsgChanged
//
// Slots:
//    by   <Number>   ! variacao minima; ausente ou <= 0 = qualquer mudanca
//
// A palavra que faz a diferenca e EMITIDO. Comparar com a amostra anterior
// daria uma mensagem por ciclo numa subida constante; comparar com a ultima
// emitida faz a taxa de saida depender da GRANDEZA, nao da taxa de amostragem.
// E o que alguem quer dizer com "me avise quando a altitude mudar".
//
// 'by' aceita numero cru (na unidade do nome do campo) ou objeto de unidade da
// dimensao certa: 'by: 100' e 'by: ( Meters 100 )' sao a mesma coisa num campo
// de distancia, e '( Seconds 100 )' e recusado.
//------------------------------------------------------------------------------
class MsgChanged : public Condition
{
   DECLARE_SUBCLASS(MsgChanged, Condition)

public:
   MsgChanged();

   bool prepare(int maxPlayers) override;
   bool evaluate(double dt, const Snapshot& snap, int slot) override;
   void resetSlot(int slot) override;

private:
   bool setSlotBy(const base::Number* const);

   SlotValue by_;
   std::vector<rules::Deadband> state_;
};

} // namespace xmsg
} // namespace mixr

#endif
