#pragma once

namespace orbit {

// MIXR nao tem nenhum propagador orbital nativo -- SpaceVehicle/
// SpaceDynamicsModel (mixr::models::player::space) sao stubs sem fisica
// nenhuma (confirmado lendo o fonte vendorizado). A mecanica orbital
// abaixo (2-body, Terra esferica) e 100% nossa; o que E nativo e o jeito
// de aplicar a posicao resultante num Player -- ver
// Player::setGeocPosition(ecef, /*slaved=*/true) em main.cpp, que
// desliga a integracao de posicao nativa (Player::positionUpdate()) e
// deixa cada tick ser 100% ditado por este calculo.
struct CircularOrbit
{
   double altitudeM{};       // altitude acima do raio equatorial da Terra (m)
   double inclinationDeg{};  // inclinacao do plano orbital (graus)
   double raanDeg{};         // RAAN -- longitude do nodo ascendente em t=0 (graus)
   double argLatDeg0{};      // argumento de latitude (fase orbital) em t=0 (graus)
};

// Periodo orbital (2-body): T = 2*pi*sqrt(a^3/mu)
double orbitalPeriodSec(const CircularOrbit& orbit);

// Ground track de uma orbita circular, incluindo rotacao da Terra:
//   n = sqrt(mu / a^3)                         (movimento medio)
//   u(t) = u0 + n*t                            (argumento de latitude)
//   lat = asin(sin(i)*sin(u))
//   lon = raan + atan2(cos(i)*sin(u), cos(u)) - earthRotRate*t
void groundTrack(const CircularOrbit& orbit, const double simTimeSec,
                  double* latDeg, double* lonDeg, double* altM);

} // namespace orbit
