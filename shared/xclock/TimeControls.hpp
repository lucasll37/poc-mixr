#ifndef __xclock_TimeControls_H__
#define __xclock_TimeControls_H__

#include "xclock/ConsoleKeyboard.hpp"

#include <string>

namespace mixr {
namespace xclock {

class ClockStation;

//------------------------------------------------------------------------------
// Class: TimeControls
//
// Description: Liga as teclas do console ao relogio da ClockStation. Fica
//              aqui, e nao no main.cpp de cada subprojeto, para que os
//              subprojetos compartilhem as MESMAS teclas (mesmo motivo pelo
//              qual o Tacview virou shared/xtacview em vez de uma copia por
//              poc).
//
// Teclas:
//    +  =     acelerar (proximo degrau da escala)
//    -  _     frear    (degrau anterior; abaixo de 1x e camara lenta)
//    espaco p pausar / retomar
//    1        voltar para tempo real (1x) e retomar
//    h  ?     reimprimir esta ajuda
//
// Escala em degraus (0.1x ... 64x) em vez de passo fixo: a faixa util vai de
// camara lenta a dezenas de vezes, e um passo linear que sirva no topo e
// grosso demais embaixo.
//------------------------------------------------------------------------------
class TimeControls
{
public:
   explicit TimeControls(ClockStation* const);

   // Consome as teclas pendentes e aplica. Retorna true se algo mudou.
   bool poll();

   // false quando nao ha TTY (pipe/CI): a simulacao roda, so nao ha teclado.
   bool isAvailable() const;

   // Estado corrente para a linha de status, ex.: "4x" ou "PAUSADO (4x)".
   std::string describe() const;

   static std::string helpText();

private:
   bool apply(const char key);

   ClockStation* station{};
   ConsoleKeyboard keyboard;
   int ladderIndex{};
};

}
}

#endif
