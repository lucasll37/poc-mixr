#ifndef __app_MixrText_H__
#define __app_MixrText_H__

#include "mixr/base/String.hpp"

#include <string>

//------------------------------------------------------------------------------
// app::mixrText() -- converte um mixr::base::String* em std::string SEM
// desreferenciar ponteiro nulo.
//
// ARMADILHA QUE ISTO FECHA, e ela e sutil porque a guarda "obvia" e VAZIA:
//
//   s.name = (player->getName() != nullptr) ? player->getName()->getString() : "?";
//
// 'AbstractPlayer::getName()' devolve '&pname' -- o endereco de um membro POR
// VALOR (AbstractPlayer.inl:35-38, AbstractPlayer.hpp:110). Ele NUNCA e nulo,
// entao esse teste e sempre verdadeiro e nao protege nada. Quem pode ser nulo e
// o 'const char*' que 'String::getString()' devolve: o membro e 'char* str {}'
// (String.hpp:77) e 'String::String()' nao aloca (String.cpp:13-16), entao uma
// String/Identifier nunca preenchida devolve nullptr. Dali:
//
//   * 'std::string = nullptr'          -> strlen(nullptr) -> SIGSEGV
//   * 'ostream << (const char*)nullptr' -> mesma coisa dentro de __ostream_insert
//
// 'libs/xtacview/TacviewOutput.cpp' ja fazia a checagem certa (guardar o
// 'const char*', nao o 'String*') e o comentario de la registra que foi assim
// que o DataRecorder nativo abortou uma vez. Esta funcao existe para que a
// licao valha em UM lugar so, em vez de depender de cada chamador lembrar.
//------------------------------------------------------------------------------

namespace app {

inline std::string mixrText(const mixr::base::String* const s)
{
   if (s == nullptr) return {};
   const char* const bruto{s->getString()};
   return (bruto != nullptr) ? std::string{bruto} : std::string{};
}

}

#endif
