#pragma once

#include "mixr/base/Object.hpp"

#include <string>

//------------------------------------------------------------------------------
// Factory de objetos MIXR do sim-runner -- em DUAS funcoes, e a separacao
// importa (mesmo desenho de src/poc/single-thread/include/mixr_factory.hpp,
// so que sem xtacview/xclock/xjoystick/xmsg/dis/linkage/recorder: o
// sim-runner nao declara Tacview, controle de tempo, joystick, mensagens,
// rede DIS nem gravador -- ver src/server/README.md).
//
// mixrFactoryBuiltin()  a cadeia SEM plugin. Devolve nullptr para nome
//                       desconhecido. E usada como SONDA pelo registro de
//                       plugins (shared/xplugin), para recusar na CARGA um
//                       plugin cujo nome de fabrica ja exista -- em vez de
//                       deixa-lo silenciosamente inerte.
//
// mixrFactory()         a que vai para o edl_parser. NUNCA devolve nullptr:
//                       nome desconhecido e fatal, com diagnostico.
//------------------------------------------------------------------------------
mixr::base::Object* mixrFactoryBuiltin(const std::string& name);

mixr::base::Object* mixrFactory(const std::string& name);
