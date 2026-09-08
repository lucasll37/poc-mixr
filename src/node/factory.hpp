#pragma once

#include "mixr/base/Object.hpp"

#include <string>

//------------------------------------------------------------------------------
// Factory de objetos MIXR deste host -- mesmo padrao de duas funcoes que
// app/include/mixr_factory.hpp e src/rl/bindings/mixr_factory.hpp, mas com a
// cadeia COMPLETA (ao contrario da de src/rl/bindings, enxuta para UM cenario
// hermetico conhecido): node roda qualquer cenario de src/poc/**, que pode
// usar terreno, DIS, joystick fisico ou MsgFeed -- omitir um elo faria o
// bloco EDL correspondente construir nada, em silencio.
//
// factoryBuiltin()  a cadeia SEM plugin. Devolve nullptr para nome
//                   desconhecido -- usada como SONDA por libs/xplugin para
//                   recusar, na CARGA, um plugin cujo nome de fabrica colida
//                   com o framework.
//
// factory()         a que vai para o edl_parser. NUNCA devolve nullptr --
//                   devolver nullptr aqui termina em SIGSEGV dentro do
//                   edl_parser deste fork (mesmo motivo documentado em
//                   app/include/mixr_factory.hpp e
//                   src/rl/bindings/mixr_factory.hpp).
namespace node {

mixr::base::Object* factoryBuiltin(const std::string& name);

mixr::base::Object* factory(const std::string& name);

}
