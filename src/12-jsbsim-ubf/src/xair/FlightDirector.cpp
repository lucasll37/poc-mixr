#include "xair/FlightDirector.hpp"

#include "xair/JsbsimFlightModel.hpp"

#include "domain/geometry.hpp"

#include "mixr/models/player/Player.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Angles.hpp"

#include <algorithm>
#include <cmath>

namespace mixr {
namespace xair {

IMPLEMENT_SUBCLASS(FlightDirector, "FlightDirector")

BEGIN_SLOTTABLE(FlightDirector)
   "maxBank",         //  1
   "maxPitch",        //  2
   "bankPerDegErr",   //  3
   "pitchPerMeter",   //  4
   "altIntegral",     // 12
   "rollGain",        //  5
   "rollRateGain",    //  6
   "pitchGain",       //  7
   "pitchRateGain",   //  8
   "throttleGain",    //  9
   "aileronSign",     // 10
   "elevatorSign",    // 11
END_SLOTTABLE(FlightDirector)

BEGIN_SLOT_MAP(FlightDirector)
   ON_SLOT( 1, setSlotMaxBank,       base::Angle)
   ON_SLOT( 2, setSlotMaxPitch,      base::Angle)
   ON_SLOT( 3, setSlotBankPerDegErr, base::Number)
   ON_SLOT( 4, setSlotPitchPerMeter, base::Number)
   ON_SLOT( 5, setSlotRollGain,      base::Number)
   ON_SLOT( 6, setSlotRollRateGain,  base::Number)
   ON_SLOT( 7, setSlotPitchGain,     base::Number)
   ON_SLOT( 8, setSlotPitchRateGain, base::Number)
   ON_SLOT( 9, setSlotThrottleGain,  base::Number)
   ON_SLOT(10, setSlotAileronSign,   base::Number)
   ON_SLOT(11, setSlotElevatorSign,  base::Number)
   ON_SLOT(12, setSlotAltIntegral,   base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(FlightDirector)

namespace {

const double RAD2DEG{57.295779513082320876798154814105};

double clamp(const double v, const double lo, const double hi)
{
   if (v < lo) return lo;
   if (v > hi) return hi;
   return v;
}

}

FlightDirector::FlightDirector()
{
   STANDARD_CONSTRUCTOR()
}

void FlightDirector::copyData(const FlightDirector& org, const bool)
{
   BaseClass::copyData(org);

   command = org.command;
   commanded = org.commanded;

   maxBankDeg = org.maxBankDeg;
   maxPitchDeg = org.maxPitchDeg;
   bankPerDegErr = org.bankPerDegErr;
   pitchPerMeter = org.pitchPerMeter;
   altIntegralGain = org.altIntegralGain;
   rollGain = org.rollGain;
   rollRateGain = org.rollRateGain;
   pitchGain = org.pitchGain;
   pitchRateGain = org.pitchRateGain;
   throttleGain = org.throttleGain;
   aileronSign = org.aileronSign;
   elevatorSign = org.elevatorSign;

   aileron = org.aileron;
   elevator = org.elevator;
   rudder = org.rudder;
   throttle = org.throttle;
}

void FlightDirector::reset()
{
   BaseClass::reset();

   commanded = false;
   command = domain::FlightCommand{};
   aileron = 0.0;
   elevator = 0.0;
   rudder = 0.0;
   throttle = 0.8;
   cmdBankDeg = 0.0;
   altIntegral = 0.0;
}

JsbsimFlightModel* FlightDirector::findFlightModel()
{
   // Irmao dentro do mesmo player, achado POR TIPO -- mesma tecnica que o
   // Player nativo usa para localizar seus subsistemas.
   models::Player* const player{getOwnship()};
   if (player == nullptr) return nullptr;

   base::Pair* const pair{player->findByType(typeid(JsbsimFlightModel))};
   return (pair != nullptr) ? dynamic_cast<JsbsimFlightModel*>(pair->object()) : nullptr;
}

//------------------------------------------------------------------------------
// FASE 0 -- lei de controle: comando de alto nivel -> superficies.
//
// Malha em cascata, a mesma de um piloto automatico simples:
//    erro de rumo     -> banco desejado -> aileron  (com amortecimento por p)
//    erro de altitude -> arfagem desejada -> profundor (amortecimento por q)
//    erro de velocidade -> manete
//------------------------------------------------------------------------------
void FlightDirector::dynamics(const double dt)
{
   if (dt <= 0.0) return;

   models::Player* const player{getOwnship()};
   JsbsimFlightModel* const model{findFlightModel()};
   if (player == nullptr || model == nullptr) return;

   if (!commanded) {
      // Sem comando ainda (antes do primeiro tick da arvore): mantem o
      // estado inicial da aeronave como alvo, em vez de comandar zero.
      command.headingDeg = player->getHeadingD();
      command.altitudeM = player->getAltitudeM();
      command.speedKts = player->getTotalVelocityKts();
      commanded = true;
   }

   const double hdgDeg{player->getHeadingD()};
   const double rollDeg{player->getRollD()};
   const double pitchDeg{player->getPitchD()};
   const double altM{player->getAltitudeM()};
   const double speedKts{player->getTotalVelocityKts()};

   const base::Vec3d rates{player->getAngularVelocities()};   // rad/s no corpo
   const double rollRateDps{rates[0] * RAD2DEG};
   const double pitchRateDps{rates[1] * RAD2DEG};

   // --- lateral: rumo -> banco -> aileron ---
   const double hdgErr{domain::wrap180(command.headingDeg - hdgDeg)};
   cmdBankDeg = clamp(hdgErr * bankPerDegErr, -maxBankDeg, maxBankDeg);
   const double bankErr{cmdBankDeg - rollDeg};
   aileron = clamp(aileronSign * (bankErr * rollGain - rollRateDps * rollRateGain), -1.0, 1.0);

   // --- vertical: altitude -> arfagem -> profundor ---
   //
   // O termo INTEGRAL nao e enfeite: com proporcional puro a aeronave
   // estabiliza abaixo do alvo (o "droop" classico). Rodando dava uma
   // descida permanente de ~5 m/s com o profundor praticamente em zero --
   // a arfagem pedida (0.7 graus) era menor do que a necessaria para voo
   // nivelado (~3 graus, o proprio angulo de ataque de cruzeiro).
   const double altErr{command.altitudeM - altM};
   altIntegral += altErr * dt;

   // Anti-windup: a contribuicao do integral fica limitada a faixa util de
   // arfagem, senao ele "carrega" durante uma manobra e sobra depois.
   const double integralLimit{(altIntegralGain > 0.0) ? (maxPitchDeg / altIntegralGain) : 0.0};
   altIntegral = clamp(altIntegral, -integralLimit, integralLimit);

   double cmdPitchDeg{clamp(altErr * pitchPerMeter + altIntegral * altIntegralGain,
                            -maxPitchDeg, maxPitchDeg)};

   // Compensacao de curva: em banco, parte da sustentacao vai para o lado,
   // entao pede-se um pouco mais de nariz para nao afundar.
   const double bankRad{rollDeg / RAD2DEG};
   cmdPitchDeg += (1.0 / std::max(std::cos(bankRad), 0.2) - 1.0) * 2.0;

   const double pitchErr{cmdPitchDeg - pitchDeg};
   elevator = clamp(elevatorSign * (pitchErr * pitchGain - pitchRateDps * pitchRateGain), -1.0, 1.0);

   // --- manete: erro de velocidade ---
   const double speedErr{command.speedKts - speedKts};
   throttle = clamp(throttle + speedErr * throttleGain * dt, 0.0, 1.0);

   rudder = 0.0;   // coordenacao deixada para a estabilidade natural do modelo

   model->setControls(aileron, elevator, rudder, throttle);
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool FlightDirector::setSlotMaxBank(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   maxBankDeg = base::Degrees::convertStatic(*msg);
   return (maxBankDeg > 0.0);
}

bool FlightDirector::setSlotMaxPitch(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   maxPitchDeg = base::Degrees::convertStatic(*msg);
   return (maxPitchDeg > 0.0);
}

bool FlightDirector::setSlotBankPerDegErr(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   bankPerDegErr = msg->getDouble();
   return (bankPerDegErr > 0.0);
}

bool FlightDirector::setSlotPitchPerMeter(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   pitchPerMeter = msg->getDouble();
   return (pitchPerMeter > 0.0);
}

bool FlightDirector::setSlotAltIntegral(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   altIntegralGain = msg->getDouble();
   return (altIntegralGain >= 0.0);
}

bool FlightDirector::setSlotRollGain(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   rollGain = msg->getDouble();
   return true;
}

bool FlightDirector::setSlotRollRateGain(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   rollRateGain = msg->getDouble();
   return true;
}

bool FlightDirector::setSlotPitchGain(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   pitchGain = msg->getDouble();
   return true;
}

bool FlightDirector::setSlotPitchRateGain(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   pitchRateGain = msg->getDouble();
   return true;
}

bool FlightDirector::setSlotThrottleGain(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   throttleGain = msg->getDouble();
   return true;
}

bool FlightDirector::setSlotAileronSign(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   aileronSign = (msg->getDouble() >= 0.0) ? 1.0 : -1.0;
   return true;
}

bool FlightDirector::setSlotElevatorSign(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   elevatorSign = (msg->getDouble() >= 0.0) ? 1.0 : -1.0;
   return true;
}

} // namespace xair
} // namespace mixr
