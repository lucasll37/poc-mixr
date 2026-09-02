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
}

bool PatrolPlan::advance(const double dt)
{
   legTimer_ += dt;
   if (legTimer_ < legSeconds_) return false;

   legTimer_ -= legSeconds_;
   leg_ += 1;
   return true;
}

FlightCommand PatrolPlan::command() const
{
   FlightCommand cmd;
   cmd.headingDeg = wrap360(startHeadingDeg_
                            + POC_MODEL_TURN_SIGN * turnPerLegDeg_ * static_cast<double>(leg_));
   cmd.altitudeM = altitudeM_;
   cmd.speedKts = speedKts_;
   return cmd;
}

} // namespace domain
