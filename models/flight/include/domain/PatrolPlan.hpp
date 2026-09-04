#pragma once

#include "domain/FlightCommand.hpp"

#include <cstdint>
#include <random>

namespace domain {

//------------------------------------------------------------------------------
// PatrolPlan -- circuito de patrulha ciclico.
//
// Regra de negocio pura (sem MIXR, sem BehaviorTree.CPP, testavel isolada):
// a aeronave voa uma perna por 'legSeconds' e entao vira 'turnPerLegDeg'
// graus, indefinidamente. Com turnPerLegDeg=90 o circuito e um quadrado;
// com 120, um triangulo; com 60, um hexagono.
//
// advance(dt) so e chamado pelo no Patrol da arvore -- quando o drone esta
// em RTB ou evadindo, o relogio da perna NAO corre, entao a patrulha e
// retomada exatamente de onde parou.
//
// O retorno a base e outra questao e mora em domain/RtbPlan.hpp.
//
// JITTER DE RUMO (opcional, desligado por padrao): setHeadingJitter() liga um
// pequeno offset aleatorio, resorteado a cada troca de perna -- nunca por
// 'dt', que e o que manteria o determinismo entre 1/2/4 threads de tempo
// critico (o numero de trocas de perna por player independe de quantas
// threads existem; a cadencia de dt por frame, nao). A semente que chega
// aqui via setHeadingJitter() ja e o resultado FINAL de uma hierarquia de
// derivacao (semente mestra do cenario -> hash do nome do player -> salt de
// proposito) calculada por quem chama -- ver BtBehavior::configurePlans() e
// shared/xrandom/DeterministicRng.hpp. Esta classe so semeia e sorteia; nao
// sabe de master seed, nome de player nem salt -- e por isso continua sem
// incluir nenhum header do SDK (ver o cabecalho de DeterministicRng.hpp para
// o motivo: test_domain/test_tree compilam esta classe SEM MIXR/SDK).
//------------------------------------------------------------------------------
class PatrolPlan
{
public:
   PatrolPlan() = default;

   void configure(double startHeadingDeg, double legSeconds, double turnPerLegDeg,
                  double altitudeM, double speedKts);

   void reset();

   // Integra o tempo de permanencia na perna atual; devolve true quando
   // trocou de perna neste passo.
   bool advance(double dt);

   FlightCommand command() const;

   int legIndex() const                { return leg_; }
   double legTimeRemaining() const     { return legSeconds_ - legTimer_; }

   // amplitudeDeg <= 0 desliga o jitter (currentJitterDeg_ fica sempre 0,
   // dump identico ao de hoje). 'seed' e o resultado final ja derivado --
   // ver o comentario de classe acima.
   void setHeadingJitter(double amplitudeDeg, std::uint64_t seed);

private:
   void resampleJitter();

   double startHeadingDeg_{};
   double legSeconds_{60.0};
   double turnPerLegDeg_{90.0};
   double altitudeM_{1500.0};
   double speedKts_{120.0};

   double legTimer_{};
   int leg_{};

   double jitterAmplitudeDeg_{0.0};
   std::uint64_t jitterSeed_{0};
   std::mt19937_64 jitterRng_{};
   double currentJitterDeg_{0.0};
};

} // namespace domain
