#pragma once

// namespace ANINHADO em mixr::models::xNavstar_3 -- ver o comentario equivalente,
// mais detalhado, em models/template/docs/CONTRATO.md secao 6: um cenario
// pode carregar mais de um plugin no mesmo processo, e dois tipos com o MESMO
// nome qualificado em dois .so's distintos colidiriam sob RTLD_LOCAL.
namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace domain {

//------------------------------------------------------------------------------
// Constantes puras -- duplicadas de proposito, nao herdadas de
// mixr::base::nav::ERADM/mixr::base (domain/ tem ZERO includes do MIXR, ver
// CONTRATO.md secao 6). Sao constantes fisicas fixas, nao algo que diverge.
//------------------------------------------------------------------------------

// Raio equatorial WGS84 (m) -- o mesmo valor de mixr::base::nav::ERADM.
constexpr double EARTH_EQUATORIAL_RADIUS_M{6378137.0};

// Parametro gravitacional padrao da Terra, mu = G*M (m^3/s^2). O MIXR nao tem
// esta constante em lugar nenhum (confirmado por busca exaustiva no fonte
// vendorizado) -- mecanica orbital nao existe no framework nativo.
constexpr double MU_EARTH_M3_S2{398600.4418e9};

// Taxa de rotacao sideral da Terra (rad/s).
constexpr double EARTH_ROTATION_RATE_RAD_S{7.2921150e-5};

//------------------------------------------------------------------------------
// OrbitElements -- os quatro numeros que descrevem uma orbita CIRCULAR (a
// unica forma modelada aqui -- ver ARCHITECTURE.md, "fidelidade orbital: por
// que so circular", para o porque de excentricidade ficar de fora).
//
// altitudeM: acima de EARTH_EQUATORIAL_RADIUS_M (nao acima do nivel do mar
// local -- e' a mesma simplificacao esferica que o resto deste arquivo usa).
// Defaults sao valores REPRESENTATIVOS de uma orbita GPS/MEO moderna -- NAO
// e reivindicada precisao historica para o Navstar-3 real (SVN-3, lancado em
// 1978, sem efemeride precisa preservada e util para reproduzir aqui).
//------------------------------------------------------------------------------
struct OrbitElements
{
   double altitudeM{20180000.0};   // ~26.560 km de semi-eixo maior, MEO tipico de GPS
   double inclinationDeg{55.0};    // inclinacao tipica da constelacao GPS
   double raanDeg{0.0};            // ascensao reta do nodo ascendente em simTimeS==0
   double argLatDeg0{0.0};         // argumento de latitude (fase orbital) em simTimeS==0
};

double semiMajorAxisM(const OrbitElements& e);
double meanMotionRadPerSec(const OrbitElements& e);
double orbitalPeriodSec(const OrbitElements& e);

//------------------------------------------------------------------------------
// GroundTrack -- o ponto sub-satelite, num referencial FIXO NA TERRA (ja
// pronto para converter para ECEF e aplicar via Player::setGeocPosition()).
//
// simTimeS e' tempo simulado DECORRIDO desde a epoca do cenario (t=0
// corresponde a raanDeg/argLatDeg0 como dados), acumulado por 'dt' a cada
// frame de tempo critico -- NUNCA relogio de parede (ver o motivo em
// ARCHITECTURE.md, "determinismo").
//
// A longitude ja subtrai EARTH_ROTATION_RATE_RAD_S*simTimeS -- e' o termo que
// converte a fase orbital (calculada num referencial INERCIAL) para um
// referencial que gira junto com a Terra. Nao aplicar essa correcao de novo
// em lugar nenhum (ver ARCHITECTURE.md, armadilha 2).
//------------------------------------------------------------------------------
struct GroundTrack
{
   double latDeg{};
   double lonDeg{};
   double altM{};
};

GroundTrack groundTrack(const OrbitElements& e, double simTimeS);

// Vetor 3D PROPRIO de domain/ -- nao mixr::base::Vec3d (domain/ nao inclui
// MIXR). Quem consome (ubf/) converte para o tipo do framework na fronteira.
struct Vec3
{
   double x{};
   double y{};
   double z{};
};

//------------------------------------------------------------------------------
// eciPosition() -- a posicao Cartesiana INERCIAL (ECI-like; a esfericidade da
// Terra e o referencial nao-rotativo sao as unicas aproximacoes -- nao ha
// precessao/nutacao modeladas). Formula classica de elementos circulares para
// Cartesiano (o argumento do perigeu se funde em 'u' porque e=0):
//
//   u = argLatDeg0 + meanMotion*simTimeS   (rad)
//   x = a*(cos(RAAN)*cos(u) - sin(RAAN)*sin(u)*cos(i))
//   y = a*(sin(RAAN)*cos(u) + cos(RAAN)*sin(u)*cos(i))
//   z = a*sin(u)*sin(i)
//
// DELIBERADAMENTE SEM o termo de rotacao terrestre -- ver
// EclipseGeometry.hpp para o porque de precisar ficar separada de
// groundTrack() em vez de virar "a mesma formula menos um termo".
//------------------------------------------------------------------------------
Vec3 eciPosition(const OrbitElements& e, double simTimeS);

} // namespace domain
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
