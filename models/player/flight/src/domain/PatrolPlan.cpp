#include "domain/PatrolPlan.hpp"

#include "domain/geometry.hpp"

//------------------------------------------------------------------------------
// O SENTIDO da curva do circuito de patrulha.
//
// Existe para a demonstracao de recarga do modelo (make check-plugin-hotswap):
// e a regra de negocio que muda quando SO o plugin e recompilado, sem tocar no
// .epp e sem relinkar o executavel. O default 1.0 e o comportamento de sempre;
// -1.0 inverte o circuito inteiro, e a divergencia aparece em TODAS as linhas
// 'frame=' nos primeiros segundos.
//
// Escolhido de proposito entre as coisas que o EDL NAO sobrescreve: 'legTurn:'
// ajusta o ANGULO da curva, nunca o sentido. Sem isso a demonstracao provaria
// so que um slot funciona, e nao que CODIGO novo entrou no processo.
//------------------------------------------------------------------------------
#ifndef POC_MODEL_TURN_SIGN
   #define POC_MODEL_TURN_SIGN 1.0
#endif

namespace domain {

void PatrolPlan::configure(const double startHeadingDeg, const double legSeconds,
                           const double turnPerLegDeg, const double altitudeM,
                           const double speedKts)
{
   startHeadingDeg_ = startHeadingDeg;
   legSeconds_ = (legSeconds > 0.0) ? legSeconds : 60.0;
   turnPerLegDeg_ = turnPerLegDeg;
   altitudeM_ = altitudeM;
   speedKts_ = speedKts;
   reset();
}

void PatrolPlan::reset()
{
   legTimer_ = 0.0;
   leg_ = 0;
   jitterRng_.seed(jitterSeed_);
   resampleJitter();
}

bool PatrolPlan::advance(const double dt)
{
   legTimer_ += dt;

   // Loop, e nao 'if': um dt maior que VARIAS pernas (ex.: um passo de
   // controle gigante, ou legSeconds configurado muito pequeno) tem de
   // trocar de perna uma vez por fronteira cruzada, nao uma vez so. Um 'if'
   // aqui deixava legTimeRemaining() NEGATIVO sempre que dt >= 2*legSeconds_
   // -- a folga so era recuperada aos poucos, numa chamada por vez, ate o
   // acumulo (legTimer_) cair de volta abaixo de legSeconds_.
   bool trocouDePerna{false};
   while (legTimer_ >= legSeconds_) {
      legTimer_ -= legSeconds_;
      leg_ += 1;
      resampleJitter();
      trocouDePerna = true;
   }
   return trocouDePerna;
}

FlightCommand PatrolPlan::command() const
{
   FlightCommand cmd;
   cmd.headingDeg = wrap360(startHeadingDeg_
                            + POC_MODEL_TURN_SIGN * turnPerLegDeg_ * static_cast<double>(leg_)
                            + currentJitterDeg_);
   cmd.altitudeM = altitudeM_;
   cmd.speedKts = speedKts_;
   return cmd;
}

void PatrolPlan::setHeadingJitter(const double amplitudeDeg, const std::uint64_t seed)
{
   jitterAmplitudeDeg_ = (amplitudeDeg > 0.0) ? amplitudeDeg : 0.0;
   jitterSeed_ = seed;
   jitterRng_.seed(jitterSeed_);
   resampleJitter();
}

void PatrolPlan::resampleJitter()
{
   currentJitterDeg_ = (jitterAmplitudeDeg_ > 0.0)
      ? std::uniform_real_distribution<double>(-jitterAmplitudeDeg_, jitterAmplitudeDeg_)(jitterRng_)
      : 0.0;
}

} // namespace domain
