#ifndef __xdrone_DroneDynamics_H__
#define __xdrone_DroneDynamics_H__

#include "mixr/models/dynamics/DynamicsModel.hpp"

namespace mixr {
namespace base { class Angle; class Distance; class Number; }
namespace models { class Player; }

namespace xdrone {

//------------------------------------------------------------------------------
// Class: DroneDynamics
//
// Description: Dynamics model 3-DOF escrito do zero para esta PoC (nao e o
//              RacModel nem o JSBSimModel do framework). Integra rumo,
//              velocidade e altitude com limites de taxa, e escreve o
//              resultado no Player pelos setters nativos.
//
// Factory name: DroneDynamics
//
// Slots:
//    maxTurnRate   <Angle>     ! Taxa maxima de curva POR SEGUNDO (default: 8 deg/s)
//    maxBank       <Angle>     ! Banco em curva maxima, so atitude (default: 30 deg)
//    maxClimbRate  <Number>    ! Taxa maxima de subida/descida, m/s (default: 8)
//    maxAccel      <Number>    ! Aceleracao longitudinal maxima, m/s/s (default: 3)
//    minSpeed      <Number>    ! Velocidade minima, kts (default: 40)
//    maxSpeed      <Number>    ! Velocidade maxima, kts (default: 180)
//    cmdHeading    <Angle>     ! Rumo comandado inicial (default: o do proprio player)
//    cmdAltitude   <Distance>  ! Altitude comandada inicial (default: a do proprio player)
//    cmdSpeed      <Number>    ! Velocidade comandada inicial, kts (default: a do player)
//
// COMO ISTO SE ENCAIXA NO FRAMEWORK (o ponto da poc):
//
//  1) A classe implementa a MESMA interface que o RacModel/JSBSimModel
//     implementam (setCommandedHeadingD/Altitude/VelocityKts), entao
//     qualquer coisa que ja comande um dynamics model do MIXR -- inclusive
//     o Autopilot nativo -- comanda este tambem. Herdar o padrao e o que
//     mantem o modelo proprio "plugavel".
//
//  2) dynamics(dt) e chamado por Player::dynamics(), que por sua vez roda
//     na FASE 0 do frame de tempo critico (uma vez por frame, com o dt do
//     frame inteiro -- ver a nota do dt*4 em System/Player). Depois de
//     dynamics(), o proprio Player integra a POSICAO a partir da
//     velocidade que escrevemos aqui (Player::positionUpdate(), integracao
//     trapezoidal) -- por isso NAO chamamos setPosition*() com slaved.
//
//  3) O ponteiro para o player nao vem por slot: e o container. Mesmo
//     padrao do JSBSimModel nativo -- findContainerByType(typeid(Player)).
//------------------------------------------------------------------------------
class DroneDynamics : public models::DynamicsModel
{
   DECLARE_SUBCLASS(DroneDynamics, models::DynamicsModel)

public:
   DroneDynamics();

   // --- interface padrao de DynamicsModel (a mesma do RacModel) ---
   void dynamics(const double dt) override;
   void reset() override;

   bool isHeadingHoldOn() const override                { return hdgHold; }
   double getCommandedHeadingD() const override         { return cmdHeadingDeg; }
   bool setHeadingHoldOn(const bool b) override         { hdgHold = b; return true; }
   bool setCommandedHeadingD(const double h, const double hDps = 0, const double maxBank = 0) override;

   bool isVelocityHoldOn() const override               { return spdHold; }
   double getCommandedVelocityKts() const override      { return cmdSpeedKts; }
   bool setVelocityHoldOn(const bool b) override        { spdHold = b; return true; }
   bool setCommandedVelocityKts(const double v, const double vNps = 0) override;

   bool isAltitudeHoldOn() const override               { return altHold; }
   double getCommandedAltitude() const override         { return cmdAltitudeM; }
   bool setAltitudeHoldOn(const bool b) override        { altHold = b; return true; }
   bool setCommandedAltitude(const double a, const double aMps = 0, const double maxPitch = 0) override;

   // --- telemetria do proprio modelo (usada no status do main.cpp) ---
   double getBankAngleDeg() const                       { return bankDeg; }
   double getTurnRateDps() const                        { return turnRateDps; }

private:
   models::Player* getPlayer();

   // limites (slots)
   double maxTurnRateDps{8.0};
   double maxBankDeg{30.0};
   double maxClimbRateMps{8.0};
   double maxAccelMps2{3.0};
   double minSpeedKts{40.0};
   double maxSpeedKts{180.0};

   // comandos correntes (-9999 = "ainda nao comandado", mesmo sentinela do
   // RacModel nativo: na primeira passada adotamos o estado real do player)
   double cmdHeadingDeg{-9999.0};
   double cmdAltitudeM{-9999.0};
   double cmdSpeedKts{-9999.0};

   // copia dos comandos vindos do .epp -- reset() volta para eles (os
   // comandos correntes sao sobrescritos pela arvore de comportamento a
   // cada frame, e um RESET_EVENT tem que devolver o cenario ao inicio)
   double initCmdHeadingDeg{-9999.0};
   double initCmdAltitudeM{-9999.0};
   double initCmdSpeedKts{-9999.0};

   bool hdgHold{true};
   bool altHold{true};
   bool spdHold{true};

   // estado interno exposto como telemetria
   double bankDeg{};
   double turnRateDps{};

   // slot table helpers
   bool setSlotMaxTurnRate(const base::Angle* const);
   bool setSlotMaxBank(const base::Angle* const);
   bool setSlotMaxClimbRate(const base::Number* const);
   bool setSlotMaxAccel(const base::Number* const);
   bool setSlotMinSpeed(const base::Number* const);
   bool setSlotMaxSpeed(const base::Number* const);
   bool setSlotCmdHeading(const base::Angle* const);
   bool setSlotCmdAltitude(const base::Distance* const);
   bool setSlotCmdSpeed(const base::Number* const);
};

} // namespace xdrone
} // namespace mixr

#endif
