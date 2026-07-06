#pragma once

#include "mixr/linkage/IoDevice.hpp"

#include <array>
#include <termios.h>

namespace keyboard {

// Ordem dos canais discretos expostos por este dispositivo (porta 0).
// Mapeamento tecla->canal usado tanto por readInputs() quanto pelos
// adapters DiscreteInput declarados em configs/linkage.epp.
enum Channel {
   ALT_UP = 0, ALT_DOWN, HDG_LEFT, HDG_RIGHT, SPD_DOWN, SPD_UP,
   FORM_1, FORM_2, FORM_3, FORM_4, RTB,
   NUM_CHANNELS
};

//------------------------------------------------------------------------------
// Class: KeyboardDevice
//
// Le teclas do terminal (termios em modo raw, sem eco/canonical, stdin
// nao-bloqueante) e expoe cada tecla mapeada como um canal discreto de
// entrada -- e o unico pedaco customizado desta PoC para input (o
// framework MIXR nao tem nenhuma classe de teclado pronta neste modulo,
// so UsbJoystick/MockDevice). Tudo em volta (adapters, IoData, IoHandler)
// e o mecanismo nativo mixr::linkage.
//
// Factory name: KeyboardDevice
//------------------------------------------------------------------------------
class KeyboardDevice final : public mixr::linkage::IoDevice
{
   DECLARE_SUBCLASS(KeyboardDevice, mixr::linkage::IoDevice)

public:
   KeyboardDevice();

   void reset() final;

   int getNumDiscreteInputChannels() const final { return NUM_CHANNELS; }
   int getNumDiscreteInputPorts() const final { return 1; }
   bool getDiscreteInput(bool* const value, const int channel, const int port) const final;

   int getNumDiscreteOutputChannels() const final { return 0; }
   int getNumDiscreteOutputPorts() const final { return 0; }
   bool setDiscreteOutput(const bool, const int, const int) final { return false; }

   int getNumAnalogInputs() const final { return 0; }
   bool getAnalogInput(double* const, const int) const final { return false; }
   int getNumAnalogOutputs() const final { return 0; }
   bool setAnalogOutput(const double, const int) final { return false; }

private:
   void readInputs() final;
   void writeOutputs() final {}

   bool rawModeEnabled{};
   termios savedTermios{};
   int savedFcntlFlags{};
   std::array<bool, NUM_CHANNELS> keyState{};
};

} // namespace keyboard
