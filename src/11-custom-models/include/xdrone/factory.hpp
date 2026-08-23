#ifndef __xdrone_factory_H__
#define __xdrone_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; }

namespace xdrone {

// Factory das classes proprias desta PoC -- mesma forma das factories do
// framework (mixr::models::factory, mixr::base::factory, ...): recebe o
// nome usado no .epp e devolve uma instancia nova, ou nullptr se o nome
// nao for daqui. E encadeada ANTES das do framework no mixr_factory.cpp.
base::Object* factory(const std::string& name);

} // namespace xdrone
} // namespace mixr

#endif
