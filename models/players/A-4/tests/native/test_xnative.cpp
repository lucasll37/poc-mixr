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
#include "ubf/BtBehavior.hpp"
#include "xnative/AlertDatalink.hpp"
#include "events/payloads/EID_ALERT/TacticalAlert.hpp"
#include "xnative/factory.hpp"

#include "bt/bt_factory.hpp"
#include "bt/bt_factory_sdk.hpp"

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

namespace {

using namespace mixr;
using namespace mixr::models;

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

   base::Degrees jitter{6.0};
   EXPECT_TRUE(obj->setSlotByName("patrolJitterHeading", &jitter));

   base::Float semente{20260903.0};
   EXPECT_TRUE(obj->setSlotByName("patrolMasterSeed", &semente));
   EXPECT_TRUE(obj->setSlotByName("patrolSeedOverride", &semente));

   obj->unref();
}

//------------------------------------------------------------------------------
// Hierarquia de sementes -- override tem prioridade sobre a derivacao pelo
// master seed. reset() (publico, chama configurePlans() por dentro) nao
// precisa de Station/Player nenhum: sem container, o nome do player cai no
// fallback "" -- e por isso os dois lados desta comparacao usam o MESMO
// nome implicito, o que isola exatamente a variavel que importa (override
// presente ou nao), sem depender de simulação nenhuma.
//------------------------------------------------------------------------------
TEST(Slots, PatrolSeedOverrideAusenteUsaDerivacaoDoMaster)
{
   auto* const a = static_cast<xnative::BtBehavior*>(xnative::factory("BtBehavior"));
   auto* const b = static_cast<xnative::BtBehavior*>(xnative::factory("BtBehavior"));
   ASSERT_NE(a, nullptr);
   ASSERT_NE(b, nullptr);

   base::Degrees jitter{10.0};
   base::Float masterSeed{999.0};
   for (auto* const obj : {a, b}) {
      obj->setSlotByName("patrolJitterHeading", &jitter);
      obj->setSlotByName("patrolMasterSeed", &masterSeed);
   }

   a->reset();
   b->reset();

   EXPECT_NEAR(a->patrolPlan().command().headingDeg, b->patrolPlan().command().headingDeg, 1e-9)
      << "mesma master seed, sem override -- tem de reproduzir o mesmo jitter";

   a->unref();
   b->unref();
}

TEST(Slots, PatrolSeedOverridePresenteTemPrioridadeSobreOMaster)
{
   auto* const semOverride = static_cast<xnative::BtBehavior*>(xnative::factory("BtBehavior"));
   auto* const comOverride = static_cast<xnative::BtBehavior*>(xnative::factory("BtBehavior"));
   ASSERT_NE(semOverride, nullptr);
   ASSERT_NE(comOverride, nullptr);

   base::Degrees jitter{10.0};
   base::Float mesmaMasterSeed{999.0};
   base::Float override{42.0};

   semOverride->setSlotByName("patrolJitterHeading", &jitter);
   semOverride->setSlotByName("patrolMasterSeed", &mesmaMasterSeed);

   comOverride->setSlotByName("patrolJitterHeading", &jitter);
   comOverride->setSlotByName("patrolMasterSeed", &mesmaMasterSeed);
   comOverride->setSlotByName("patrolSeedOverride", &override);

   semOverride->reset();
   comOverride->reset();

   EXPECT_NE(semOverride->patrolPlan().command().headingDeg,
             comOverride->patrolPlan().command().headingDeg)
      << "com a MESMA master seed, o override tinha de mudar o resultado";

   semOverride->unref();
   comOverride->unref();
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

   auto* const alerta = static_cast<events::TacticalAlert*>(xnative::factory("TacticalAlert"));
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

   auto* const alerta = static_cast<events::TacticalAlert*>(xnative::factory("TacticalAlert"));
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

   auto* const longe = static_cast<events::TacticalAlert*>(xnative::factory("TacticalAlert"));
   longe->setSender(101, "falcon1"); longe->setContactName("bandit1"); longe->setRangeM(30000.0);
   auto* const perto = static_cast<events::TacticalAlert*>(xnative::factory("TacticalAlert"));
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

// ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): o teste acima
// ("MaisProximoVenceOEmpate") nunca exercitava um EMPATE de verdade --
// 'longe'/'perto' tem rangeM sempre DIFERENTE (30000 vs 9000). O terceiro
// termo do OR em AlertDatalink.cpp (desempate por senderId em EMPATE EXATO
// de rangeM) nunca era testado por nenhum teste do repositorio -- trocar
// '<' por '<='/'>' ali, ou remover o termo (reintroduzindo dependencia de
// ordem de chegada entre threads, exatamente o defeito que este codigo
// existe para evitar, ver AlertDatalink.hpp:59-66), nao quebrava
// 'make test'. Mesmo padrao ja usado em tests/domain/test_track_selection.cpp
// (EmpateDeAlcanceVenceOMenorIdDePista/DesempateEIndependenteDaOrdemDaLista)
// para a regra identica, so' nao tinha sido replicado pra AlertDatalink.
TEST(AlertDatalink, EmpateExatoDeAlcanceVenceOMenorSenderId)
{
   auto* const dl = new SondaDatalink();
   ASSERT_NE(dl, nullptr);

   auto* const idMaior = static_cast<events::TacticalAlert*>(xnative::factory("TacticalAlert"));
   idMaior->setSender(103, "falcon3"); idMaior->setContactName("bandit1"); idMaior->setRangeM(9000.0);
   auto* const idMenor = static_cast<events::TacticalAlert*>(xnative::factory("TacticalAlert"));
   idMenor->setSender(101, "falcon1"); idMenor->setContactName("bandit1"); idMenor->setRangeM(9000.0);

   // MESMO rangeM (9000.0, empate exato) -- o desempate tem de ser por
   // menor senderId, e nao pela ordem de chegada.
   dl->onDatalinkMessageEvent(idMaior);
   dl->onDatalinkMessageEvent(idMenor);
   dl->receive(0.02);

   ASSERT_TRUE(dl->hasAlert());
   EXPECT_EQ(dl->getAlert().senderId, 101) << "em empate exato de alcance, o menor senderId tem de vencer";
   EXPECT_EQ(dl->getAlert().senderName, "falcon1");

   idMaior->unref(); idMenor->unref(); dl->unref();
}

// Mesma propriedade, ORDEM DE CHEGADA invertida -- prova que o resultado
// nao depende de qual TacticalAlert foi entregue primeiro (a mesma garantia
// de independencia de ordem que test_track_selection.cpp ja prova para a
// selecao de pista de radar).
TEST(AlertDatalink, EmpateExatoDeAlcanceEIndependenteDaOrdemDeChegada)
{
   auto* const dl = new SondaDatalink();
   ASSERT_NE(dl, nullptr);

   auto* const idMaior = static_cast<events::TacticalAlert*>(xnative::factory("TacticalAlert"));
   idMaior->setSender(103, "falcon3"); idMaior->setContactName("bandit1"); idMaior->setRangeM(9000.0);
   auto* const idMenor = static_cast<events::TacticalAlert*>(xnative::factory("TacticalAlert"));
   idMenor->setSender(101, "falcon1"); idMenor->setContactName("bandit1"); idMenor->setRangeM(9000.0);

   // Ordem invertida em relacao ao teste acima.
   dl->onDatalinkMessageEvent(idMenor);
   dl->onDatalinkMessageEvent(idMaior);
   dl->receive(0.02);

   ASSERT_TRUE(dl->hasAlert());
   EXPECT_EQ(dl->getAlert().senderId, 101) << "o menor senderId vence, mesmo chegando primeiro";

   idMaior->unref(); idMenor->unref(); dl->unref();
}

//------------------------------------------------------------------------------
// bt_nodes::registerNodes()/registerSdkNodes() -- o que BtBehavior::
// buildTree() (privado, so' alcancavel por genAction() com um FlightState
// de verdade -- fora do escopo desta camada, ver o cabecalho de
// test_rl_bridge_behavior.cpp para o mesmo limite) chama a cada
// reconstrucao da arvore. Testado direto nesta camada, sem BtBehavior nem
// Station: os dois testes exercitam a MESMA BT::BehaviorTreeFactory que
// buildTree() usa, sem precisar do resto da maquina.
//
// DEBITO DE TESTE RECONHECIDO (mesmo espirito do de test_rl_bridge_
// behavior.cpp, nao redescobrir): os dois testes abaixo provam o
// MECANISMO (registrar duas vezes sem resetar lanca; reatribuir a factory
// evita) contra uma 'BT::BehaviorTreeFactory' LOCAL, nao contra o membro
// 'btFactory' de um 'BtBehavior' de verdade -- reverter o fix em
// 'BtBehavior::buildTree()' NAO faz estes dois testes falharem (foi
// verificado rodando: reverter e rodar so' pega os dois testes locais
// intactos). Fechar essa lacuna de verdade exigiria o mesmo Bench pesado
// (WorldModel+AirVehicle+reset()) que test_flight_state_action.cpp usa,
// aplicado a um SEGUNDO ciclo reset()+genAction().
//
// INVESTIGADO E NAO REPRODUZIDO hoje (verificado rodando, nao so' lido):
// 'src/rl/tests/test_smoke.py' chama env.reset() varias vezes no MESMO
// processo, e um repro isolado (reset -> step x3 -> reset -> step) TAMBEM
// nao lanca -- 'BtBehavior::reset()' parece nunca ser chamado uma segunda
// vez pelo cascade de reset() do 'UbfArbiter' nativo (a mesma incerteza ja
// registrada no comentario de 'genAction()', motivo do 'plansReady'
// preguicoso). O fix e' cautela defensiva -- mesma classe de garantia que
// 'tree = BT::Tree();' ja da' pra 'tree' em reset() -- nao a correcao de
// um crash observado em producao.
//------------------------------------------------------------------------------

// ACHADO POR AUDITORIA (nao redescobrir): BT::BehaviorTreeFactory::
// registerBuilder() lanca BehaviorTreeException("ID [...] already
// registered") pra qualquer ID ja presente (bt_factory.cpp:92, BT.CPP). Um
// SEGUNDO 'buildTree()' na MESMA instancia de BtBehavior (se algum dia
// 'treeBuilt' voltar a 'false' antes de outra decisao) chamaria
// registerNodes()/registerSdkNodes() de novo sobre a MESMA factory, nunca
// resetada. Este teste prova o MECANISMO:
// registrar duas vezes sem reatribuir a factory lanca.
TEST(BtFactoryRegistration, RegistrarDuasVezesNaMesmaFactorySemResetarLanca)
{
   BT::BehaviorTreeFactory factory;
   const bt_nodes::NodeContext context;
   bt_nodes::registerNodes(factory, context);
   bt_nodes::registerSdkNodes(factory, context);

   EXPECT_THROW(bt_nodes::registerNodes(factory, context), BT::BehaviorTreeException);
}

// O fix em BtBehavior::buildTree(): reatribuir 'btFactory = BT::
// BehaviorTreeFactory();' antes de registrar de novo torna a operacao
// idempotente -- MESMO padrao ja usado em reset() para 'tree'
// ('tree = BT::Tree();').
TEST(BtFactoryRegistration, ReatribuirUmaFactoryNovaAntesDeRegistrarDeNovoNaoLanca)
{
   BT::BehaviorTreeFactory factory;
   const bt_nodes::NodeContext context;
   bt_nodes::registerNodes(factory, context);
   bt_nodes::registerSdkNodes(factory, context);

   factory = BT::BehaviorTreeFactory();
   EXPECT_NO_THROW({
      bt_nodes::registerNodes(factory, context);
      bt_nodes::registerSdkNodes(factory, context);
   });
}

} // namespace
