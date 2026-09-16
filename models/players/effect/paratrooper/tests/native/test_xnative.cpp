//
// Camada 3 do MODELO -- as classes MIXR proprias, SEM levantar Station.
//
// Nao levanta simulacao: nada aqui tem WorldModel, Station, terreno ou
// JSBSim. Sao objetos construidos com 'new', slots setados a mao e metodos
// chamados direto.
//
#include "xnative/factory.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>

namespace {

using namespace mixr::models::xparatrooper;

//------------------------------------------------------------------------------
// A FABRICA e as duas listas que o descritor do plugin publica.
//------------------------------------------------------------------------------
TEST(Factory, ConstroiTudoQueDeclara)
{
   for (const char* const* p = factoryNames(); *p != nullptr; ++p) {
      mixr::base::Object* const obj{factory(*p)};
      EXPECT_NE(obj, nullptr) << "declarou '" << *p << "' mas a fabrica devolveu nulo";
      if (obj != nullptr) obj->unref();
   }
}

TEST(Factory, RecusaNomeDesconhecido)
{
   EXPECT_EQ(factory("NaoExiste"), nullptr);
   EXPECT_EQ(factory(""), nullptr);
}

TEST(Factory, TodaClasseDeclaradaExportaMetaObject)
{
   std::set<std::string> nomes;
   for (const char* const* p = factoryNames(); *p != nullptr; ++p) nomes.insert(*p);

   std::set<std::string> comMeta;
   for (const mixr::base::MetaObject* const* m = metaObjects(); *m != nullptr; ++m) {
      comMeta.insert((*m)->getFactoryName());
   }
   EXPECT_EQ(nomes, comMeta) << "factoryNames() e metaObjects() divergiram";
}

} // namespace
