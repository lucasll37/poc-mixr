#include "linkage/KeyboardDevice.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

namespace keyboard {

IMPLEMENT_SUBCLASS(KeyboardDevice, "KeyboardDevice")
EMPTY_SLOTTABLE(KeyboardDevice)

KeyboardDevice::KeyboardDevice()
{
   STANDARD_CONSTRUCTOR()
}

void KeyboardDevice::copyData(const KeyboardDevice& org, const bool)
{
   BaseClass::copyData(org);
   // estado de terminal (fd, termios salvo) e proprio de cada instancia --
   // reset() e quem de fato abre o dispositivo, nao ha nada util pra
   // copiar aqui.
}

void KeyboardDevice::deleteData()
{
   if (rawModeEnabled) {
      ::tcsetattr(STDIN_FILENO, TCSANOW, &savedTermios);
      ::fcntl(STDIN_FILENO, F_SETFL, savedFcntlFlags);
      rawModeEnabled = false;
   }
}

void KeyboardDevice::reset()
{
   // AbstractIoDevice::reset() e puro virtual (sem corpo) -- nao ha
   // BaseClass::reset() pra chamar aqui (mesmo padrao de MockDevice).
   if (rawModeEnabled) return;

   if (::tcgetattr(STDIN_FILENO, &savedTermios) != 0) {
      std::cerr << "[keyboard] tcgetattr falhou (stdin nao e um terminal?): "
                << std::strerror(errno) << std::endl;
      return;
   }

   termios raw{savedTermios};
   raw.c_lflag &= ~(ICANON | ECHO);
   raw.c_cc[VMIN] = 0;
   raw.c_cc[VTIME] = 0;
   ::tcsetattr(STDIN_FILENO, TCSANOW, &raw);

   savedFcntlFlags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
   ::fcntl(STDIN_FILENO, F_SETFL, savedFcntlFlags | O_NONBLOCK);

   rawModeEnabled = true;

   std::cout << "[keyboard] terminal em modo raw -- "
             << "W/S=altitude A/D=heading Q/E=velocidade 1-4=formacao R=RTB"
             << std::endl;
}

bool KeyboardDevice::getDiscreteInput(bool* const value, const int channel, const int port) const
{
   if (value == nullptr || port != 0 || channel < 0 || channel >= NUM_CHANNELS) return false;
   *value = keyState[static_cast<std::size_t>(channel)];
   return true;
}

void KeyboardDevice::readInputs()
{
   keyState.fill(false);

   if (!rawModeEnabled) return;

   char c{};
   while (::read(STDIN_FILENO, &c, 1) == 1) {
      switch (c) {
         case 'w': case 'W': keyState[ALT_UP] = true;    break;
         case 's': case 'S': keyState[ALT_DOWN] = true;  break;
         case 'a': case 'A': keyState[HDG_LEFT] = true;  break;
         case 'd': case 'D': keyState[HDG_RIGHT] = true; break;
         case 'q': case 'Q': keyState[SPD_DOWN] = true;  break;
         case 'e': case 'E': keyState[SPD_UP] = true;    break;
         case '1':           keyState[FORM_1] = true;    break;
         case '2':           keyState[FORM_2] = true;    break;
         case '3':           keyState[FORM_3] = true;    break;
         case '4':           keyState[FORM_4] = true;    break;
         case 'r': case 'R': keyState[RTB] = true;       break;
         default: break;
      }
   }
}

} // namespace keyboard
