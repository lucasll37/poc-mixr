#ifndef __xair_FlightDirector_H__
#define __xair_FlightDirector_H__

#include "mixr/models/system/System.hpp"

#include "domain/FlightCommand.hpp"

namespace mixr {
namespace base { class Angle; class Number; }

namespace xair {
class JsbsimFlightModel;

//------------------------------------------------------------------------------
// Class: FlightDirector
//
// Description: Piloto automatico proprio -- converte um comando de ALTO
//              NIVEL (rumo, altitude, velocidade) nas deflexoes de superficie
//              que o 6-DOF entende (aileron, profundor, leme, manete).
//
// Factory name: FlightDirector
//
// Slots:
//    maxBank        <Angle>   ! Banco maximo comandado (default: 45 deg)
//    maxPitch       <Angle>   ! Arfagem maxima comandada (default: 15 deg)
//    bankPerDegErr  <Number>  ! Graus de banco por grau de erro de rumo (default: 1.5)
//    pitchPerMeter  <Number>  ! Graus de arfagem por metro de erro de altitude (default: 0.02)
//    altIntegral    <Number>  ! Termo integral do laco de altitude, graus por (m.s) (default: 0.004)
//    rollGain       <Number>  ! Ganho do aileron sobre o erro de banco (default: 0.03)
//    rollRateGain   <Number>  ! Amortecimento pela taxa de rolagem (default: 0.10)
//    pitchGain      <Number>  ! Ganho do profundor sobre o erro de arfagem (default: 0.05)
//    pitchRateGain  <Number>  ! Amortecimento pela taxa de arfagem (default: 0.20)
//    throttleGain   <Number>  ! Ganho da manete sobre o erro de velocidade (default: 0.01)
//    aileronSign    <Number>  ! +1/-1 -- convencao de sinal do modelo JSBSim (default: 1)
//    elevatorSign   <Number>  ! +1/-1 -- idem (default: -1)
//
// POR QUE ESTA CLASSE EXISTE: sem herdar models::DynamicsModel, nao ha a
// interface setCommandedHeadingD()/setCommandedAltitude() do framework --
// e, mesmo que houvesse, o F4N nao tem autopiloto JSBSim proprio
// (gotcha ja documentado nas pocs 04 e 05: 'ap/heading_hold' nao existe
// nesse modelo). A traducao comando -> superficie tem que existir em
// algum lugar; aqui ela e explicita, configuravel por EDL e testavel.
//
// FASE 0: e uma malha de controle continua, entao roda junto com a fisica.
// A ORDEM em 'components:' importa -- este componente vem ANTES do
// JsbsimFlightModel, para que as deflexoes deste frame ja entrem na
// integracao deste frame.
//------------------------------------------------------------------------------
class FlightDirector : public models::System
{
   DECLARE_SUBCLASS(FlightDirector, models::System)

public:
   FlightDirector();

   void reset() override;

   // Comando de alto nivel -- escrito pela acao da UBF (fase 3)
   void setCommand(const domain::FlightCommand& cmd)   { command = cmd; commanded = true; }
   const domain::FlightCommand& getCommand() const     { return command; }
   bool hasCommand() const                             { return commanded; }

   // Saidas correntes (telemetria/diagnostico)
   double getAileron() const    { return aileron; }
   double getElevator() const   { return elevator; }
   double getThrottle() const   { return throttle; }
   double getCommandedBankDeg() const { return cmdBankDeg; }

protected:
   // FASE 0 -- lei de controle
   void dynamics(const double dt) override;

private:
   JsbsimFlightModel* findFlightModel();

   domain::FlightCommand command{};
   bool commanded{};

   double maxBankDeg{45.0};
   double maxPitchDeg{15.0};
   double bankPerDegErr{1.5};
   double pitchPerMeter{0.02};
   double altIntegralGain{0.004};
   double rollGain{0.03};
   double rollRateGain{0.10};
   double pitchGain{0.05};
   double pitchRateGain{0.20};
   double throttleGain{0.01};
   double aileronSign{1.0};
   double elevatorSign{-1.0};

   double aileron{};
   double elevator{};
   double rudder{};
   double throttle{0.8};
   double cmdBankDeg{};
   double altIntegral{};   // acumulador do laco de altitude (m.s)

   // slot table helper methods
   bool setSlotMaxBank(const base::Angle* const);
   bool setSlotMaxPitch(const base::Angle* const);
   bool setSlotBankPerDegErr(const base::Number* const);
   bool setSlotPitchPerMeter(const base::Number* const);
   bool setSlotAltIntegral(const base::Number* const);
   bool setSlotRollGain(const base::Number* const);
   bool setSlotRollRateGain(const base::Number* const);
   bool setSlotPitchGain(const base::Number* const);
   bool setSlotPitchRateGain(const base::Number* const);
   bool setSlotThrottleGain(const base::Number* const);
   bool setSlotAileronSign(const base::Number* const);
   bool setSlotElevatorSign(const base::Number* const);
};

} // namespace xair
} // namespace mixr

#endif
