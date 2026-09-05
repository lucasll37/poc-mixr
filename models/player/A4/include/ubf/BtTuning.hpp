#ifndef __xnative_ubf_BtTuning_H__
#define __xnative_ubf_BtTuning_H__

#include "domain/LaunchPolicy.hpp"

#include <cstdint>
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

   // jitter de rumo da patrulha (opcional, desligado por padrao -- ver o
   // cabecalho de domain/PatrolPlan.hpp para o "porque" do desenho e
   // shared/xrandom/DeterministicRng.hpp para a hierarquia de derivacao)
   double patrolJitterHeadingDeg{0.0};      // amplitude; 0 = recurso desligado
   std::uint64_t patrolMasterSeed{0};       // mesmo valor em todo player do cenario
   bool patrolSeedOverrideSet{false};       // true so se o slot abaixo foi de fato declarado
   std::uint64_t patrolSeedOverride{0};     // pula a derivacao por nome para ESTE player

   // retorno a base
   double rtbAltitudeM{3000.0};
   double rtbSpeedKts{400.0};
   double arrivalRadiusM{3.0 * 1852.0};   // raio que conta como "chegou"
   double fuelReserve{0.35};              // fracao que dispara o RTB

   // evasao
   double breakTurnDeg{110.0};
   double evadeClimbM{600.0};             // desconflito vertical
   double evadeSpeedKts{450.0};
   double evadeHoldSec{30.0};             // histerese: quanto a manobra vale sem contato
   double terrainClearanceM{500.0};       // piso anti-CFIT: folga sobre o terreno

   // apoio a um alerta recebido
   double supportSpeedKts{420.0};

   // envelope de lancamento do missil (so faz efeito em aviao com 'stores:'
   // declarado -- ver domain/LaunchPolicy.hpp)
   domain::LaunchEnvelope launchEnv{};
};

} // namespace xnative
} // namespace mixr

#endif
