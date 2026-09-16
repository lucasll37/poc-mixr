#pragma once

#include "domain/CircularOrbit.hpp"

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace domain {

enum class SunState { SUNLIT, ECLIPSE };

//------------------------------------------------------------------------------
// sunState() -- modelo de sombra CILINDRICA (sem penumbra): a Terra projeta
// um cilindro infinito de raio 'earthRadiusM' ao longo do eixo anti-sol. Boa
// aproximacao na escala MEO/LEO (o raio angular real do Sol, ~0,26 grau, faz
// o cone de umbra ser quase cilindrico a estas distancias).
//
// 'satPosM' e 'sunDirUnit' TEM que estar no MESMO referencial -- este modelo
// usa domain::eciPosition() (inercial) pareado com uma direcao de sol
// TAMBEM fixa nesse referencial (ver ubf::Navstar3BtBehavior). Uma "direcao
// fixa, sem estacao do ano" so e honesta num referencial INERCIAL: em ECEF a
// direcao real do sol varre uma volta completa por dia sideral, entao um
// vetor "fixo" em ECEF seria uma aproximacao PIOR, nao inofensiva. E por
// isso eciPosition() e groundTrack() sao DUAS funcoes separadas em
// CircularOrbit.hpp, nunca "a mesma formula menos um termo": alimentar o
// lat/lon fixo-na-Terra neste teste faria o sol parecer varrer ao redor da
// Terra uma vez por dia sideral relativo ao satelite -- resultado com
// aparencia plausivel, mas numericamente errado.
//
// sunDirUnit: vetor UNITARIO do centro da Terra em direcao ao Sol -- calibrar
// a normalizacao e responsabilidade de quem chama (funcao pura e total).
//
// Convencao de fronteira (perpDist == earthRadiusM): SUNLIT -- a borda conta
// como fora da sombra (mesmo espirito inclusivo de domain::ParachuteFsm, no
// modelo paratrooper deste repositorio).
//------------------------------------------------------------------------------
SunState sunState(const Vec3& satPosM, const Vec3& sunDirUnit,
                   double earthRadiusM = EARTH_EQUATORIAL_RADIUS_M);

const char* labelOf(SunState s);
bool sunStateFromLabel(const char* label, SunState& out);

} // namespace domain
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
