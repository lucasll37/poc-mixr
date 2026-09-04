#pragma once

#include "mixr/base/Object.hpp"

#include <string>

//------------------------------------------------------------------------------
// Factory de objetos MIXR deste host -- mesmo padrao de duas funcoes que
// src/poc/dis/single-thread/include/mixr_factory.hpp / app/include/mixr_factory.hpp.
//
// mixrFactoryBuiltin()  a cadeia SEM plugin. Devolve nullptr para nome
//                       desconhecido -- usada como SONDA por shared/xplugin
//                       para recusar, na CARGA, um plugin cujo nome de
//                       fabrica colida com o framework.
//
// mixrFactory()         a que vai para o edl_parser. NUNCA devolve nullptr
//                       (ver o mesmo cabecalho em app/include/mixr_factory.hpp
//                       para o porque -- devolver nullptr aqui termina em
//                       SIGSEGV dentro do edl_parser deste fork).
//
// MAIS ENXUTA que a do app/single-thread de proposito: este host nao usa
// ( ClockStation ) (a simulacao e passo-fixo, comandada por
// NativeSimulation::step(), nunca em tempo real), nem joystick, nem
// shared/xmsg, nem rede DIS (o cenario de RL e hermetico, mesmo motivo de
// tests/scenario/make_fixture.py). So xplugin (obrigatorio) + xtacview
// (visualizacao opcional no Tacview) + o framework nativo.
mixr::base::Object* mixrFactoryBuiltin(const std::string& name);

mixr::base::Object* mixrFactory(const std::string& name);
