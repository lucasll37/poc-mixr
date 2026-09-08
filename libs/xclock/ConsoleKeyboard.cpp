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

   // ACHADO POR AUDITORIA (nao redescobrir): F_SETFL muda flags da OPEN FILE
   // DESCRIPTION, compartilhada entre processos que herdaram o mesmo fd via
   // fork()+exec() -- o caso normal de rodar um binario interativo a partir
   // de um shell. Sem salvar e restaurar O_NONBLOCK (so o termios era
   // restaurado), sair normalmente deixava o shell que invocou o processo
   // com stdin nao-bloqueante -- reproduzido isolado com fork+fcntl. Salva
   // ANTES de setar, pra restaurar exatamente o que havia (nao so' "limpar"
   // O_NONBLOCK, que pode nao ter sido o estado original).
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
