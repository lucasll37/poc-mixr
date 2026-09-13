//
// Beacon -- camada 'native' (a classe MIXR de verdade, SEM levantar Station).
//
// Bench: um mixr::models::WorldModel de bancada com dois Beacon inseridos em
// tempo de execucao via Simulation::addNewPlayer() (publico) -- sem EDL, sem
// Station. Simulation::reset() e' chamado uma vez so' para inicializar a
// lista de players (privada) para uma PairStream vazia -- sem isso,
// addNewPlayer()/updatePlayerList() desreferenciaria um safe_ptr nulo (lido
// no fonte antes de escrever este teste: Simulation::updatePlayerList()
// so' inicializa a lista quando ha algo para trocar, mas Simulation::reset()
// SEMPRE cria uma PairStream nova, mesmo vazia).
//
// world->updateData(dt) sozinho basta para exercitar os DOIS lados do
// evento: Simulation::updateData() chama updatePlayerList() (drena a fila
// de novos players, ja com container()/nome ajustados) e, na MESMA
// chamada, Simulation::updateBgPlayerList() chama player->updateData(dt)
// em cada um -- e' aqui que Beacon::broadcastPing()/onPingEvent() rodam,
// exatamente como no laco de fundo (10 Hz) de qualquer poc deste
// repositorio.
//
// O que este teste NAO cobre, de proposito: os slots (pingInterval/
// pingMessage) sao privados -- so' o parser EDL de verdade os alcanca (via
// setSlotByIndex(), protegido). Isso e' coberto pelo cenario de verdade
// (src/poc/my-event/), nao aqui.
//
#include "Beacon.hpp"
#include "xnative/factory.hpp"
#include "events/payloads/EID_PING/PingMessage.hpp"

#include "mixr/models/WorldModel.hpp"

#include <gtest/gtest.h>

namespace {

using namespace mixr;
using namespace mixr::models::xBeacon;

//------------------------------------------------------------------------------
// Bench -- dois Beacon de bancada, dentro do MESMO WorldModel.
//------------------------------------------------------------------------------
struct Bench
{
   models::WorldModel* const world;
   Beacon* const a;
   Beacon* const b;

   Bench() : world(new models::WorldModel()), a(new Beacon()), b(new Beacon())
   {
      world->reset();               // inicializa a lista de players (vazia)
      world->addNewPlayer("a", a);  // enfileira -- so' entra na lista no
      world->addNewPlayer("b", b);  // proximo updateData()
   }

   ~Bench()
   {
      a->unref();
      b->unref();
      world->unref();
   }

   void tick(const double dt) { world->updateData(dt); }
};

//------------------------------------------------------------------------------
// factory() -- as duas classes que este plugin registra, e so' elas.
//------------------------------------------------------------------------------
TEST(Factory, ConstroiTudoQueDeclara)
{
   for (const char* const* p = factoryNames(); *p != nullptr; ++p) {
      base::Object* const obj{factory(std::string(*p))};
      ASSERT_NE(obj, nullptr) << "factory() nao construiu '" << *p << "'";
      obj->unref();
   }
}

TEST(Factory, NomeDesconhecidoDevolveNulo)
{
   EXPECT_EQ(factory("NomeQueNaoExiste"), nullptr);
}

TEST(Factory, TodaClasseDeclaradaExportaMetaObject)
{
   const char* const* nomes{factoryNames()};
   const base::MetaObject* const* metas{metaObjects()};
   int n{};
   for (; nomes[n] != nullptr; ++n) {
      ASSERT_NE(metas[n], nullptr) << "sem MetaObject para '" << nomes[n] << "'";
   }
   EXPECT_EQ(metas[n], nullptr) << "NOMES[]/METAS[] fora de sincronia (tamanhos diferentes)";
}

//------------------------------------------------------------------------------
// getMajorType() -- o mesmo padrao minimo de mixr::models::Building.
//------------------------------------------------------------------------------
TEST(Beacon, MajorTypeEhBuilding)
{
   Beacon b;
   EXPECT_EQ(b.getMajorType(), models::Player::BUILDING);
}

//------------------------------------------------------------------------------
// Nasce sem ter emitido nem recebido nada.
//------------------------------------------------------------------------------
TEST(Beacon, NasceZerado)
{
   Beacon b;
   EXPECT_EQ(b.getSentCount(), 0u);
   EXPECT_EQ(b.getReceivedCount(), 0u);
}

//------------------------------------------------------------------------------
// O PRIMEIRO updateData() ja dispara o primeiro ping (pingTimer nasce em
// 0.0 -- ver o cabecalho de Beacon::reset()) -- e como os DOIS Beacons desta
// bancada nascem no mesmo estado, o PRIMEIRO tick ja basta para EMITIR e
// TRATAR nos dois sentidos: a emite e b trata, b emite e a trata.
//------------------------------------------------------------------------------
TEST(Beacon, EmiteETrataNoPrimeiroTick)
{
   Bench bench;
   bench.tick(0.1);

   EXPECT_EQ(bench.a->getSentCount(), 1u);
   EXPECT_EQ(bench.b->getSentCount(), 1u);

   ASSERT_EQ(bench.a->getReceivedCount(), 1u);
   ASSERT_EQ(bench.b->getReceivedCount(), 1u);

   // a recebeu de b, e vice-versa -- nao de si mesmo.
   EXPECT_EQ(bench.a->getLastSenderName(), "b");
   EXPECT_EQ(bench.b->getLastSenderName(), "a");
   EXPECT_EQ(bench.a->getLastMessage(), "ping");
   EXPECT_EQ(bench.b->getLastMessage(), "ping");
   EXPECT_EQ(bench.a->getLastSequenceReceived(), 1u);
   EXPECT_EQ(bench.b->getLastSequenceReceived(), 1u);
}

//------------------------------------------------------------------------------
// Depois do primeiro ping, o intervalo default (5 s) tem que decorrer antes
// do segundo -- nem cedo (deriva errada), nem nunca (timer que nao rearma).
// dt=0.5 divide 5.0 exatamente, entao a aritmetica de ponto flutuante nao
// arrisca um tick a mais/a menos por erro de arredondamento.
//------------------------------------------------------------------------------
TEST(Beacon, ReemiteApenasAposOIntervalo)
{
   Bench bench;

   for (int i = 0; i < 9; ++i) bench.tick(0.5);   // t = 0.1 .. 4.5s (10 ticks contando o 1o)
   EXPECT_EQ(bench.a->getSentCount(), 1u) << "nao deveria ter reemitido antes do intervalo";

   bench.tick(0.5);                               // t = 5.0s -- fecha o intervalo
   EXPECT_EQ(bench.a->getSentCount(), 2u);
   EXPECT_EQ(bench.a->getLastSequenceReceived(), 2u);
}

//------------------------------------------------------------------------------
// events::PingMessage -- payload NAO-nulo: os campos sobrevivem a um
// set/get direto, sem depender de Beacon nenhum.
//------------------------------------------------------------------------------
TEST(PingMessage, CamposSobrevivemAoSetGet)
{
   events::PingMessage msg;
   msg.setSender(7, "farol-a");
   msg.setSequence(42);
   msg.setMessage("ola");

   EXPECT_EQ(msg.getSenderId(), 7);
   EXPECT_EQ(msg.getSenderName(), "farol-a");
   EXPECT_EQ(msg.getSequence(), 42u);
   EXPECT_EQ(msg.getMessage(), "ola");
}

} // namespace
