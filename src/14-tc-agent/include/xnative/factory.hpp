#ifndef __xnative_factory_H__
#define __xnative_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; }

namespace xnative {

// Factory do que SOBROU de proprio nesta PoC -- so as pecas do UBF, o
// datalink derivado e a carga util da mensagem. Player, dinamica, piloto
// automatico, sensor e agente vem todos do framework (ver factory.cpp).
base::Object* factory(const std::string& name);

} // namespace xnative
} // namespace mixr

#endif
