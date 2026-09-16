//
// ActionParatrooperStick -- prova, sem Station, que o mecanismo nativo de
// Action (trigger() uma vez, OnboardComputer::actionManager() chamando
// process(dt) a cada ciclo de fundo ate isCompleted()) libera N estacoes
// espacadas no tempo, a partir de um UNICO trigger().
//
#include "xnative/ActionParatrooperStick.hpp"
#include "xnative/ParatrooperPlaceholder.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/OnboardComputer.hpp"
#include "mixr/models/system/SimpleStoresMgr.hpp"

#include "mixr/base/numeric/Integer.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"

#include <gtest/gtest.h>

namespace {

using namespace mixr;
using namespace mixr::models;
using namespace mixr::models::xC_130;

//------------------------------------------------------------------------------
// Bench -- mesmo padrao de test_paratrooper_release.cpp, com N estacoes
// PARATROOPER (parametrizado, pra testar tanto "sobra carga" quanto "acaba a
// carga no meio da sequencia").
//------------------------------------------------------------------------------
struct Bench
{
   WorldModel* const world;
   AirVehicle* const air;
   OnboardComputer* obc{};

   explicit Bench(const int numStations)
      : world(new WorldModel()), air(new AirVehicle())
   {
      air->container(world);
      air->setID(9201);

      obc = new OnboardComputer();
      addChild(air, "obc", obc);

      const auto sms = new SimpleStoresMgr();
      addChild(air, "stores", sms);

      base::Integer n{numStations};
      sms->setSlotByName("numStations", &n);

      const auto stores = new base::PairStream();
      for (int i = 1; i <= numStations; ++i) {
         addStore(*stores, std::to_string(i).c_str(), new ParatrooperPlaceholder());
      }
      sms->setSlotByName("stores", stores);
      stores->unref();

      air->reset();
   }

   ~Bench()
   {
      air->unref();
      world->unref();
   }

private:
   static void addChild(base::Component* const parent, const char* const name, base::Object* const obj)
   {
      const auto pair = new base::Pair(name, obj);
      obj->unref();
      parent->addComponent(pair);
      pair->unref();
   }

   static void addStore(base::PairStream& list, const char* const name, base::Object* const obj)
   {
      const auto pair = new base::Pair(name, obj);
      obj->unref();
      list.put(pair);
      pair->unref();
   }
};

// setSlotByName() so' LE o valor (extrai getInt()/getReal()/etc.) -- os
// objetos de slot aqui sao de vida curta, na PILHA, igual ao 'base::Integer
// numStations{3}' que o proprio Bench acima ja usa: nunca precisam de
// ref()/unref() manual.
void setCount(ActionParatrooperStick& action, const int n)
{
   base::Integer v{n};
   action.setSlotByName("count", &v);
}

void setInterval(ActionParatrooperStick& action, const int seconds)
{
   base::Integer v{seconds};
   action.setSlotByName("interval", &v);
}

TEST(ActionParatrooperStick, TriggerLiberaOPrimeiroNaHoraEFicaEmProgresso)
{
   Bench bench(10);
   ActionParatrooperStick action;
   setCount(action, 5);

   EXPECT_TRUE(action.trigger(bench.obc));
   EXPECT_EQ(1, action.getReleasedSoFar());
   EXPECT_FALSE(action.isCompleted());
   EXPECT_TRUE(action.isInProgress());
}

TEST(ActionParatrooperStick, ProcessAntesDoIntervaloNaoLiberaOutro)
{
   Bench bench(10);
   ActionParatrooperStick action;
   setCount(action, 3);
   setInterval(action, 2);   // 2 segundos

   action.trigger(bench.obc);
   ASSERT_EQ(1, action.getReleasedSoFar());

   action.process(1.0);   // 1s < 2s -- nao libera ainda
   EXPECT_EQ(1, action.getReleasedSoFar());
   EXPECT_FALSE(action.isCompleted());
}

TEST(ActionParatrooperStick, ProcessAposOIntervaloLiberaOProximo)
{
   Bench bench(10);
   ActionParatrooperStick action;
   setCount(action, 3);
   setInterval(action, 2);

   action.trigger(bench.obc);
   action.process(1.0);
   action.process(1.5);   // acumulado 2.5s >= 2s -- libera o segundo
   EXPECT_EQ(2, action.getReleasedSoFar());
   EXPECT_FALSE(action.isCompleted());

   action.process(2.0);   // libera o terceiro (e ultimo) -- completa
   EXPECT_EQ(3, action.getReleasedSoFar());
   EXPECT_TRUE(action.isCompleted());
}

TEST(ActionParatrooperStick, UmProcessQuePulaVariosIntervalosLiberaVarios)
{
   // Um dt grande o suficiente pra cruzar MAIS de um intervalo de uma vez
   // tem que liberar todos os que couberem, nao so um por chamada.
   Bench bench(10);
   ActionParatrooperStick action;
   setCount(action, 5);
   setInterval(action, 1);

   action.trigger(bench.obc);            // libera 1
   action.process(10.0);                 // 10s / 1s -- da pra liberar os 4 restantes
   EXPECT_EQ(5, action.getReleasedSoFar());
   EXPECT_TRUE(action.isCompleted());
}

TEST(ActionParatrooperStick, AcabaACargaNoMeioCompletaCedoSemFicarPreso)
{
   Bench bench(3);   // so 3 estacoes, count pede 10
   ActionParatrooperStick action;
   setCount(action, 10);
   setInterval(action, 1);

   action.trigger(bench.obc);   // libera 1/3
   action.process(1.0);         // libera 2/3
   action.process(1.0);         // libera 3/3 -- acabou a carga
   EXPECT_EQ(3, action.getReleasedSoFar());

   action.process(1.0);         // nao ha mais estacao -- degrada, completa
   EXPECT_TRUE(action.isCompleted());
}

TEST(ActionParatrooperStick, SemOnboardComputerDevolveFalseSemCrashar)
{
   ActionParatrooperStick action;
   setCount(action, 5);
   EXPECT_FALSE(action.trigger(nullptr));
   EXPECT_TRUE(action.isCompleted());
}

TEST(ActionParatrooperStick, CountZeroCompletaSemLiberarNada)
{
   Bench bench(10);
   ActionParatrooperStick action;
   setCount(action, 0);

   EXPECT_FALSE(action.trigger(bench.obc));
   EXPECT_EQ(0, action.getReleasedSoFar());
   EXPECT_TRUE(action.isCompleted());
}

} // namespace
