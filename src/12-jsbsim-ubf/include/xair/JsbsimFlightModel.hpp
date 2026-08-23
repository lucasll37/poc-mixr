#ifndef __xair_JsbsimFlightModel_H__
#define __xair_JsbsimFlightModel_H__

#include "mixr/models/system/System.hpp"

#include <mutex>
#include <string>

namespace JSBSim { class FGFDMExec; }

namespace mixr {
namespace base { class Number; class String; }

namespace xair {

//------------------------------------------------------------------------------
// Class: JsbsimFlightModel
//
// Description: Dinamica de voo 6-DOF de verdade, integrada DIRETO com a
//              biblioteca JSBSim (JSBSim::FGFDMExec), escrita do zero para
//              esta PoC.
//
// Factory name: JsbsimFlightModel
//
// Slots:
//    rootDir      <String>   ! Raiz dos dados JSBSim (aircraft/, engine/, systems/)
//    model        <String>   ! Nome da aeronave (subpasta de aircraft/), ex.: "F4N"
//    trimOnReset  <Number>   ! !=0 => roda o trim do JSBSim no reset (default: 1)
//    debugLevel   <Number>   ! debug_lvl do JSBSim (default: 0)
//    subSteps     <Number>   ! passos de integracao por frame (default: 4)
//
// POR QUE ISTO NAO E UM models::DynamicsModel (pedido explicito):
//
//   O framework tem o seu proprio adaptador de JSBSim
//   (models::JSBSimModel : AerodynamicsModel : DynamicsModel). Aqui NADA
//   disso e herdado: esta classe e um models::System comum que fala com a
//   FGFDMExec por conta propria. O contrato que sobra com o MIXR e apenas:
//
//     1) rodar na FASE 0 do frame (System::dynamics), e
//     2) escrever o resultado no Player com os setters publicos.
//
//   Consequencias que valem entender (sao o "preco" de nao herdar):
//
//   a) O Player fica com getDynamicsModel() == nullptr. Player::dynamics()
//      tolera isso (so pula a chamada) e segue chamando positionUpdate().
//   b) Por isso escrevemos a posicao com setPositionLLA(..., slaved=true):
//      o flag 'slaved' desliga a integracao de posicao nativa, deixando a
//      posicao 100% ditada pelo JSBSim. E o MESMO mecanismo que a poc/10
//      usa para os satelites e que o NetIO nativo usa para entidades
//      remotas -- nao e um truque nosso.
//   c) Quem quiser comandar rumo/altitude/velocidade NAO tem a interface
//      setCommandedHeadingD()/... do DynamicsModel: quem traduz comando de
//      alto nivel em deflexao de superficie e o FlightDirector desta poc
//      (que, alias, e o que a poc/04 mostrou ser necessario de qualquer
//      forma -- o F4N nao tem autopiloto JSBSim proprio).
//
// ENTRADAS: aileron/elevator/rudder/throttle normalizados (-1..1 / 0..1),
// escritos pelo FlightDirector na mesma fase 0, ANTES desta classe (a ordem
// dos irmaos em 'components:' e a ordem de atualizacao -- ver scenario.epp).
//
// THREADS: cada aeronave tem a SUA FGFDMExec. A CARGA do modelo (parse de
// XML) e serializada por um mutex estatico; o passo de integracao (Run())
// e por instancia e roda em paralelo sem lock.
//------------------------------------------------------------------------------
class JsbsimFlightModel : public models::System
{
   DECLARE_SUBCLASS(JsbsimFlightModel, models::System)

public:
   JsbsimFlightModel();

   void reset() override;

   // --- entradas de controle (escritas pelo FlightDirector) ---
   void setControls(const double aileron, const double elevator,
                    const double rudder, const double throttle);

   // --- telemetria vinda do 6-DOF (nao existe sem fisica de verdade) ---
   double getAlphaDeg() const     { return alphaDeg; }
   double getBetaDeg() const      { return betaDeg; }
   double getMach() const         { return mach; }
   double getGLoad() const        { return gLoad; }
   double getFuelLbs() const      { return fuelLbs; }
   double getFuelFraction() const;
   bool isRunning() const         { return fdm != nullptr; }

protected:
   // FASE 0 -- um passo de integracao do JSBSim
   void dynamics(const double dt) override;

   bool shutdownNotification() override;

private:
   bool createFdm();
   void applyInitialConditions();
   void publishToPlayer();

   JSBSim::FGFDMExec* fdm{};

   std::string rootDir{"./poc/04-jsbsim-6dof/data/jsbsim/"};
   std::string modelName{"F4N"};
   bool trimOnReset{true};
   int debugLevel{};
   int subSteps{4};

   // entradas correntes (escritas na fase 0 pelo FlightDirector, lidas aqui
   // na mesma fase, na mesma thread do player)
   double aileronCmd{};
   double elevatorCmd{};
   double rudderCmd{};
   double throttleCmd{0.8};

   // telemetria publicada
   double alphaDeg{};
   double betaDeg{};
   double mach{};
   double gLoad{1.0};
   double fuelLbs{};
   double fuelCapacityLbs{};

   // slot table helper methods
   bool setSlotRootDir(const base::String* const);
   bool setSlotModel(const base::String* const);
   bool setSlotTrimOnReset(const base::Number* const);
   bool setSlotDebugLevel(const base::Number* const);
   bool setSlotSubSteps(const base::Number* const);
};

} // namespace xair
} // namespace mixr

#endif
