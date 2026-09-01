#pragma once

#include "domain/FlightCommand.hpp"

namespace domain {

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
