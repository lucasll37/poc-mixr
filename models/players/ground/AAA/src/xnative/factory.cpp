#include "xnative/factory.hpp"

#include "ubf/AaaAction.hpp"
#include "ubf/AaaBehavior.hpp"
#include "ubf/AaaState.hpp"
#include "xnative/AaaAgent.hpp"
#include "xnative/AaaSite.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace models {
namespace xAAA {

//------------------------------------------------------------------------------
// Registra as cinco classes deste modelo. Ao acrescentar uma classe nova,
// ela entra em TRES lugares, sempre juntos: aqui (o if/else), em NOMES[] e
// em METAS[] logo abaixo -- o registro de plugin confere as tres listas na
// carga e recusa se ficarem fora de sincronia.
//
// ( UbfAgent ) nativo continua existindo e continua NAO entrando aqui --
// e' classe NATIVA do MIXR (factory "UbfAgent", registrada em
// base/factory.cpp), nao deste modelo. 'AaaAgent' e' outra coisa: uma
// subclasse PROPRIA, minima, que so' fecha um ciclo de referencia com o
// proprio player no shutdown (ver include/xnative/AaaAgent.hpp) -- o
// cenario de producao passou a declarar ( AaaAgent ) no lugar de
// ( UbfAgent ) por causa disso. Ver include/xnative/AaaSite.hpp para o
// "porque" completo de nao haver AgentTC aqui.
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if (name == AaaSite::getFactoryName())          obj = new AaaSite();
   else if (name == AaaState::getFactoryName())    obj = new AaaState();
   else if (name == AaaBehavior::getFactoryName()) obj = new AaaBehavior();
   else if (name == AaaAction::getFactoryName())   obj = new AaaAction();
   else if (name == AaaAgent::getFactoryName())    obj = new AaaAgent();

   return obj;
}

namespace {

const char* const NOMES[] = {
   "AaaSite",
   "AaaState",
   "AaaBehavior",
   "AaaAction",
   "AaaAgent",
   nullptr
};

const base::MetaObject* const METAS[] = {
   AaaSite::getMetaObject(),
   AaaState::getMetaObject(),
   AaaBehavior::getMetaObject(),
   AaaAction::getMetaObject(),
   AaaAgent::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xAAA
} // namespace models
} // namespace mixr
