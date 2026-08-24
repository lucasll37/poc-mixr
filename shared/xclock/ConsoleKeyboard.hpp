#ifndef __xclock_ConsoleKeyboard_H__
#define __xclock_ConsoleKeyboard_H__

#include <termios.h>

namespace mixr {
namespace xclock {

//------------------------------------------------------------------------------
// Class: ConsoleKeyboard
//
// Description: Leitura de tecla avulsa, sem ENTER e sem bloquear o laco --
//              termios em modo raw (~ICANON/~ECHO, VMIN=VTIME=0) com stdin
//              em O_NONBLOCK. Restaura o terminal no destrutor.
//
// NAO e um mixr::linkage::IoDevice de proposito. O IoDevice/IoHandler existe
// para casar canais NOMEADOS declarados em EDL com hardware de entrada; aqui
// nao ha canal nenhum a nomear -- as teclas chamam metodos publicos comuns da
// ClockStation. Um IoDevice so acrescentaria cerimonia.
//
// SEM TTY (pipe, redirecionamento, CI) tcgetattr() falha: isActive() fica
// false, poll() sempre devolve '\0' e a simulacao roda normalmente. Isso
// importa neste repo porque os cenarios tambem sao rodados de forma nao
// interativa (-deterministic), onde nao ha terminal.
//------------------------------------------------------------------------------
class ConsoleKeyboard
{
public:
   ConsoleKeyboard();
   ~ConsoleKeyboard();

   ConsoleKeyboard(const ConsoleKeyboard&) = delete;
   ConsoleKeyboard& operator=(const ConsoleKeyboard&) = delete;

   bool isActive() const;

   // Proxima tecla pendente, ou '\0' se nao houver nenhuma.
   char poll() const;

private:
   termios original{};
   bool active{};
};

}
}

#endif
