#include "xnative/factory.hpp"

#include "ubf/ExampleAction.hpp"
#include "ubf/ExampleBehavior.hpp"
#include "ubf/ExampleState.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace models {
namespace xtemplate {

//------------------------------------------------------------------------------
// Registra as tres classes deste template. Ao acrescentar uma classe nova
// (na sua copia deste diretorio), ela entra em TRES lugares, sempre juntos:
// aqui (o if/else), em NOMES[] e em METAS[] logo abaixo -- o registro de
// plugin confere as tres listas na carga e recusa se ficarem fora de
// sincronia.
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if (name == ExampleState::getFactoryName())         obj = new ExampleState();
   else if (name == ExampleBehavior::getFactoryName()) obj = new ExampleBehavior();
   else if (name == ExampleAction::getFactoryName())   obj = new ExampleAction();

   return obj;
}

namespace {

const char* const NOMES[] = {
   "ExampleState",
   "ExampleBehavior",
   "ExampleAction",
   nullptr
};

const base::MetaObject* const METAS[] = {
   ExampleState::getMetaObject(),
   ExampleBehavior::getMetaObject(),
   ExampleAction::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xtemplate
} // namespace models
} // namespace mixr
