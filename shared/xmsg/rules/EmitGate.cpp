#include "xmsg/rules/EmitGate.hpp"

#include "xmsg/rules/timeTolerance.hpp"

namespace mixr {
namespace xmsg {
namespace rules {

void EmitGate::configure(const double minIntervalSeconds)
{
   minInterval_ = (minIntervalSeconds > 0.0) ? minIntervalSeconds : 0.0;
   reset();
}

void EmitGate::reset()
{
   // Comeca "vencido" para a primeira emissao sair na hora. Comecar em zero
   // atrasaria toda mensagem periodica em um intervalo inteiro, sem que
   // ninguem tivesse pedido isso.
   sinceLast_ = minInterval_;
   pending_ = false;
   deferred_ = 0;
}

bool EmitGate::update(const double dt, const bool wants)
{
   sinceLast_ += dt;

   if (wants) pending_ = true;

   // Mesma tolerancia do 'hold' do Schmitt, pela mesma razao: sem ela um
   // 'every: ( Seconds 1 )' a 10 Hz emitiria a cada 11 ciclos, nao a cada 10
   // (ver rules/timeTolerance.hpp).
   const bool liberado{(minInterval_ <= 0.0) || reached(sinceLast_, minInterval_)};

   if (pending_ && liberado) {
      pending_ = false;
      sinceLast_ = 0.0;
      return true;
   }

   // Quis sair e nao saiu: e isso que a mensagem de saude reporta.
   if (wants) ++deferred_;

   return false;
}

} // namespace rules
} // namespace xmsg
} // namespace mixr
