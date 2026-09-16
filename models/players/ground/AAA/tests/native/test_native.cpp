//
// Camada 3 do MODELO AAA -- as classes MIXR proprias, SEM levantar Station.
// Mesmo raciocinio da camada 'native' de models/players/air/A-4 (ver o
// cabecalho de test_xnative.cpp la): a coerencia da fabrica so era
// conferida na CARGA do plugin (PluginRegistry, tarde e so' em teste de
// integracao) -- aqui e' instantaneo, e e exatamente o que quebra quando
// alguem acrescenta uma classe e esquece uma das tres listas
// (factory()/NOMES[]/METAS[] em xnative/factory.cpp).
//
// AaaSite em si (include/xnative/AaaSite.hpp) nao tem NENHUM slot ou
// comportamento proprio -- e' so' uma subclasse de SamVehicle pra herdar
// minLaunchRange/maxLaunchRange nativos (ver o cabecalho da classe). Por
// isso nao ha teste especifico pra ela alem de fazer parte desta varredura
// generica: nao ha nada mais pra exercitar que nao seja "a fabrica
// constroi" e "o MetaObject bate com o nome".
//
#include "xnative/factory.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>

TEST(Factory, ConstroiTudoQueDeclara)
{
   for (const char* const* p = mixr::models::xAAA::factoryNames(); *p != nullptr; ++p) {
      mixr::base::Object* const obj{mixr::models::xAAA::factory(*p)};
      EXPECT_NE(obj, nullptr) << "declarou '" << *p << "' mas a fabrica devolveu nulo";
      if (obj != nullptr) obj->unref();
   }
}

TEST(Factory, RecusaNomeDesconhecido)
{
   EXPECT_EQ(mixr::models::xAAA::factory("NaoExiste"), nullptr);
   EXPECT_EQ(mixr::models::xAAA::factory(""), nullptr);
}

TEST(Factory, TodaClasseDeclaradaExportaMetaObject)
{
   std::set<std::string> nomes;
   for (const char* const* p = mixr::models::xAAA::factoryNames(); *p != nullptr; ++p) nomes.insert(*p);

   std::set<std::string> comMeta;
   for (const mixr::base::MetaObject* const* m = mixr::models::xAAA::metaObjects(); *m != nullptr; ++m) {
      comMeta.insert((*m)->getFactoryName());
   }
   EXPECT_EQ(nomes, comMeta) << "factoryNames() e metaObjects() divergiram";
}
