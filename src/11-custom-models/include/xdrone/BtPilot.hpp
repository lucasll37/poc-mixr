#ifndef __xdrone_BtPilot_H__
#define __xdrone_BtPilot_H__

#include "mixr/models/system/System.hpp"

#include "domain/PatrolPlan.hpp"
#include "domain/ThreatPolicy.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <atomic>
#include <mutex>
#include <string>

namespace mixr {
namespace base { class Angle; class Distance; class Number; class String; class Time; }

namespace xdrone {

//------------------------------------------------------------------------------
// Class: BtPilot
//
// Description: Subsistema que HOSPEDA uma arvore de comportamento
//              (BehaviorTree.CPP v3) e a tica DENTRO do frame de tempo
//              critico do MIXR.
//
// Factory name: BtPilot
//
// Slots:
//    treeFile        <String>    ! XML da arvore (obrigatorio)
//    patrolHeading   <Angle>     ! Rumo da primeira perna da patrulha
//    legTime         <Time>      ! Duracao de cada perna (default: 45 s)
//    legTurn         <Angle>     ! Curva ao fim de cada perna (default: 90 deg)
//    patrolAltitude  <Distance>  ! Altitude de patrulha (default: 1500 m)
//    patrolSpeed     <Number>    ! Velocidade de patrulha, kts (default: 110)
//    rtbAltitude     <Distance>  ! Altitude no retorno (default: 800 m)
//    rtbSpeed        <Number>    ! Velocidade no retorno, kts (default: 140)
//    arrivalRadius   <Distance>  ! Raio que conta como "chegou na base" (default: 600 m)
//    breakTurn       <Angle>     ! Quebra de evasao (default: 110 deg)
//    evadeClimb      <Distance>  ! Desconflito vertical na evasao (default: 250 m)
//    evadeSpeed      <Number>    ! Velocidade na evasao, kts (default: 160)
//
// POR QUE ISTO E UM System, E NAO CODIGO NO main.cpp (o ponto da poc):
//
//  1) System::process(dt) e a FASE 3 do frame ("logica e controle"), que
//     roda depois de a fisica (fase 0) e os sensores (fases 1 e 2) terem
//     terminado -- ou seja, a arvore decide sobre um estado do mundo ja
//     estabilizado e igual para todos os players. Ticar a arvore no laco
//     de background (como faz a poc/05) decide sobre um estado que pode
//     estar sendo reescrito pela thread T/C no mesmo instante.
//
//  2) Como o tick roda dentro da travessia da lista de players, ele e
//     distribuido pelo pool NATIVO de threads (slot 'numTcThreads' de
//     simulation::Simulation): com N threads, as arvores dos players rodam
//     de fato em paralelo, round-robin. Cada BtPilot registra em qual
//     thread/nucleo foi ticado (threadTag()/currentCpu()) para que isso
//     seja observavel no status impresso pelo main.cpp.
//
//  3) Cada player tem a SUA arvore, a SUA factory e o SEU NodeContext --
//     nada e compartilhado entre players, que e o que torna o paralelismo
//     seguro sem nenhum lock nosso no caminho quente. As dependencias dos
//     nos entram pelo construtor (registerBuilder), nao pelo blackboard --
//     ver bt/NodeContext.hpp.
//
//  4) DETERMINISMO: o tick e funcao pura do estado do mundo daquele frame.
//     Nada aqui le relogio, sorteia ou depende de qual thread executou --
//     por isso 'make check-custom-models' produz o MESMO estado com 1, 2
//     ou 4 threads T/C. threadTag()/currentCpu() sao gravados so para
//     observabilidade e nunca realimentam a decisao.
//
// A arvore e construida preguicosamente no primeiro process() (e nao no
// reset()) porque so ai o ownship e os subsistemas irmaos ja estao
// resolvidos; a construcao e serializada por um mutex estatico porque
// varios BtPilot podem chegar nesse ponto simultaneamente, em threads
// diferentes, e BT::BehaviorTreeFactory::createTreeFromFile() nao e
// reentrante.
//------------------------------------------------------------------------------
class BtPilot : public models::System
{
   DECLARE_SUBCLASS(BtPilot, models::System)

public:
   BtPilot();

   void reset() override;

   // Usados pelos nos da arvore (todos rodam na mesma thread do tick)
   domain::PatrolPlan& patrolPlan()               { return patrol; }
   domain::RtbPlan& rtbPlan()                     { return rtb; }
   const domain::ThreatPolicy& threatPolicy() const { return threat; }

   void setState(const std::string& newState);
   std::string getState() const;

   // dt do frame corrente, para os nos que integram tempo (patrulha).
   // So e valido durante o tick, que roda na mesma thread do process().
   double getFrameDt() const                      { return frameDt; }

   long getTickCount() const     { return tickCount.load(std::memory_order_relaxed); }
   int getLastThreadTag() const  { return lastThreadTag.load(std::memory_order_relaxed); }
   int getLastCpu() const        { return lastCpu.load(std::memory_order_relaxed); }

protected:
   // FASE 3 -- tick da arvore de comportamento
   void process(const double dt) override;

   // Convencao do framework (models/System.hpp, nota 3): soltar no
   // SHUTDOWN_EVENT o que a classe criou -- aqui, a arvore.
   bool shutdownNotification() override;

private:
   void buildTree();
   void configurePlans();

   // planos/regras puros (domain/, sem MIXR e sem BT)
   domain::PatrolPlan patrol;
   domain::RtbPlan rtb;
   domain::ThreatPolicy threat;

   // arvore de comportamento desta aeronave
   BT::BehaviorTreeFactory btFactory;
   BT::Tree tree;
   bool treeBuilt{};
   bool treeValid{};
   double frameDt{};

   // slots
   std::string treeFile;
   double patrolHeadingDeg{};
   double legTimeSec{45.0};
   double legTurnDeg{90.0};
   double patrolAltitudeM{1500.0};
   double patrolSpeedKts{110.0};
   double rtbAltitudeM{800.0};
   double rtbSpeedKts{140.0};
   double arrivalRadiusM{600.0};
   double breakTurnDeg{110.0};
   double evadeClimbM{250.0};
   double evadeSpeedKts{160.0};

   // observabilidade (escrito na thread T/C, lido no laco de background)
   std::atomic<long> tickCount{};
   std::atomic<int> lastThreadTag{-1};
   std::atomic<int> lastCpu{-1};

   mutable std::mutex stateMutex;
   std::string state{"INIT"};

   // slot table helpers
   bool setSlotTreeFile(const base::String* const);
   bool setSlotPatrolHeading(const base::Angle* const);
   bool setSlotLegTime(const base::Time* const);
   bool setSlotLegTurn(const base::Angle* const);
   bool setSlotPatrolAltitude(const base::Distance* const);
   bool setSlotPatrolSpeed(const base::Number* const);
   bool setSlotRtbAltitude(const base::Distance* const);
   bool setSlotRtbSpeed(const base::Number* const);
   bool setSlotArrivalRadius(const base::Distance* const);
   bool setSlotBreakTurn(const base::Angle* const);
   bool setSlotEvadeClimb(const base::Distance* const);
   bool setSlotEvadeSpeed(const base::Number* const);
};

} // namespace xdrone
} // namespace mixr

#endif
