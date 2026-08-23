#pragma once

#include <string>

namespace mixr { namespace base { class Object; } }

// Factory dos objetos MIXR deste subprojeto.
//
// Padrao do framework: a primeira factory da cadeia que devolver nao-nulo
// vence, entao as locais vem antes das do framework. Extraida do main.cpp
// para manter a orquestracao fina -- mesmo arranjo de poc/03, 05 e 08.
mixr::base::Object* mixrFactory(const std::string& name);
