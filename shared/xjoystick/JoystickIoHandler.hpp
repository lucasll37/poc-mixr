#ifndef __xjoystick_JoystickIoHandler_H__
#define __xjoystick_JoystickIoHandler_H__

#include "mixr/linkage/IoHandler.hpp"

#include <string>

namespace mixr {
namespace base { class String; class Number; }
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
//    player       <String>   ! Nome do player a controlar (ex.: "bandit1")
//    deviceIndex  <Number>   ! Indice do dispositivo a checar (default: 0) --
//                            ! TEM que bater com o 'deviceIndex:' do
//                            ! ( UsbJoystick ) dentro de 'devices:', mais
//                            ! abaixo no mesmo 'ioHandler:'.
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
// FALLBACK SEM HARDWARE -- o UsbJoystick nativo degrada sozinho (canais em
// zero, sem exception/abort), mas ele nao AVISA quem o chama que o
// dispositivo sumiu -- IoData::getAnalogInput() so olha limite de indice,
// nao "o valor veio de verdade". Sem essa checagem, um bandit1 sem joystick
// fisico ficaria voando em manual com entradas zeradas (manete na metade,
// sem stick nenhum), em vez de manter o Autopilot scripted de sempre.
// Por isso este handler checa a EXISTENCIA do dispositivo ele mesmo
// (hasRealJoystick(), mesma ordem de busca do UsbJoystick_linux.cpp:
// '/dev/js<N>' depois '/dev/input/js<N>') antes de tocar em qualquer coisa:
// sem o arquivo, nem desengata o Autopilot nem aplica stick -- o player
// continua exatamente como ficaria SEM nenhuma secao 'ioHandler:' no EDL.
// A checagem roda todo frame (um stat(), custo desprezivel): plugar o
// joystick no meio de uma execucao troca para controle manual sem reiniciar
// nada, e desplugar devolve o Autopilot.
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
   bool hasRealJoystick() const;

   std::string playerName;
   int deviceIndex{};

private:
   // slot table helper methods
   bool setSlotPlayer(const base::String* const);
   bool setSlotDeviceIndex(const base::Number* const);
};

}
}

#endif
