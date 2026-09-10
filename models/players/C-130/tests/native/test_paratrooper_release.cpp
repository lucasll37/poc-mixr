//
// ActionParatrooperRelease -- prova, sem Station, que a busca generica por
// 'type:' funciona: libera as estacoes "PARATROOPER" uma a uma, ignora uma
// estacao de outro tipo, e degrada sem crashar quando nao sobra estacao
// livre.
//
#include "xnative/ActionParatrooperRelease.hpp"
#include "xnative/ParatrooperPlaceholder.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/player/effect/Decoy.hpp"
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
// Bench -- um AirVehicle "de bancada" com OnboardComputer + SimpleStoresMgr
// (2 ParatrooperPlaceholder + 1 Decoy, para provar o filtro por tipo). Mesmo
// padrao de models/players/A-4/tests/native/test_flight_state_action.cpp --
// air->reset() muda useCoordSys de CS_NONE para CS_LOCAL e popula os
// ponteiros de subsistema via updateSystemPointers().
//------------------------------------------------------------------------------
struct Bench
{
   WorldModel* const world;
   AirVehicle* const air;
   OnboardComputer* obc{};

   Bench() : world(new WorldModel()), air(new AirVehicle())
   {
      air->container(world);
      air->setID(9101);

      obc = new OnboardComputer();
      addChild(air, "obc", obc);

      const auto sms = new SimpleStoresMgr();
      addChild(air, "stores", sms);

      base::Integer numStations{3};
      sms->setSlotByName("numStations", &numStations);

      const auto stores = new base::PairStream();
      addStore(*stores, "1", new ParatrooperPlaceholder());   // type "PARATROOPER" (default)
      addStore(*stores, "2", new ParatrooperPlaceholder());   // type "PARATROOPER" (default)
      addStore(*stores, "3", new Decoy());                    // type "Decoy" -- NAO deve ser tocado
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
      obj->unref();   // Pair::Pair() ja deu ref() -- devolve a nossa
      parent->addComponent(pair);
      pair->unref();  // addComponent() ja deu ref() -- devolve a nossa
   }

   static void addStore(base::PairStream& list, const char* const name, base::Object* const obj)
   {
      const auto pair = new base::Pair(name, obj);
      obj->unref();
      list.put(pair);
      pair->unref();
   }
};

TEST(ActionParatrooperRelease, LiberaAsDuasEstacoesDeParatrooperEIgnoraODecoy)
{
   Bench bench;
   ActionParatrooperRelease action;

   EXPECT_TRUE(action.trigger(bench.obc)) << "primeira estacao PARATROOPER livre";
   EXPECT_TRUE(action.trigger(bench.obc)) << "segunda estacao PARATROOPER livre";
   EXPECT_FALSE(action.trigger(bench.obc)) << "nao sobra estacao PARATROOPER -- nunca toca no Decoy";
}

TEST(ActionParatrooperRelease, SemOnboardComputerDevolveFalseSemCrashar)
{
   ActionParatrooperRelease action;
   EXPECT_FALSE(action.trigger(nullptr));
}

} // namespace
