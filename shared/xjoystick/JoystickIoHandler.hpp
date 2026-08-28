#ifndef __xjoystick_JoystickIoHandler_H__
#define __xjoystick_JoystickIoHandler_H__

#include "mixr/linkage/IoHandler.hpp"

#include <string>

namespace mixr {
namespace base { class String; }
namespace models { class AirVehicle; }
namespace xjoystick {

//------------------------------------------------------------------------------
// Class: JoystickIoHandler
//
// Description: Le um mixr::linkage::UsbJoystick (via IoData/AnalogInput,
//              tudo nativo -- ver o bloco 'ioHandler:' do scenario.epp.in) e
//              aplica roll/pitch/leme/manete diretamente no AirVehicle
//              indicado pelo slot 'player'.
//
// Factory name: JoystickIoHandler
//
// Slots:
//    player   <String>   ! Nome do player a controlar (ex.: "bandit1")
//
// ARMADILHA DE DESENHO -- nao esta em nenhum exemplo do framework, foi
// preciso decidir: o player alvo normalmente tem um
// ( Autopilot headingHoldMode/altitudeHoldMode/velocityHoldMode: true ) para
// manter o cenario inicial. O Autopilot reimpoe esses modos a cada fase do
// frame de tempo critico (ate 50 Hz), enquanto este handler e sondado no
// laco de background (10 Hz, mesmo lugar do xclock::TimeControls::poll()) --
// se o stick fosse escrito so no AirVehicle sem desengatar os hold modes, o
// Autopilot sobrescreveria o comando do joystick antes da proxima leitura.
// Por isso, ao localizar o Autopilot do player (Player::getPilotByType),
// este handler desliga os tres hold modes TODO frame antes de aplicar o
// stick -- idempotente, sem precisar de um evento explicito de "assumir
// controle".
//
// SEM HARDWARE: o dispositivo nativo (UsbJoystick) ja degrada sozinho --
// sem '/dev/input/jsX' so loga um aviso e os canais ficam em zero. Este
// handler nao precisa (nem deve) tratar essa ausencia: aplicar zero em
// roll/pitch/leme e manete no cutoff e um comportamento seguro, nao um erro.
//------------------------------------------------------------------------------
class JoystickIoHandler final : public linkage::IoHandler
{
   DECLARE_SUBCLASS(JoystickIoHandler, linkage::IoHandler)

public:
   JoystickIoHandler();

private:
   void inputDevicesImpl(const double dt) override;
   void outputDevicesImpl(const double dt) override { writeDeviceOutputs(dt); }

   models::AirVehicle* findTarget();

   std::string playerName;

private:
   // slot table helper methods
   bool setSlotPlayer(const base::String* const);
};

}
}

#endif
