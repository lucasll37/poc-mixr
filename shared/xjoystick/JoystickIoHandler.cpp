#include "xjoystick/JoystickIoHandler.hpp"

#include "xjoystick/ChannelMap.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/Simulation.hpp"

#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/Autopilot.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/concepts/linkage/AbstractIoData.hpp"
#include "mixr/base/util/math_utils.hpp"

namespace mixr {
namespace xjoystick {

IMPLEMENT_SUBCLASS(JoystickIoHandler, "JoystickIoHandler")

BEGIN_SLOTTABLE(JoystickIoHandler)
   "player",      // 1: nome do player a controlar
END_SLOTTABLE(JoystickIoHandler)

BEGIN_SLOT_MAP(JoystickIoHandler)
   ON_SLOT(1, setSlotPlayer, base::String)
END_SLOT_MAP()

EMPTY_DELETEDATA(JoystickIoHandler)

JoystickIoHandler::JoystickIoHandler()
{
   STANDARD_CONSTRUCTOR()
}

void JoystickIoHandler::copyData(const JoystickIoHandler& org, const bool)
{
   BaseClass::copyData(org);
   playerName = org.playerName;
}

bool JoystickIoHandler::setSlotPlayer(const base::String* const x)
{
   if (x == nullptr) return false;
   playerName = x->getString();
   return true;
}

models::AirVehicle* JoystickIoHandler::findTarget()
{
   if (playerName.empty()) return nullptr;

   const auto station = dynamic_cast<simulation::Station*>(
      findContainerByType(typeid(simulation::Station)));
   if (station == nullptr) return nullptr;

   simulation::Simulation* const sim{station->getSimulation()};
   if (sim == nullptr) return nullptr;

   base::PairStream* const players{sim->getPlayers()};
   if (players == nullptr) return nullptr;

   models::AirVehicle* result{};
   base::Pair* const p{players->findByName(playerName.c_str())};
   if (p != nullptr) result = dynamic_cast<models::AirVehicle*>(p->object());
   players->unref();
   return result;
}

void JoystickIoHandler::inputDevicesImpl(const double dt)
{
   readDeviceInputs(dt);

   models::AirVehicle* const av{findTarget()};
   if (av == nullptr) return;

   const base::AbstractIoData* const inData{getInputData()};
   if (inData == nullptr) return;

   // Desliga o piloto automatico -- ver o comentario do .hpp sobre por que
   // isto tem de acontecer TODO frame, e nao so na primeira leitura.
   base::Pair* const pilotPair{av->getPilotByType(typeid(models::Autopilot))};
   if (pilotPair != nullptr) {
      const auto ap = static_cast<models::Autopilot*>(pilotPair->object());
      ap->setHeadingHoldMode(false);
      ap->setAltitudeHoldMode(false);
      ap->setVelocityHoldMode(false);
   }

   double roll{};
   inData->getAnalogInput(ROLL_AI, &roll);
   double pitch{};
   inData->getAnalogInput(PITCH_AI, &pitch);
   av->setControlStick(base::alim(roll, 1.0), base::alim(pitch, 1.0));

   double rudder{};
   inData->getAnalogInput(RUDDER_AI, &rudder);
   av->setRudderPedalInput(base::alim(rudder, 1.0));

   double throttle{};
   inData->getAnalogInput(THROTTLE_AI, &throttle);
   if (throttle < 0.0) throttle = 0.0;
   else if (throttle > 2.0) throttle = 2.0;
   av->setThrottles(&throttle, 1);
}

}
}
