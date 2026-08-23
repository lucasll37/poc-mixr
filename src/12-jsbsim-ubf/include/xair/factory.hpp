#ifndef __xair_factory_H__
#define __xair_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; }

namespace xair {

// Factory das classes proprias desta PoC (player, dinamica, subsistemas e
// as pecas do UBF). Encadeada ANTES das do framework no mixr_factory.cpp.
//
// Note que ela registra tambem o FlightAgent: 'UbfAgentTC' nao e criado por
// nenhuma factory do MIXR (so UbfAgent e UbfArbiter sao), entao um agente
// de tempo critico SO existe em EDL se a aplicacao registrar o seu.
base::Object* factory(const std::string& name);

} // namespace xair
} // namespace mixr

#endif
