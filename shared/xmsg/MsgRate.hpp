#ifndef __xmsg_MsgRate_H__
#define __xmsg_MsgRate_H__

#include "xmsg/Condition.hpp"
#include "xmsg/SlotUnits.hpp"
#include "xmsg/rules/RateWindow.hpp"
#include "xmsg/rules/Schmitt.hpp"

#include <vector>

namespace mixr {
namespace xmsg {

//------------------------------------------------------------------------------
// Class: MsgRate
//
// Description: "avise quando o campo estiver mudando mais rapido que X" --
//              derivada sobre janela de tempo simulado, comparada com limiar.
//              Composicao de rules::RateWindow com rules::Schmitt.
//
// Factory name: MsgRate
//
// Slots:
//    above    <Number>   ! dispara com taxa acima  (exclusivo com 'below')
//    below    <Number>   ! dispara com taxa abaixo (exclusivo com 'above')
//    clear    <Number>   ! valor de desarme (default: o proprio limiar)
//    window   <Time>     ! janela da derivada (default: 1 s)
//    hold     <Time>     ! tempo continuo antes de armar (default: 0)
//
// Existe porque um limiar sobre o VALOR nao alcanca isto: duas aeronaves na
// mesma altitude, uma estabilizada e outra em queda, sao situacoes diferentes.
//
// UNIDADE DO LIMIAR: os limiares aqui sao TAXAS -- unidade do campo por
// segundo -- e nao ha classe de unidade do MIXR para isso. Por isso 'above',
// 'below' e 'clear' aceitam SO numero cru; um ( Meters -10 ) e recusado, e a
// recusa e o comportamento certo (ele diria "10 metros", nao "10 m/s").
//
// 'clear' e opcional aqui, ao contrario do MsgThreshold: a derivada sobre uma
// janela ja e um sinal suavizado, e a deteccao de borda sozinha ja impede a
// repeticao no plato. Quem quiser histerese ainda pode pedir.
//------------------------------------------------------------------------------
class MsgRate : public Condition
{
   DECLARE_SUBCLASS(MsgRate, Condition)

public:
   MsgRate();

   bool prepare(int maxPlayers) override;
   bool evaluate(double dt, const Snapshot& snap, int slot) override;
   void resetSlot(int slot) override;

private:
   bool setSlotAbove(const base::Number* const);
   bool setSlotBelow(const base::Number* const);
   bool setSlotClear(const base::Number* const);
   bool setSlotWindow(const base::Number* const);
   bool setSlotHold(const base::Number* const);

   SlotValue above_, below_, clear_;
   double window_{1.0};
   double hold_{};

   std::vector<rules::RateWindow> win_;
   std::vector<rules::Schmitt> state_;
};

} // namespace xmsg
} // namespace mixr

#endif
