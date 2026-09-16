#pragma once

namespace mixr {
namespace models {
namespace xA_4 {
namespace domain {

// Geometria relativa entre duas posicoes no plano tangente local (NED da
// "gaming area" do WorldModel: x=Norte, y=Leste, metros a partir do ponto
// de referencia do cenario).
//
// Tudo aqui e matematica pura: nenhuma chamada ao MIXR.

// Normaliza um angulo para (-180, 180].
double wrap180(double deg);

// Normaliza um angulo para [0, 360).
double wrap360(double deg);

// Rumo verdadeiro (graus) de (fromN,fromE) para (toN,toE).
double headingToDeg(double fromN, double fromE, double toN, double toE);

// Distancia horizontal (metros) entre dois pontos NED.
double distanceM(double fromN, double fromE, double toN, double toE);

} // namespace domain
} // namespace xA_4
} // namespace models
} // namespace mixr
