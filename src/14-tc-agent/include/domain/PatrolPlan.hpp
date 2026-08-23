#pragma once

#include "domain/FlightCommand.hpp"

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
// retomada exatamente de onde parou (mesma semantica de domain::Mission da
// poc/03).
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

private:
   double startHeadingDeg_{};
   double legSeconds_{60.0};
   double turnPerLegDeg_{90.0};
   double altitudeM_{1500.0};
   double speedKts_{120.0};

   double legTimer_{};
   int leg_{};
};

//------------------------------------------------------------------------------
// RtbPlan -- retorno a base (a origem da area de jogo, por padrao).
//
// Tambem puro: recebe a posicao NED do drone e devolve o comando de voo. A
// "chegada" e um raio simples; o reabastecimento em si e estado do
// FuelSystem, nao daqui.
//------------------------------------------------------------------------------
class RtbPlan
{
public:
   RtbPlan() = default;

   void configure(double baseN, double baseE, double arrivalRadiusM,
                  double altitudeM, double speedKts);

   FlightCommand command(double ownN, double ownE, double ownHeadingDeg) const;

   bool arrived(double ownN, double ownE) const;
   double distanceToBaseM(double ownN, double ownE) const;

private:
   double baseN_{};
   double baseE_{};
   double arrivalRadiusM_{500.0};
   double altitudeM_{800.0};
   double speedKts_{140.0};
};

} // namespace domain
