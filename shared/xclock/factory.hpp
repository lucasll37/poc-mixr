#ifndef __xclock_factory_H__
#define __xclock_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; }
namespace xclock {

// Factory da biblioteca xclock -- encadeie no factory() do main.cpp ANTES
// das factories do framework (mesmo padrao de shared/x* dos exemplos MIXR).
// Precisa vir antes de mixr::simulation::factory(), que responde por
// "Station": a ClockStation e uma Station, mas com nome de fabrica proprio.
base::Object* factory(const std::string&);

}
}

#endif
