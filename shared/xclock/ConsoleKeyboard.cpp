#include "xclock/ConsoleKeyboard.hpp"

#include <fcntl.h>
#include <unistd.h>

namespace mixr {
namespace xclock {

ConsoleKeyboard::ConsoleKeyboard()
{
   if (::tcgetattr(STDIN_FILENO, &original) != 0) return;

   termios raw{original};
   raw.c_lflag &= ~(ICANON | ECHO);
   raw.c_cc[VMIN] = 0;
   raw.c_cc[VTIME] = 0;
   if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return;

   ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
   active = true;
}

ConsoleKeyboard::~ConsoleKeyboard()
{
   if (active) ::tcsetattr(STDIN_FILENO, TCSANOW, &original);
}

bool ConsoleKeyboard::isActive() const
{
   return active;
}

char ConsoleKeyboard::poll() const
{
   if (!active) return '\0';
   char c{};
   const ssize_t n{::read(STDIN_FILENO, &c, 1)};
   return (n == 1) ? c : '\0';
}

}
}
