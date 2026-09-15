#pragma once

#include <cstdint>
#include <string>

namespace mixr {
namespace models {
namespace xA_4 {

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
   // libs/xrandom/DeterministicRng.hpp para a hierarquia de derivacao)
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

   // atraso de reacao do piloto a uma ameaca de RWR (domain::
   // EvasionReactionPlan) -- estocastico, derivado do MESMO
   // patrolMasterSeed/instanceSeed acima (ver ubf::BtBehavior::
   // configurePlans(), salt proprio). Os dois defaults em 0.0 fazem o
   // recurso reagir no PROPRIO frame em que a ameaca aparece (ver
   // domain/EvasionReactionPlan.hpp), nunca desligado por completo: ao
   // contrario de slowRollStick, nao ha estado "sem RWR" que precise ser
   // preservado byte a byte -- sem ( Rwr )/( RwrTrkMgr ) no .edl,
   // snapshot().hasRwrThreat nunca fica true e o atraso nunca comeca a
   // contar (mesmo raciocinio de weaponReady em domain/WorldView.hpp).
   double evadeReactionMinDelaySec{0.0};
   double evadeReactionMaxDelaySec{0.0};

   // envelope de lancamento de missil (domain/LaunchPolicy.hpp). So' importa
   // em cenarios com 'stores:'/GuidedMissile declarado -- sem StoresMgr,
   // snapshot().weaponReady nunca fica true e LaunchEnvelopeCondition nunca
   // sucede, entao estes numeros ficam inertes (mesmo raciocinio de
   // weaponReady em domain/WorldView.hpp).
   double launchMinRangeM{500.0};
   double launchMaxRangeM{9000.0};
   double launchConeDeg{45.0};

   // slow roll (acrobacia aleatoria -- ver domain/AerobaticPlan.hpp).
   // O default de slowRollStick e ZERO, ou seja o recurso nasce DESLIGADO:
   // nenhum cenario que nao declare estes slots muda de comportamento, e o
   // dump deterministico continua byte a byte o de antes.
   double slowRollMinIntervalSec{60.0};    // piso do intervalo entre manobras
   double slowRollMaxIntervalSec{180.0};   // teto; <= ao piso vira intervalo FIXO
   double slowRollStick{0.0};              // aileron -1..1; 0 = desligado
   double slowRollTimeoutSec{20.0};        // aborta a manobra que nao fecha
   // Folga minima sobre o piso anti-CFIT (terrainClearanceM) para comecar
   // uma acrobacia -- ver bt_nodes::DecisionContext::
   // hasAerobaticAltitudeMargin(). Sem essa borda, giros sucessivos perto do
   // pior caso acumulam perda de altitude ate colidir com o terreno. O
   // default (1500 m) fica bem acima do custo medio de um giro isolado
   // (~530 m, README de sandbox/A4-6DOF-RANDOM) de proposito: com 900 m
   // ainda houve colisao em testes de longa duracao -- a saida de um giro
   // pode entrar numa recuperacao oscilante (fugoide), cujo vale pode se
   // aproximar de 900 m por conta propria, somado ao custo do proprio giro.
   // Negativo desliga a borda; zero exige estar no minimo no piso.
   double slowRollMinMarginM{1500.0};
};

} // namespace xA_4
} // namespace models
} // namespace mixr

