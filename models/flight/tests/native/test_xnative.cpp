//
// Camada 3 do MODELO -- as classes MIXR proprias, SEM levantar Station.
//
// As camadas 1 (domain) e 2 (tree) nao linkam MIXR. Esta linka, e e o preco de
// testar o que de fato herda do framework: slots, tabelas de fabrica e a
// logica de fronteira de fase do datalink.
//
// O que ela NAO faz e levantar simulacao: nada aqui tem WorldModel, Station,
// terreno ou JSBSim. Sao objetos construidos com 'new', slots setados a mao e
// metodos chamados direto -- milissegundos, e o diagnostico aponta para a
// linha errada em vez de para "o cenario divergiu".
//
#include "xnative/AlertDatalink.hpp"
#include "xnative/TacticalAlert.hpp"
#include "xnative/ThreadTag.hpp"
#include "xnative/factory.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Float.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/Times.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace mixr;

//------------------------------------------------------------------------------
// receive() e onDatalinkMessageEvent() sao PROTECTED -- em producao quem os
// chama e a maquinaria de fases do Player. Uma subclasse-sonda os expoe sem
// mudar uma linha da API de producao: alargar o acesso so por causa de teste
// seria pior do que este 'using'.
//------------------------------------------------------------------------------
class SondaDatalink : public xnative::AlertDatalink
{
public:
   using AlertDatalink::onDatalinkMessageEvent;
   using AlertDatalink::receive;
};

//------------------------------------------------------------------------------
// A FABRICA e as duas listas que o descritor do plugin publica.
//
// Hoje essa coerencia so e conferida na CARGA do plugin, pelo
// PluginRegistry -- ou seja, tarde e num teste de integracao. Aqui e
// instantaneo, e e exatamente o que quebra quando alguem acrescenta uma classe
// e esquece de uma das listas.
//------------------------------------------------------------------------------
TEST(Factory, ConstroiTudoQueDeclara)
{
   for (const char* const* p = xnative::factoryNames(); *p != nullptr; ++p) {
      base::Object* const obj{xnative::factory(*p)};
      EXPECT_NE(obj, nullptr) << "declarou '" << *p << "' mas a fabrica devolveu nulo";
      if (obj != nullptr) obj->unref();
   }
}

TEST(Factory, RecusaNomeDesconhecido)
{
   EXPECT_EQ(xnative::factory("NaoExiste"), nullptr);
   EXPECT_EQ(xnative::factory(""), nullptr);
}

TEST(Factory, TodaClasseDeclaradaExportaMetaObject)
{
   std::set<std::string> nomes;
   for (const char* const* p = xnative::factoryNames(); *p != nullptr; ++p) nomes.insert(*p);

   std::set<std::string> comMeta;
   for (const base::MetaObject* const* m = xnative::metaObjects(); *m != nullptr; ++m) {
      comMeta.insert((*m)->getFactoryName());
   }
   EXPECT_EQ(nomes, comMeta) << "factoryNames() e metaObjects() divergiram";
}

//------------------------------------------------------------------------------
// SLOTS -- tipo e unidade.
//
// Ate agora a unica conferencia era o cenario de producao parsear. Um erro de
// unidade (Meters onde se esperava Feet) passaria batido, porque o parser so
// checa o TIPO.
//------------------------------------------------------------------------------
TEST(Slots, AlertDatalinkAceitaHoldTimeEmTempo)
{
   base::Object* const obj{xnative::factory("AlertDatalink")};
   ASSERT_NE(obj, nullptr);

   base::Seconds s{25.0};
   EXPECT_TRUE(obj->setSlotByName("holdTime", &s));

   // Tipo errado tem de ser RECUSADO -- e o que faz o parser somar erro em vez
   // de aceitar um numero cru como se fosse tempo.
   base::Float n{25.0};
   EXPECT_FALSE(obj->setSlotByName("holdTime", &n));

   EXPECT_FALSE(obj->setSlotByName("naoExisteEsteSlot", &s));
   obj->unref();
}

TEST(Slots, BtBehaviorAceitaOsSlotsDoCenario)
{
   base::Object* const obj{xnative::factory("BtBehavior")};
   ASSERT_NE(obj, nullptr);

   base::String arquivo{"./nao/existe.xml"};
   EXPECT_TRUE(obj->setSlotByName("treeFile", &arquivo));

   base::Degrees graus{90.0};
   EXPECT_TRUE(obj->setSlotByName("patrolHeading", &graus));
   EXPECT_TRUE(obj->setSlotByName("legTurn", &graus));

   base::Seconds seg{60.0};
   EXPECT_TRUE(obj->setSlotByName("legTime", &seg));
   EXPECT_TRUE(obj->setSlotByName("evadeHold", &seg));

   base::Meters metros{1750.0};
   EXPECT_TRUE(obj->setSlotByName("patrolAltitude", &metros));
   EXPECT_TRUE(obj->setSlotByName("terrainClearance", &metros));

   base::Float num{160.0};
   EXPECT_TRUE(obj->setSlotByName("patrolSpeed", &num));

   // HERDADO de base::ubf::AbstractBehavior -- o indice dele e resolvido em
   // tempo de execucao contra a tabela viva do MIXR.
   base::Float voto{50.0};
   EXPECT_TRUE(obj->setSlotByName("vote", &voto));

   obj->unref();
}

//------------------------------------------------------------------------------
// A FRONTEIRA DE FASE do datalink.
//
// O cabecalho do AlertDatalink afirma que a promocao numa fronteira de fase da
// latencia FIXA de um frame para todos os receptores. Ate agora isso so era
// exercitado de lado, pelo cenario 'intruder'. Aqui e afirmado.
//------------------------------------------------------------------------------
TEST(AlertDatalink, AlertaSoValeNoFrameSEGUINTE)
{
   auto* const dl = new SondaDatalink();
   ASSERT_NE(dl, nullptr);

   auto* const alerta = static_cast<xnative::TacticalAlert*>(xnative::factory("TacticalAlert"));
   ASSERT_NE(alerta, nullptr);
   alerta->setSender(101, "falcon1");
   alerta->setContactName("bandit1");
   alerta->setRangeM(12000.0);

   EXPECT_FALSE(dl->hasAlert()) << "nasceu com alerta";

   dl->onDatalinkMessageEvent(alerta);
   EXPECT_FALSE(dl->hasAlert()) << "promoveu no MESMO frame -- a latencia deixou de ser fixa";
   EXPECT_EQ(dl->getReceivedCount(), 1);

   dl->receive(0.02);
   EXPECT_TRUE(dl->hasAlert()) << "nao promoveu na fronteira de fase";
   EXPECT_EQ(dl->getAlert().senderName, "falcon1");
   EXPECT_EQ(dl->getAlert().contactName, "bandit1");

   alerta->unref();
   dl->unref();
}

TEST(AlertDatalink, AlertaEnvelheceEExpira)
{
   auto* const dl = new SondaDatalink();
   ASSERT_NE(dl, nullptr);
   base::Seconds hold{1.0};
   ASSERT_TRUE(dl->setSlotByName("holdTime", &hold));

   auto* const alerta = static_cast<xnative::TacticalAlert*>(xnative::factory("TacticalAlert"));
   alerta->setSender(102, "falcon2");
   alerta->setContactName("bandit1");
   dl->onDatalinkMessageEvent(alerta);
   dl->receive(0.02);
   ASSERT_TRUE(dl->hasAlert());

   // Meio segundo: ainda vale.
   for (int i = 0; i < 25; ++i) dl->receive(0.02);
   EXPECT_TRUE(dl->hasAlert()) << "expirou antes do holdTime";

   // Passa de 1 s: expira.
   for (int i = 0; i < 40; ++i) dl->receive(0.02);
   EXPECT_FALSE(dl->hasAlert()) << "nao expirou depois do holdTime";

   alerta->unref();
   dl->unref();
}

TEST(AlertDatalink, MaisProximoVenceOEmpate)
{
   auto* const dl = new SondaDatalink();
   ASSERT_NE(dl, nullptr);

   auto* const longe = static_cast<xnative::TacticalAlert*>(xnative::factory("TacticalAlert"));
   longe->setSender(101, "falcon1"); longe->setContactName("bandit1"); longe->setRangeM(30000.0);
   auto* const perto = static_cast<xnative::TacticalAlert*>(xnative::factory("TacticalAlert"));
   perto->setSender(103, "falcon3"); perto->setContactName("bandit1"); perto->setRangeM(9000.0);

   // Os dois no MESMO frame -- o desempate tem de ser por distancia, e nao
   // pela ordem de chegada, senao o resultado dependeria da thread.
   dl->onDatalinkMessageEvent(longe);
   dl->onDatalinkMessageEvent(perto);
   dl->receive(0.02);

   ASSERT_TRUE(dl->hasAlert());
   EXPECT_EQ(dl->getAlert().senderName, "falcon3") << "o mais proximo tem de vencer";

   longe->unref(); perto->unref(); dl->unref();
}

//------------------------------------------------------------------------------
// ThreadTag -- indice pequeno e ESTAVEL por thread.
//------------------------------------------------------------------------------
TEST(ThreadTag, EstavelNaMesmaThreadEDistintoEntreThreads)
{
   const int meu{xnative::threadTag()};
   EXPECT_EQ(meu, xnative::threadTag()) << "mudou na mesma thread";
   EXPECT_GE(meu, 0);

   int outro{-1};
   std::thread t{[&outro] { outro = xnative::threadTag(); }};
   t.join();
   EXPECT_GE(outro, 0);
   EXPECT_NE(outro, meu) << "duas threads receberam o mesmo indice";
}

} // namespace
