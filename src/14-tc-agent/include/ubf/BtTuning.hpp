#ifndef __xnative_ubf_BtTuning_H__
#define __xnative_ubf_BtTuning_H__

#include <string>

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Os numeros que o cenario ajusta no BtBehavior -- e SO eles.
//
// Uma unica questao: reunir num lugar tudo o que vem do EDL, com os defaults
// visiveis lado a lado. Nao ha logica aqui e nao ha tipo do MIXR: os slots
// (que sao a fronteira com o EDL) convertem unidade e escrevem nestes
// campos -- ver src/ubf/BtBehaviorSlots.cpp.
//
// O ganho pratico: BtBehavior::copyData() copia UM membro em vez de
// quatorze, e acrescentar um parametro novo passa a ser uma linha aqui mais
// uma no slot -- sem risco de esquecer a copia.
//
// Unidades explicitas no nome do campo (a armadilha classica desta PoC).
//------------------------------------------------------------------------------
struct BtTuning
{
   std::string treeFile;               // XML da arvore (obrigatorio)

   // patrulha
   double patrolHeadingDeg{};          // rumo da primeira perna
   double legTimeSec{60.0};            // duracao de cada perna
   double legTurnDeg{90.0};            // curva ao fim de cada perna
   double patrolAltitudeM{4000.0};
   double patrolSpeedKts{350.0};

   // retorno a base
   double rtbAltitudeM{3000.0};
   double rtbSpeedKts{400.0};
   double arrivalRadiusM{3.0 * 1852.0};   // raio que conta como "chegou"
   double fuelReserve{0.35};              // fracao que dispara o RTB

   // evasao
   double breakTurnDeg{110.0};
   double evadeClimbM{600.0};             // desconflito vertical
   double evadeSpeedKts{450.0};

   // apoio a um alerta recebido
   double supportSpeedKts{420.0};
};

} // namespace xnative
} // namespace mixr

#endif
