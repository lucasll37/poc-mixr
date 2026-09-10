#include "domain/PatrolPlan.hpp"

#include "domain/geometry.hpp"

//------------------------------------------------------------------------------
// O SENTIDO da curva do circuito de patrulha.
//
// Existe para a prova de recarga do modelo -- o teste 'plugin-hotswap'
// (tests/plugin/run_plugin_hotswap.py, suite 'plugin'): e a regra de negocio
// que muda quando SO o plugin e recompilado, sem tocar no .edl e sem relinkar
// o executavel. O default 1.0 e o comportamento de sempre; -1.0 inverte o
// circuito inteiro, e a divergencia aparece em TODAS as linhas 'frame=' nos
// primeiros segundos. As duas variantes saem do MESMO fonte, atras de
// '-Dvariants=true' (ver o meson.build deste projeto), nunca de um sed no
// fonte versionado.
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
   jitterRng_.reset();
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
   jitterRng_.seed(seed);
   resampleJitter();
}

void PatrolPlan::resampleJitter()
{
   // symmetric() ja trata amplitude <= 0 devolvendo 0 SEM consumir o
   // gerador -- mesma semantica do ternario que estava aqui, agora escrita
   // uma vez so, em libs/xrandom.
   currentJitterDeg_ = jitterRng_.symmetric(jitterAmplitudeDeg_);
}

} // namespace domain
