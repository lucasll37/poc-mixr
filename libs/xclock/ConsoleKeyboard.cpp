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

   // F_SETFL muda flags da open file description, compartilhada entre
   // processos que herdaram o mesmo fd via fork()+exec() -- o caso normal de
   // rodar um binario interativo a partir de um shell. Sem salvar e
   // restaurar O_NONBLOCK, sair normalmente deixava o shell que invocou o
   // processo com stdin nao-bloqueante. O flag original e salvo antes de
   // setar, para restaurar exatamente o estado anterior.
   originalFlags = ::fcntl(STDIN_FILENO, F_GETFL);
   if (originalFlags != -1) ::fcntl(STDIN_FILENO, F_SETFL, originalFlags | O_NONBLOCK);
   active = true;
}

ConsoleKeyboard::~ConsoleKeyboard()
{
   if (!active) return;
   ::tcsetattr(STDIN_FILENO, TCSANOW, &original);
   if (originalFlags != -1) ::fcntl(STDIN_FILENO, F_SETFL, originalFlags);
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
