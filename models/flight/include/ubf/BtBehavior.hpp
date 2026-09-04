#ifndef __xnative_ubf_BtBehavior_H__
#define __xnative_ubf_BtBehavior_H__

#include "mixr/base/ubf/AbstractBehavior.hpp"

#include "bt/DecisionContext.hpp"
#include "domain/PatrolPlan.hpp"
#include "domain/RtbPlan.hpp"
#include "domain/ThreatPolicy.hpp"
#include "ubf/BtTuning.hpp"
#include "ubf/FlightState.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <string>

namespace mixr {
namespace base { class Angle; class Distance; class Number; class String; class Time; }

namespace xnative {

//------------------------------------------------------------------------------
// Class: BtBehavior
//
// Description: A DECISAO -- um comportamento do UBF cuja politica interna e
//              uma arvore do BehaviorTree.CPP.
//
// Factory name: BtBehavior
//
// Slots:
//    treeFile        <String>    ! XML da arvore (obrigatorio)
//    patrolHeading   <Angle>     ! Rumo da primeira perna
//    legTime         <Time>      ! Duracao de cada perna (default: 60 s)
//    legTurn         <Angle>     ! Curva ao fim de cada perna (default: 90 deg)
//    patrolAltitude  <Distance>  ! Altitude de patrulha (default: 4000 m)
//    patrolSpeed     <Number>    ! Velocidade de patrulha, kts (default: 350)
//    rtbAltitude     <Distance>  ! Altitude no retorno (default: 3000 m)
//    rtbSpeed        <Number>    ! Velocidade no retorno, kts (default: 400)
//    arrivalRadius   <Distance>  ! Raio que conta como "chegou" (default: 3 NM)
//    fuelReserve     <Number>    ! Fracao de combustivel que dispara o RTB (default: 0.35)
//    breakTurn       <Angle>     ! Quebra de evasao (default: 110 deg)
//    evadeClimb      <Distance>  ! Desconflito vertical na evasao (default: 600 m)
//    evadeSpeed      <Number>    ! Velocidade na evasao, kts (default: 450)
//    evadeHold       <Time>      ! Histerese da evasao sem contato (default: 30 s)
//    supportSpeed    <Number>    ! Velocidade ao apoiar um alerta, kts (default: 420)
//    terrainClearance <Distance> ! Folga sobre o terreno no desconflito da
//                                ! evasao -- piso anti-CFIT (default: 500 m;
//                                ! 0 desliga e volta ao piso absoluto)
//    launchMinRange  <Distance>  ! Alcance minimo de lancamento (default: 1500 m)
//    launchMaxRange  <Distance>  ! Alcance maximo de lancamento (default: 9000 m)
//    launchCone      <Angle>     ! Meio-angulo do cone de lancamento (default: 45 deg)
//    patrolJitterHeading <Angle> ! Amplitude do jitter de rumo na patrulha, sorteado a
//                                ! cada troca de perna (default: 0 deg = desligado)
//    patrolMasterSeed <Number>   ! Semente do cenario; cada player deriva a sua propria
//                                ! sequencia a partir do proprio nome (default: 0)
//    patrolSeedOverride <Number> ! Opcional: substitui a derivacao acima so para ESTE
//                                ! player (a presenca do slot e o que importa, nao o
//                                ! valor -- 0 e uma semente valida)
//
// COMO UBF E BehaviorTree.CPP SE ENCAIXAM (o ponto desta poc):
//
//   O UBF define TRES papeis -- percepcao (AbstractState), decisao
//   (AbstractBehavior) e atuacao (AbstractAction) -- mas nao diz COMO
//   decidir. Esta classe preenche o papel de decisao delegando a politica
//   a uma arvore de comportamento: genAction() tica a arvore, a arvore
//   preenche um FlightDecision, e o resultado vira uma FlightAction
//   ref-contada com o VOTO deste comportamento.
//
//   O ganho de compor os dois: a arvore concorre, por voto, com
//   comportamentos que NAO sao arvore (ver AltitudeSafetyBehavior) dentro
//   de um UbfArbiter nativo. Uma regra de seguranca simples nao precisa
//   virar um ramo da arvore -- ela vira um Behavior irmao com voto maior.
//
//   genAction() pode devolver nullptr (a arvore nao decidiu nada) -- o UBF
//   trata isso como "sem recomendacao", nao como erro.
//
// reset() NAO CHEGA AQUI: um Agent nao propaga o ciclo de componentes as
// pecas do UBF (o proprio Agent.hpp diz que as chamadas "nao sao repassadas
// ao restante do framework de comportamento"), e um comportamento dentro de
// um UbfArbiter esta a dois niveis de distancia. Por isso configurePlans()
// e chamado PREGUICOSAMENTE no primeiro genAction(), e nao so no reset() --
// sem isso os planos de voo ficam com os DEFAULTS do domain::PatrolPlan
// (rumo 0, 1500 m, 120 kts) em vez dos valores dos slots.
//------------------------------------------------------------------------------
class BtBehavior : public base::ubf::AbstractBehavior, public bt_nodes::DecisionContext
{
   DECLARE_SUBCLASS(BtBehavior, base::ubf::AbstractBehavior)

public:
   BtBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;

   void reset() override;

   // --- bt_nodes::DecisionContext: o que os nos da arvore enxergam deste
   //     comportamento (mesma thread do tick). Sao os mesmos metodos de
   //     sempre; o 'override' e o que os prende ao contrato da interface. ---
   const domain::WorldView& snapshot() const override    { return snap; }
   bt_nodes::FlightDecision& decision() override         { return currentDecision; }
   domain::PatrolPlan& patrolPlan() override             { return patrol; }
   domain::RtbPlan& rtbPlan() override                   { return rtb; }
   const domain::ThreatPolicy& threatPolicy() const override { return threat; }
   double getFrameDt() const override                    { return frameDt; }
   double getFuelReserve() const override                { return tune.fuelReserve; }
   double getSupportSpeedKts() const override            { return tune.supportSpeedKts; }
   const domain::LaunchEnvelope& launchEnvelope() const override { return tune.launchEnv; }

protected:
   bool shutdownNotification() override;

private:
   void buildTree();
   void configurePlans();

   // Percepcao -> domain::ThreatPolicy, uma vez por ciclo. E aqui que a
   // histerese da evasao envelhece (ver domain/ThreatPolicy.hpp).
   void feedThreatPolicy(double dt);

   FlightState::Snapshot snap;
   bt_nodes::FlightDecision currentDecision;

   domain::PatrolPlan patrol;
   domain::RtbPlan rtb;
   domain::ThreatPolicy threat;

   BT::BehaviorTreeFactory btFactory;
   BT::Tree tree;
   bool treeBuilt{};
   bool treeValid{};
   bool plansReady{};
   double frameDt{};

   // Tudo o que o EDL ajusta (ver ubf/BtTuning.hpp e src/ubf/BtBehaviorSlots.cpp)
   BtTuning tune;

   // slot table helper methods -- corpos em src/ubf/BtBehaviorSlots.cpp
   bool setSlotTreeFile(const base::String* const);
   bool setSlotPatrolHeading(const base::Angle* const);
   bool setSlotLegTime(const base::Time* const);
   bool setSlotLegTurn(const base::Angle* const);
   bool setSlotPatrolAltitude(const base::Distance* const);
   bool setSlotPatrolSpeed(const base::Number* const);
   bool setSlotRtbAltitude(const base::Distance* const);
   bool setSlotRtbSpeed(const base::Number* const);
   bool setSlotArrivalRadius(const base::Distance* const);
   bool setSlotFuelReserve(const base::Number* const);
   bool setSlotBreakTurn(const base::Angle* const);
   bool setSlotEvadeClimb(const base::Distance* const);
   bool setSlotEvadeSpeed(const base::Number* const);
   bool setSlotEvadeHold(const base::Time* const);
   bool setSlotSupportSpeed(const base::Number* const);
   bool setSlotTerrainClearance(const base::Distance* const);
   bool setSlotLaunchMinRange(const base::Distance* const);
   bool setSlotLaunchMaxRange(const base::Distance* const);
   bool setSlotLaunchCone(const base::Angle* const);
   bool setSlotPatrolJitterHeading(const base::Angle* const);
   bool setSlotPatrolMasterSeed(const base::Number* const);
   bool setSlotPatrolSeedOverride(const base::Number* const);
};

} // namespace xnative
} // namespace mixr

#endif
