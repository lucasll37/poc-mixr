#pragma once

namespace domain {

// Geometria relativa entre duas posicoes no plano tangente local (NED da
// "gaming area" do WorldModel: x=Norte, y=Leste, metros a partir do ponto
// de referencia do cenario).
//
// Tudo aqui e matematica pura: nenhuma chamada ao MIXR. Quem le a posicao
// dos players (e converte para estes numeros) e o ProximitySensor, na
// fase 2 do frame de tempo critico.
struct RelativeGeometry
{
   double rangeM{};          // distancia horizontal (metros)
   double bearingDeg{};      // rumo verdadeiro do observador para o alvo (graus)
   double relBearingDeg{};   // marcacao relativa ao nariz do observador (-180..180)
   double deltaAltM{};       // altitude do alvo menos a do observador (metros)
};

// Normaliza um angulo para (-180, 180].
double wrap180(double deg);

// Normaliza um angulo para [0, 360).
double wrap360(double deg);

// Rumo verdadeiro (graus) de (fromN,fromE) para (toN,toE).
double headingToDeg(double fromN, double fromE, double toN, double toE);

// Distancia horizontal (metros) entre dois pontos NED.
double distanceM(double fromN, double fromE, double toN, double toE);

// Geometria completa observador -> alvo.
RelativeGeometry relativeTo(double ownN, double ownE, double ownAltM, double ownHeadingDeg,
                            double tgtN, double tgtE, double tgtAltM);

} // namespace domain
