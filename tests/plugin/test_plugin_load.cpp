//
// O CONTRATO de carga dinamica do MODELO, travado em vermelho/verde.
//
// Este binario NAO linka codigo de poc nenhuma -- so mixr_dep e o contrato de
// shared/xplugin. Isso e parte do que se afirma: o modelo nao depende da
// aplicacao, so das classes base do MIXR.
//
// O que aqui e afirmacao e nao fe:
//
//   * o ponto de entrada existe e o descritor e coerente;
//   * as 6 classes que o cenario nomeia sao construiveis pela .so;
//   * RTTI e dynamic_cast ATRAVESSAM a fronteira do .so, mesmo com o modelo
//     compilado com -fvisibility=hidden e aberto com RTLD_LOCAL. Esta e a
//     assercao mais importante do arquivo: e o "gate" da decisao de esconder
//     os simbolos. Se ela ficar vermelha, tire gnu_symbol_visibility:'hidden'
//     do alvo do modelo -- o RTLD_LOCAL sozinho ja cobre o essencial;
//   * slot PROPRIO e slot HERDADO resolvem os dois. O herdado e o que prova
//     que o offset da base e calculado em TEMPO DE EXECUCAO (macros.hpp:305),
//     e por isso o registro NAO usa contagem de slots como guarda de ABI.
//
#include "xplugin/PluginAbi.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Float.hpp"
#include "mixr/base/ubf/AbstractBehavior.hpp"
#include "mixr/base/ubf/AbstractState.hpp"
#include "mixr/base/units/Times.hpp"
#include "mixr/models/system/Datalink.hpp"

#include <gtest/gtest.h>

#include <dlfcn.h>

#include <set>
#include <string>
#include <typeinfo>

namespace {

using mixr::xplugin::PluginDescV1;

// As classes que o cenario de producao nomeia. FlightAgentTC nao entra: so a
// multi-thread o tem, e esta camada roda contra o modelo da single-thread.
const char* const ESPERADAS[] = {
   "AlertDatalink", "TacticalAlert", "FlightState",
   "BtBehavior", "AltitudeSafetyBehavior", "FlightAction",
};

class Modelo : public ::testing::Test
{
protected:
   void SetUp() override
   {
      // As MESMAS flags do PluginRegistry -- se o teste usasse outras, nao
      // estaria testando o que roda em producao.
      handle = ::dlopen(MODEL_PLUGIN_SO, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
      ASSERT_NE(handle, nullptr) << ::dlerror();

      ::dlerror();
      void* const sym{::dlsym(handle, "mixr_plugin_v1")};
      const char* const err{::dlerror()};
      ASSERT_EQ(err, nullptr) << err;
      ASSERT_NE(sym, nullptr);

      using entry_fn = const PluginDescV1* (*)();
      desc = reinterpret_cast<entry_fn>(sym)();
      ASSERT_NE(desc, nullptr);
   }
   // Sem dlclose no TearDown, de proposito: mesma politica do PluginRegistry,
   // pelo mesmo motivo (os MetaObject/SlotTable estaticos das classes moram
   // dentro da imagem).
   void* handle{};
   const PluginDescV1* desc{};
};

// --- o descritor -------------------------------------------------------------

TEST_F(Modelo, DescritorBateComOHost)
{
   EXPECT_EQ(desc->abi, mixr::xplugin::PLUGIN_ABI);
   EXPECT_EQ(desc->struct_size, sizeof(PluginDescV1));
   EXPECT_EQ(desc->mixr_version, static_cast<std::uint32_t>(MIXR_VERSION));
   EXPECT_EQ(desc->cxx11_abi, static_cast<std::uint32_t>(MIXR_PLUGIN_CXX11_ABI));
   EXPECT_STREQ(desc->plugin_name, "flight");
   EXPECT_NE(desc->factory, nullptr);
}

TEST_F(Modelo, DeclaraAsClassesQueOCenarioNomeia)
{
   ASSERT_NE(desc->factory_names, nullptr);

   std::set<std::string> declaradas;
   for (const char* const* p = desc->factory_names; *p != nullptr; ++p) declaradas.insert(*p);

   for (const char* nome : ESPERADAS) {
      EXPECT_EQ(declaradas.count(nome), 1u) << "o modelo nao declara '" << nome << "'";
   }
}

TEST_F(Modelo, ConstroiTudoQueDeclaraENadaAlem)
{
   for (const char* const* p = desc->factory_names; *p != nullptr; ++p) {
      mixr::base::Object* const obj{desc->factory(*p)};
      EXPECT_NE(obj, nullptr) << "declarou '" << *p << "' mas a fabrica devolveu nulo";
      if (obj != nullptr) obj->unref();
   }
   EXPECT_EQ(desc->factory("NaoExiste"), nullptr);
   EXPECT_EQ(desc->factory(nullptr), nullptr);
}

// --- O GATE: RTTI atravessando a fronteira do .so ----------------------------

TEST_F(Modelo, RttiAtravessaAFronteiraDoSo)
{
   mixr::base::Object* const beh{desc->factory("BtBehavior")};
   ASSERT_NE(beh, nullptr);
   // Funciona porque typeinfo/vtable das classes base sao WEAK com visibilidade
   // default em libmixr_base.so, e porque type_info::operator== cai em strcmp
   // no ELF -- por isso RTLD_LOCAL basta, ao contrario do que o comentario
   // herdado do POCO no BehaviorTree.CPP sugere.
   EXPECT_NE(dynamic_cast<mixr::base::ubf::AbstractBehavior*>(beh), nullptr);
   EXPECT_TRUE(beh->isClassType(typeid(mixr::base::ubf::AbstractBehavior)));
   EXPECT_TRUE(beh->isFactoryName("BtBehavior"));
   beh->unref();

   mixr::base::Object* const st{desc->factory("FlightState")};
   ASSERT_NE(st, nullptr);
   EXPECT_NE(dynamic_cast<mixr::base::ubf::AbstractState*>(st), nullptr);
   st->unref();

   // Este e o que mais importa na pratica: Player::processComponents casa o
   // datalink do aviao por findByType(typeid(models::Datalink)).
   mixr::base::Object* const dl{desc->factory("AlertDatalink")};
   ASSERT_NE(dl, nullptr);
   EXPECT_NE(dynamic_cast<mixr::models::Datalink*>(dl), nullptr);
   EXPECT_TRUE(dl->isClassType(typeid(mixr::models::Datalink)));
   dl->unref();
}

// --- os slots ----------------------------------------------------------------

TEST_F(Modelo, SlotProprioEHerdadoResolvem)
{
   mixr::base::Object* const beh{desc->factory("BtBehavior")};
   ASSERT_NE(beh, nullptr);

   // proprio (indice local 1 na tabela do BtBehavior)
   mixr::base::String arquivo{"./nao/existe.xml"};
   EXPECT_TRUE(beh->setSlotByName("treeFile", &arquivo));

   // HERDADO de base::ubf::AbstractBehavior -- o indice dele e resolvido em
   // runtime, dentro da .so, contra a tabela viva do libmixr_base.so.
   mixr::base::Float voto{50.0};
   EXPECT_TRUE(beh->setSlotByName("vote", &voto));

   EXPECT_FALSE(beh->setSlotByName("naoExisteEsteSlot", &voto));
   beh->unref();

   mixr::base::Object* const dl{desc->factory("AlertDatalink")};
   ASSERT_NE(dl, nullptr);
   mixr::base::Seconds hold{25.0};
   EXPECT_TRUE(dl->setSlotByName("holdTime", &hold));
   dl->unref();
}

// --- os MetaObject exportados ------------------------------------------------

// Sem este campo, app/MetaObjectReport ficaria cego para o modelo INTEIRO --
// reportClass<T>() e template e getMetaObject() e estatica, nao virtual.
TEST_F(Modelo, MetaObjectsCobremAsClassesDeclaradas)
{
   ASSERT_NE(desc->metas, nullptr);

   std::set<std::string> comMeta;
   for (const mixr::base::MetaObject* const* m = desc->metas; *m != nullptr; ++m) {
      comMeta.insert((*m)->getFactoryName());
   }
   for (const char* const* p = desc->factory_names; *p != nullptr; ++p) {
      EXPECT_EQ(comMeta.count(*p), 1u) << "'" << *p << "' nao exporta MetaObject";
   }
}

TEST_F(Modelo, MetaObjectContaInstancias)
{
   const mixr::base::MetaObject* alvo{};
   for (const mixr::base::MetaObject* const* m = desc->metas; *m != nullptr; ++m) {
      if (std::string{(*m)->getFactoryName()} == "FlightAction") alvo = *m;
   }
   ASSERT_NE(alvo, nullptr);

   const int antes{alvo->count};
   mixr::base::Object* const obj{desc->factory("FlightAction")};
   ASSERT_NE(obj, nullptr);
   EXPECT_EQ(alvo->count, antes + 1);
   obj->unref();
   EXPECT_EQ(alvo->count, antes);
}

} // namespace
