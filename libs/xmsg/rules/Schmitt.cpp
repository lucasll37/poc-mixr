#include "xmsg/rules/Schmitt.hpp"

#include "xmsg/rules/timeTolerance.hpp"

namespace mixr {
namespace xmsg {
namespace rules {

bool Schmitt::configure(const Sense sense, const double trip, const double clear,
                        const double holdSeconds)
{
   sense_ = sense;
   trip_ = trip;
   clear_ = clear;
   hold_ = (holdSeconds > 0.0) ? holdSeconds : 0.0;
   reset();

   // A histerese pode ser DEGENERADA (clear == trip): "sem histerese" e uma
   // escolha legitima -- a deteccao de borda sozinha ja impede a repeticao
   // enquanto a condicao continua valendo. O que se recusa e histerese
   // APONTANDO PARA O LADO ERRADO, que nao e escolha, e bug: o gatilho
   // desarmaria junto com o armar e dispararia a cada ciclo na fronteira, com
   // um sintoma (enxurrada de mensagens) que nao aponta para a causa.
   //
   // Quem exige 'clear:' de verdade e a camada do EDL, no MsgThreshold, onde
   // o usuario esta escrevendo um limiar sobre um sinal cru.
   if (sense_ == Sense::Above) return clear_ <= trip_;
   return clear_ >= trip_;
}

void Schmitt::reset()
{
   active_ = false;
   candidate_ = false;
   holdTimer_ = 0.0;
}

//------------------------------------------------------------------------------
// raw() -- a condicao instantanea, ja com histerese.
//
// Armado e desarmado usam limiares DIFERENTES, e e so isso que a histerese e:
// enquanto ativo, so 'clear' desarma; enquanto inativo, so 'trip' arma.
//------------------------------------------------------------------------------
bool Schmitt::raw(const double value) const
{
   if (sense_ == Sense::Above) {
      return active_ ? (value > clear_) : (value > trip_);
   }
   return active_ ? (value < clear_) : (value < trip_);
}

bool Schmitt::update(const double dt, const double value, const bool valid)
{
   // Campo indisponivel: congela tudo. Nao avalia, nao envelhece o debounce,
   // nao muda o nivel -- e, principalmente, nao gera borda.
   if (!valid) return false;

   const bool now{raw(value)};

   if (active_) {
      // So o desarme interessa aqui; a borda de subida ja aconteceu.
      if (!now) reset();
      return false;
   }

   if (!now) {
      // Saiu do lado ligado antes de cumprir o 'hold': o transiente morreu,
      // e o relogio recomeca do zero na proxima vez.
      candidate_ = false;
      holdTimer_ = 0.0;
      return false;
   }

   // Do lado ligado, ainda nao armado: cumprir o tempo minimo.
   if (!candidate_) {
      candidate_ = true;
      holdTimer_ = 0.0;
   }
   holdTimer_ += dt;

   // Com tolerancia: 'hold' cumprido em N passos de dt tem de armar no passo
   // N, e nao no N+1 por causa da deriva da soma (ver rules/timeTolerance.hpp).
   if (!reached(holdTimer_, hold_)) return false;

   active_ = true;
   candidate_ = false;
   holdTimer_ = 0.0;
   return true;      // <- a borda, a unica coisa que esta classe emite
}

} // namespace rules
} // namespace xmsg
} // namespace mixr
