#include "xnative/factory.hpp"

#include "xnative/GuidedMissile.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace models {
namespace xmissile {

//------------------------------------------------------------------------------
// Registra a unica classe MIXR deste modelo. Ao acrescentar uma classe nova,
// ela entra em TRES lugares, sempre juntos: aqui (o if/else), em NOMES[] e
// em METAS[] logo abaixo -- o registro de plugin confere as tres listas na
// carga e recusa se ficarem fora de sincronia.
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if (name == GuidedMissile::getFactoryName()) obj = new GuidedMissile();

   return obj;
}

namespace {

const char* const NOMES[] = {
   "GuidedMissile",
   nullptr
};

const base::MetaObject* const METAS[] = {
   GuidedMissile::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xmissile
} // namespace models
} // namespace mixr
