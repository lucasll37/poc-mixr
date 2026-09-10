#include "domain/AerobaticPlan.hpp"

#include "domain/geometry.hpp"

#include <cmath>

namespace domain {

namespace {

// Um giro completo em torno do eixo longitudinal.
constexpr double kFullRollDeg{360.0};

} // namespace

void AerobaticPlan::configure(const double minIntervalSec, const double maxIntervalSec,
                              const double stickCommand, const double timeoutSec)
{
   minIntervalSec_ = (minIntervalSec > 0.0) ? minIntervalSec : 0.0;
   maxIntervalSec_ = (maxIntervalSec > minIntervalSec_) ? maxIntervalSec : minIntervalSec_;

   // Clampa em [-1, 1] -- e' a faixa que Autopilot::setControlStickRollInput()
   // aceita (fora dela ele RECUSA o valor em silencio e mantem o anterior,
   // Autopilot.cpp:1259-1266). Clampar aqui, na regra, evita que um erro de
   // cenario vire uma manobra que nunca comeca.
   stickCommand_ = (stickCommand > 1.0) ? 1.0 : ((stickCommand < -1.0) ? -1.0 : stickCommand);

   timeoutSec_ = (timeoutSec > 0.0) ? timeoutSec : 20.0;

   reset();
}

void AerobaticPlan::setSeed(const std::uint64_t seed)
{
   rng_.seed(seed);
   reset();
}

void AerobaticPlan::reset()
{
   phase_ = Phase::Idle;
   accumulatedDeg_ = 0.0;
   lastRollDeg_ = 0.0;
   hasLastRoll_ = false;
   elapsedSec_ = 0.0;

   rng_.reset();
   drawNextInterval();
}

void AerobaticPlan::drawNextInterval()
{
   // Faixa degenerada (max <= min) devolve o piso SEM consumir o gerador --
   // regra de libs/xrandom::Rng::uniform(), nao um ramo escrito aqui. Isso
   // mantem a sequencia de sorteios estavel se alguem trocar de intervalo
   // variavel para fixo e de volta, e deixa o modo fixo 100% deterministico
   // sem depender de semente nenhuma.
   timer_ = rng_.uniform(minIntervalSec_, maxIntervalSec_);
}

double AerobaticPlan::stick() const
{
   return (phase_ == Phase::Rolling) ? stickCommand_ : 0.0;
}

bool AerobaticPlan::update(const double dt, const double rollDeg)
{
   // Recurso desligado (o default): nunca sai de Idle, nunca consome o RNG,
   // nunca produz comando. E' o que mantem os cenarios existentes com dump
   // byte-identico ao de antes deste plano existir.
   if (!enabled()) {
      phase_ = Phase::Idle;
      return false;
   }

   if (phase_ == Phase::Idle) {
      timer_ -= dt;
      if (timer_ > 0.0) return false;

      // Entra na manobra. O timer nao fica negativo: ele so volta a valer
      // quando drawNextInterval() rodar no fim desta manobra, e ate la
      // timeToNextSec() reportaria lixo.
      timer_ = 0.0;
      phase_ = Phase::Rolling;
      accumulatedDeg_ = 0.0;
      elapsedSec_ = 0.0;

      // O primeiro tick da manobra so ANCORA a referencia -- nao ha
      // diferenca a integrar ainda.
      lastRollDeg_ = rollDeg;
      hasLastRoll_ = true;
      return true;
   }

   // --- Phase::Rolling ---
   elapsedSec_ += dt;

   if (hasLastRoll_) {
      // wrap180 da DIFERENCA, nunca comparacao do angulo absoluto: o banco
      // lido vive em (-180, 180] e passa por essa descontinuidade duas vezes
      // por giro (ver o comentario de classe).
      accumulatedDeg_ += wrap180(rollDeg - lastRollDeg_);
   }
   lastRollDeg_ = rollDeg;
   hasLastRoll_ = true;

   const bool completou{std::abs(accumulatedDeg_) >= kFullRollDeg};
   const bool estourou{elapsedSec_ >= timeoutSec_};

   if (completou || estourou) {
      phase_ = Phase::Idle;
      hasLastRoll_ = false;
      drawNextInterval();   // UNICO consumo do RNG, numa borda discreta
      return false;
   }

   return true;
}

} // namespace domain
