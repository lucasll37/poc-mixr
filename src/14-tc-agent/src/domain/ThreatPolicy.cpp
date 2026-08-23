#include "domain/ThreatPolicy.hpp"

#include "domain/geometry.hpp"

namespace domain {

FlightCommand ThreatPolicy::evade(const ThreatContact& contact, const double ownHeadingDeg,
                                  const double ownAltM) const
{
   FlightCommand cmd;

   // Vira para o lado oposto ao contato. relBearing > 0 => contato a
   // boreste => quebra para bombordo (subtrai graus do rumo).
   const double turn{(contact.relBearingDeg >= 0.0) ? -limits_.breakTurnDeg : limits_.breakTurnDeg};
   cmd.headingDeg = wrap360(ownHeadingDeg + turn);

   // Desconflita em altitude no sentido contrario ao do contato.
   cmd.altitudeM = (contact.deltaAltM >= 0.0) ? (ownAltM - limits_.climbM)
                                              : (ownAltM + limits_.climbM);
   if (cmd.altitudeM < 200.0) cmd.altitudeM = 200.0;   // piso simples de seguranca

   cmd.speedKts = limits_.dashSpeedKts;
   return cmd;
}

} // namespace domain
