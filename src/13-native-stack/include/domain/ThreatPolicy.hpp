#pragma once

#include "domain/FlightCommand.hpp"

namespace domain {

// Contato "cru" entregue pelo ProximitySensor -- de novo sem nenhum tipo do
// MIXR: quem traduz Player -> ThreatContact e o no da arvore, na fronteira.
struct ThreatContact
{
   double rangeM{};
   double relBearingDeg{};   // -180..180, relativo ao nariz do drone
   double deltaAltM{};       // positivo = contato acima
};

struct EvasionLimits
{
   double breakTurnDeg{110.0};   // quanto virar para longe do contato
   double climbM{250.0};         // quanto subir/descer para desconflitar
   double dashSpeedKts{160.0};   // velocidade durante a evasao
};

//------------------------------------------------------------------------------
// ThreatPolicy -- decide para onde ir quando ha um contato proximo.
//
// Regra: vira para o lado OPOSTO ao contato (se o contato esta a boreste,
// vira a bombordo) e desconflita em altitude no sentido contrario ao dele.
// Puro, deterministico e testavel sem simulacao.
//------------------------------------------------------------------------------
class ThreatPolicy
{
public:
   ThreatPolicy() = default;
   explicit ThreatPolicy(const EvasionLimits& limits) : limits_(limits) {}

   void setLimits(const EvasionLimits& limits) { limits_ = limits; }

   FlightCommand evade(const ThreatContact& contact, double ownHeadingDeg, double ownAltM) const;

private:
   EvasionLimits limits_{};
};

} // namespace domain
