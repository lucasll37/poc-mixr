#include "xdrone/DroneDynamics.hpp"

#include "domain/geometry.hpp"

#include "mixr/models/player/Player.hpp"

#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/distance_utils.hpp"

#include <algorithm>
#include <cmath>

namespace mixr {
namespace xdrone {

IMPLEMENT_SUBCLASS(DroneDynamics, "DroneDynamics")

BEGIN_SLOTTABLE(DroneDynamics)
   "maxTurnRate",    // 1: taxa maxima de curva (por segundo)
   "maxBank",        // 2: banco em curva maxima
   "maxClimbRate",   // 3: m/s
   "maxAccel",       // 4: m/s/s
   "minSpeed",       // 5: kts
   "maxSpeed",       // 6: kts
   "cmdHeading",     // 7: rumo comandado inicial
   "cmdAltitude",    // 8: altitude comandada inicial
   "cmdSpeed",       // 9: velocidade comandada inicial (kts)
END_SLOTTABLE(DroneDynamics)

BEGIN_SLOT_MAP(DroneDynamics)
   ON_SLOT(1, setSlotMaxTurnRate,  base::Angle)
   ON_SLOT(2, setSlotMaxBank,      base::Angle)
   ON_SLOT(3, setSlotMaxClimbRate, base::Number)
   ON_SLOT(4, setSlotMaxAccel,     base::Number)
   ON_SLOT(5, setSlotMinSpeed,     base::Number)
   ON_SLOT(6, setSlotMaxSpeed,     base::Number)
   ON_SLOT(7, setSlotCmdHeading,   base::Angle)
   ON_SLOT(8, setSlotCmdAltitude,  base::Distance)
   ON_SLOT(9, setSlotCmdSpeed,     base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(DroneDynamics)

namespace {

// Nos -> m/s reaproveitando a constante do framework (NM2M), em vez de
// espalhar 0.5144 pelo codigo.
const double KTS2MPS{base::distance::NM2M / 3600.0};
const double DEG2RAD{0.017453292519943295769236907684886};

// Ganho proporcional do laco de altitude: taxa de subida desejada (m/s)
// por metro de erro. 0.15 converge suave sem oscilar com maxClimbRate=8.
const double ALT_GAIN{0.15};

double clamp(const double v, const double lo, const double hi)
{
   if (v < lo) return lo;
   if (v > hi) return hi;
   return v;
}

}

DroneDynamics::DroneDynamics()
{
   STANDARD_CONSTRUCTOR()
}

void DroneDynamics::copyData(const DroneDynamics& org, const bool)
{
   BaseClass::copyData(org);

   maxTurnRateDps = org.maxTurnRateDps;
   maxBankDeg = org.maxBankDeg;
   maxClimbRateMps = org.maxClimbRateMps;
   maxAccelMps2 = org.maxAccelMps2;
   minSpeedKts = org.minSpeedKts;
   maxSpeedKts = org.maxSpeedKts;

   cmdHeadingDeg = org.cmdHeadingDeg;
   cmdAltitudeM = org.cmdAltitudeM;
   cmdSpeedKts = org.cmdSpeedKts;

   initCmdHeadingDeg = org.initCmdHeadingDeg;
   initCmdAltitudeM = org.initCmdAltitudeM;
   initCmdSpeedKts = org.initCmdSpeedKts;

   hdgHold = org.hdgHold;
   altHold = org.altHold;
   spdHold = org.spdHold;

   bankDeg = org.bankDeg;
   turnRateDps = org.turnRateDps;
}

void DroneDynamics::reset()
{
   BaseClass::reset();

   // Volta aos comandos declarados no .epp; o sentinela -9999 faz a
   // primeira passada de dynamics() adotar o estado real do player.
   cmdHeadingDeg = initCmdHeadingDeg;
   cmdAltitudeM = initCmdAltitudeM;
   cmdSpeedKts = initCmdSpeedKts;

   bankDeg = 0.0;
   turnRateDps = 0.0;
}

models::Player* DroneDynamics::getPlayer()
{
   // Mesmo padrao do JSBSimModel nativo: o player e o CONTAINER deste
   // componente, nao um slot.
   return dynamic_cast<models::Player*>(findContainerByType(typeid(models::Player)));
}

//------------------------------------------------------------------------------
// dynamics() -- FASE 0 do frame de tempo critico.
//
// Chamado por Player::dynamics() uma vez por frame, com o dt do frame
// inteiro. Depois daqui, o proprio Player integra a posicao a partir da
// velocidade NED escrita no fim desta funcao.
//------------------------------------------------------------------------------
void DroneDynamics::dynamics(const double dt)
{
   if (dt <= 0.0) return;

   models::Player* const player{getPlayer()};
   if (player == nullptr) return;

   const double hdgDeg{player->getHeadingD()};
   const double altM{player->getAltitudeM()};
   const double spdMps{player->getTotalVelocity()};

   // Primeira passada apos o reset: adota o estado inicial do player como
   // comando, para nao dar um "chute" no primeiro frame.
   if (cmdHeadingDeg <= -9999.0) cmdHeadingDeg = hdgDeg;
   if (cmdAltitudeM <= -9999.0) cmdAltitudeM = altM;
   if (cmdSpeedKts <= -9999.0) cmdSpeedKts = spdMps / KTS2MPS;

   // --- rumo: curva com taxa limitada, pelo lado mais curto ---
   const double hdgErrDeg{domain::wrap180(cmdHeadingDeg - hdgDeg)};
   turnRateDps = clamp(hdgErrDeg / dt, -maxTurnRateDps, maxTurnRateDps);
   const double newHdgDeg{domain::wrap360(hdgDeg + turnRateDps * dt)};

   // Banco proporcional a taxa de curva -- puramente de atitude (este
   // modelo nao tem equacoes de momento), mas e o que faz o drone
   // "inclinar" na curva no replay do Tacview.
   bankDeg = maxBankDeg * (turnRateDps / maxTurnRateDps);

   // --- velocidade: aceleracao longitudinal limitada ---
   const double cmdSpdMps{clamp(cmdSpeedKts, minSpeedKts, maxSpeedKts) * KTS2MPS};
   const double dv{clamp(cmdSpdMps - spdMps, -maxAccelMps2 * dt, maxAccelMps2 * dt)};
   const double newSpdMps{spdMps + dv};

   // --- altitude: taxa de subida proporcional ao erro, limitada ---
   const double altErrM{cmdAltitudeM - altM};
   const double climbMps{clamp(altErrM * ALT_GAIN, -maxClimbRateMps, maxClimbRateMps)};

   // Angulo de trajetoria convertido em atitude de arfagem (aproximacao
   // 3-DOF: sem angulo de ataque, o nariz aponta para onde o drone vai).
   const double sinPitch{(newSpdMps > 1.0) ? clamp(climbMps / newSpdMps, -1.0, 1.0) : 0.0};
   const double pitchRad{std::asin(sinPitch)};

   // --- escreve no Player ---
   // Ordem importa: atitude antes de velocidade (o Player recalcula as
   // matrizes de rotacao no setEulerAngles e as usa para converter a
   // velocidade NED para os demais referenciais).
   player->setEulerAngles(bankDeg * DEG2RAD, pitchRad, newHdgDeg * DEG2RAD);

   const double horizMps{std::sqrt(std::max(newSpdMps * newSpdMps - climbMps * climbMps, 0.0))};
   const double hdgRad{newHdgDeg * DEG2RAD};
   player->setVelocity(horizMps * std::cos(hdgRad),   // Norte
                       horizMps * std::sin(hdgRad),   // Leste
                       -climbMps);                    // Down (negativo = subindo)

   player->setAngularVelocities(0.0, 0.0, turnRateDps * DEG2RAD);

   // Nota: nao chamamos setPosition*(): a integracao de posicao e do
   // proprio Player (positionUpdate(), trapezoidal). Passar 'slaved=true'
   // num setPosition aqui DESLIGARIA essa integracao -- e o que a poc/10
   // faz de proposito com os satelites, e o oposto do que queremos aqui.
}

bool DroneDynamics::setCommandedHeadingD(const double h, const double, const double)
{
   cmdHeadingDeg = domain::wrap360(h);
   hdgHold = true;
   return true;
}

bool DroneDynamics::setCommandedVelocityKts(const double v, const double)
{
   cmdSpeedKts = clamp(v, minSpeedKts, maxSpeedKts);
   spdHold = true;
   return true;
}

bool DroneDynamics::setCommandedAltitude(const double a, const double, const double)
{
   cmdAltitudeM = a;
   altHold = true;
   return true;
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool DroneDynamics::setSlotMaxTurnRate(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   maxTurnRateDps = base::Degrees::convertStatic(*msg);
   return (maxTurnRateDps > 0.0);
}

bool DroneDynamics::setSlotMaxBank(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   maxBankDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool DroneDynamics::setSlotMaxClimbRate(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   maxClimbRateMps = msg->getDouble();
   return (maxClimbRateMps > 0.0);
}

bool DroneDynamics::setSlotMaxAccel(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   maxAccelMps2 = msg->getDouble();
   return (maxAccelMps2 > 0.0);
}

bool DroneDynamics::setSlotMinSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   minSpeedKts = msg->getDouble();
   return (minSpeedKts >= 0.0);
}

bool DroneDynamics::setSlotMaxSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   maxSpeedKts = msg->getDouble();
   return (maxSpeedKts > 0.0);
}

bool DroneDynamics::setSlotCmdHeading(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   initCmdHeadingDeg = base::Degrees::convertStatic(*msg);
   cmdHeadingDeg = initCmdHeadingDeg;
   return true;
}

bool DroneDynamics::setSlotCmdAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   initCmdAltitudeM = base::Meters::convertStatic(*msg);
   cmdAltitudeM = initCmdAltitudeM;
   return true;
}

bool DroneDynamics::setSlotCmdSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   initCmdSpeedKts = msg->getDouble();
   cmdSpeedKts = initCmdSpeedKts;
   return true;
}

} // namespace xdrone
} // namespace mixr
