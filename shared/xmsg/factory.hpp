#pragma once

#include <string>

namespace mixr {
namespace base { class Object; }

namespace xmsg {

// Factory das classes deste subsistema. Encadeie no factory() da aplicacao
// ANTES das factories do framework -- a primeira que retorna nao-nulo vence.
base::Object* factory(const std::string& name);

} // namespace xmsg
} // namespace mixr
