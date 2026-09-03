#pragma once

#include "mixr/base/Object.hpp"

#include <string>

//------------------------------------------------------------------------------
// Factory de objetos MIXR desta PoC -- em DUAS funcoes, e a separacao importa.
//
// mixrFactoryBuiltin()  a cadeia SEM plugin. Devolve nullptr para nome
//                       desconhecido. E usada como SONDA pelo registro de
//                       plugins (shared/xplugin), para recusar na CARGA um
//                       plugin cujo nome de fabrica ja exista -- em vez de
//                       deixa-lo silenciosamente inerte.
//
// mixrFactory()         a que vai para o edl_parser. NUNCA devolve nullptr:
//                       nome desconhecido e fatal, com diagnostico.
//
// Por que mixrFactory nao pode devolver nullptr -- e isto e correcao de um
// bug pre-existente, nao zelo:
//
//   * o diagnostico "undefined factory name" do proprio parser
//     (edl_parser.y:97-100) esta num ramo alcancavel so com
//     arg_list == nullptr, mas a producao 'arglist:' SEMPRE aloca um
//     PairStream -- e codigo morto, nunca dispara;
//   * e devolver nullptr leva ao edl_parser.y:179
//     ('slot_value : SLOT_ID form { ... $2->unref(); }'), que NAO checa nulo.
//     Referenced::unref() e inline e mexe em refCount/semaphore por offset
//     fixo, entao o resultado e SIGSEGV, nao erro.
//
// O sintoma de hoje ate depende da posicao sintatica: 'slot: ( X )' estoura,
// '{ ( X ) }' silencia (edl_parser.y:145 checa). Nas duas formas, sem
// mensagem.
//------------------------------------------------------------------------------
mixr::base::Object* mixrFactoryBuiltin(const std::string& name);

mixr::base::Object* mixrFactory(const std::string& name);
