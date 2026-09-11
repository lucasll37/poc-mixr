#include "domain/EvasionReactionPlan.hpp"

namespace domain {

void EvasionReactionPlan::configure(const double minDelaySec, const double maxDelaySec)
{
   minDelaySec_ = (minDelaySec > 0.0) ? minDelaySec : 0.0;
   maxDelaySec_ = (maxDelaySec > minDelaySec_) ? maxDelaySec : minDelaySec_;
   reset();
}

void EvasionReactionPlan::setSeed(const std::uint64_t seed)
{
   rng_.seed(seed);
   reset();
}

void EvasionReactionPlan::reset()
{
   phase_ = Phase::Idle;
   timer_ = 0.0;
   rng_.reset();
}

void EvasionReactionPlan::drawNextDelay()
{
   // Faixa degenerada (max <= min) devolve o piso SEM consumir o gerador --
   // regra de libs/xrandom::Rng::uniform(), nao um ramo escrito aqui.
   timer_ = rng_.uniform(minDelaySec_, maxDelaySec_);
}

bool EvasionReactionPlan::update(const double dt, const bool hasThreat)
{
   if (!hasThreat) {
      // Ameaca sumiu (ou nunca existiu): reseta sem credito parcial -- a
      // proxima deteccao sorteia um atraso NOVO, do zero (ver o comentario
      // de classe no header).
      phase_ = Phase::Idle;
      timer_ = 0.0;
      return false;
   }

   if (phase_ == Phase::Idle) {
      // Borda: a ameaca acabou de aparecer -- sorteia o atraso de reacao
      // UMA vez aqui, nunca por 'dt'.
      phase_ = Phase::Waiting;
      drawNextDelay();
      return false;
   }

   if (phase_ == Phase::Waiting) {
      timer_ -= dt;
      if (timer_ > 0.0) return false;

      timer_ = 0.0;
      phase_ = Phase::Reacted;
      return true;
   }

   // Phase::Reacted -- ja' reagiu; continua reagindo enquanto a ameaca
   // seguir presente (e' o caller, via 'hasThreat', quem decide se ainda ha
   // ameaca).
   return true;
}

} // namespace domain
