#include "xnative/factory.hpp"

#include "ubf/ParatrooperAction.hpp"
#include "ubf/ParatrooperBtBehavior.hpp"
#include "ubf/ParatrooperState.hpp"
#include "xnative/Paratrooper.hpp"
#include "xnative/ParatrooperAgentTC.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {

//------------------------------------------------------------------------------
// Registra as cinco classes deste modelo. Ao acrescentar uma classe nova,
// ela entra em TRES lugares, sempre juntos: aqui (o if/else), em NOMES[] e
// em METAS[] logo abaixo -- o registro de plugin confere as tres listas na
// carga e recusa se ficarem fora de sincronia.
//
// Nomes de fabrica sem prefixo (ao contrario de C-130, que usa "C130*"):
// nenhum deles colide hoje com A-4 ("Flight*"), C-130 ("C130*") ou o
// template/mirror ("Example*") -- confirmar com
// tests/guard/check_colisao_fabrica.py depois de qualquer mudanca aqui.
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if (name == Paratrooper::getFactoryName())              obj = new Paratrooper();
   else if (name == ParatrooperAgentTC::getFactoryName())  obj = new ParatrooperAgentTC();

   else if (name == ParatrooperState::getFactoryName())      obj = new ParatrooperState();
   else if (name == ParatrooperBtBehavior::getFactoryName()) obj = new ParatrooperBtBehavior();
   else if (name == ParatrooperAction::getFactoryName())     obj = new ParatrooperAction();

   return obj;
}

namespace {

const char* const NOMES[] = {
   "Paratrooper",
   "ParatrooperAgentTC",
   "ParatrooperState",
   "ParatrooperBtBehavior",
   "ParatrooperAction",
   nullptr
};

const base::MetaObject* const METAS[] = {
   Paratrooper::getMetaObject(),
   ParatrooperAgentTC::getMetaObject(),
   ParatrooperState::getMetaObject(),
   ParatrooperBtBehavior::getMetaObject(),
   ParatrooperAction::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xparatrooper
} // namespace models
} // namespace mixr
