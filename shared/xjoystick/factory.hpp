#ifndef __xjoystick_factory_H__
#define __xjoystick_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; }
namespace xjoystick {

// Factory da biblioteca xjoystick -- encadeie no factory() do main.cpp ANTES
// de mixr::linkage::factory() (mesmo padrao de shared/x* dos exemplos MIXR):
// a primeira que retorna nao-nulo vence, e "JoystickIoHandler" e um nome
// nosso, nao do framework.
base::Object* factory(const std::string&);

}
}

#endif
