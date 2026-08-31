#ifndef __xmsg_MsgThreshold_H__
#define __xmsg_MsgThreshold_H__

#include "xmsg/Condition.hpp"
#include "xmsg/SlotUnits.hpp"
#include "xmsg/rules/Schmitt.hpp"

#include <vector>

namespace mixr {
namespace xmsg {

//------------------------------------------------------------------------------
// Class: MsgThreshold
//
// Description: "avise quando o campo cruzar este limiar" -- com histerese e
//              tempo minimo de permanencia. Casca fina sobre rules::Schmitt.
//
// Factory name: MsgThreshold
//
// Slots:
//    above   <Number>   ! dispara acima deste valor   (exclusivo com 'below')
//    below   <Number>   ! dispara abaixo deste valor  (exclusivo com 'above')
//    clear   <Number>   ! valor de DESARME -- OBRIGATORIO
//    hold    <Time>     ! tempo continuo antes de armar (default: 0)
//
// 'clear' e obrigatorio de proposito. Sem histerese, um valor tremendo em cima
// do limiar produz uma mensagem por ciclo, e o sintoma (enxurrada) nao aponta
// para a causa (o limiar). Deixar o default "sem histerese" seria empurrar a
// armadilha para quem configura.
//
// 'hold' e o antidoto do transiente: JSBSimModel::reset() nao roda FGTrim,
// entao a aeronave comeca destrimada e os primeiros segundos tem excursoes que
// nao sao evento nenhum.
//------------------------------------------------------------------------------
class MsgThreshold : public Condition
{
   DECLARE_SUBCLASS(MsgThreshold, Condition)

public:
   MsgThreshold();

   bool prepare(int maxPlayers) override;
   bool evaluate(double dt, const Snapshot& snap, int slot) override;
   void resetSlot(int slot) override;

private:
   bool setSlotAbove(const base::Number* const);
   bool setSlotBelow(const base::Number* const);
   bool setSlotClear(const base::Number* const);
   bool setSlotHold(const base::Number* const);

   SlotValue above_, below_, clear_;
   double hold_{};

   std::vector<rules::Schmitt> state_;
};

} // namespace xmsg
} // namespace mixr

#endif
