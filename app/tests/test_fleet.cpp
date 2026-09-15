//
// app/Fleet.hpp -- achar players por nome, montar a Fleet nomeada, descobrir
// TODOS os players do cenario, e o empurrao inicial de manete.
//
// Precisa de um WorldModel de verdade com players REGISTRADOS (via
// setSlotPlayers(), o mesmo caminho que o parser EDL usa) -- findAircraft()/
// discoverPlayers() leem wm->getPlayers(), que so existe depois disso. Nao
// precisa de reset() em nenhum player: nada aqui toca posicao/altitude, so
// a lista e o tipo de cada um.
//
#include "app/Fleet.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>

namespace {

using namespace mixr;

//------------------------------------------------------------------------------
// WorldModel com players REGISTRADOS de antemao (nome -> objeto), no mesmo
// mecanismo que o parser EDL usa (Simulation::setSlotPlayers()).
//------------------------------------------------------------------------------
struct FleetBench
{
   models::WorldModel* const world{new models::WorldModel()};

   // Devolve o AirVehicle recem-criado (util para comparar ponteiro depois).
   models::AirVehicle* addAircraft(const char* const name)
   {
      const auto air = new models::AirVehicle();
      putPlayer(name, air);
      return air;
   }

   // Um Player QUALQUER, deliberadamente NAO AirVehicle -- para testar que
   // findAircraft()/applyCruiseThrottle() nao confundem tipo.
   models::Player* addGenericPlayer(const char* const name)
   {
      const auto player = new models::Player();
      putPlayer(name, player);
      return player;
   }

   ~FleetBench() { world->unref(); }

private:
   // setSlotPlayers() e' PRIVADO (e' um "slot table helper method" -- so
   // acessivel via a maquinaria de slot do proprio MIXR, mesmo padrao ja
   // usado por dl->setSlotByName("holdTime", ...) em test_xnative.cpp).
   // O nome do slot ("players") vem de Simulation.cpp: BEGIN_SLOTTABLE.
   void putPlayer(const char* const name, models::Player* const player)
   {
      const auto pair = new base::Pair(name, player);
      player->unref();   // Pair::Pair() ja deu ref() -- devolve a nossa

      auto* const pairs = (existing != nullptr) ? existing : new base::PairStream();
      pairs->put(pair);
      pair->unref();      // PairStream::put()/List::addTail() ja deu ref()

      world->setSlotByName("players", pairs);   // reaplica a lista inteira
      if (existing == nullptr) existing = pairs;
   }

   base::PairStream* existing{};
};

//------------------------------------------------------------------------------
// findAircraft()
//------------------------------------------------------------------------------
TEST(FindAircraft, EncontraPorNome)
{
   FleetBench bench;
   models::AirVehicle* const falcon1{bench.addAircraft("falcon1")};
   bench.addAircraft("falcon2");

   EXPECT_EQ(app::findAircraft(bench.world, "falcon1"), falcon1);
}

TEST(FindAircraft, NomeInexistenteDevolveNulo)
{
   FleetBench bench;
   bench.addAircraft("falcon1");

   EXPECT_EQ(app::findAircraft(bench.world, "fantasma"), nullptr);
}

TEST(FindAircraft, NomeExisteMasNaoEhAirVehicleDevolveNulo)
{
   FleetBench bench;
   bench.addGenericPlayer("generico");

   EXPECT_EQ(app::findAircraft(bench.world, "generico"), nullptr);
}

TEST(FindAircraft, WorldModelSemPlayersDevolveNulo)
{
   const auto world = new models::WorldModel();
   EXPECT_EQ(app::findAircraft(world, "qualquer"), nullptr);
   world->unref();
}

//------------------------------------------------------------------------------
// collectFleet() -- monta na ORDEM dos nomes pedidos, encerra o processo se
// faltar um.
//------------------------------------------------------------------------------
TEST(CollectFleet, MontaNaOrdemDosNomesPedidos)
{
   FleetBench bench;
   models::AirVehicle* const falcon1{bench.addAircraft("falcon1")};
   models::AirVehicle* const falcon2{bench.addAircraft("falcon2")};

   const app::Fleet fleet{app::collectFleet(bench.world, {"falcon2", "falcon1"})};

   ASSERT_EQ(fleet.size(), 2u);
   EXPECT_EQ(fleet[0], falcon2) << "a ordem da Fleet tem que seguir a ordem pedida, nao a de registro";
   EXPECT_EQ(fleet[1], falcon1);
}

TEST(CollectFleet, NomeInexistenteEncerraOProcesso)
{
   FleetBench bench;
   bench.addAircraft("falcon1");

   // collectFleet() e' documentado como fatal de proposito (Fleet.hpp): um
   // nome faltando e' erro de configuracao, nao algo para seguir sem.
   EXPECT_EXIT(app::collectFleet(bench.world, {"fantasma"}),
               ::testing::ExitedWithCode(EXIT_FAILURE), "");
}

//------------------------------------------------------------------------------
// discoverPlayers() -- TODOS os players, de QUALQUER tipo, sem lista de
// nomes (a fonte da aba Frota/Mapa e do '-folder' generico).
//------------------------------------------------------------------------------
TEST(DiscoverPlayers, DevolveTodosOsPlayersIndependenteDoTipo)
{
   FleetBench bench;
   models::AirVehicle* const falcon1{bench.addAircraft("falcon1")};
   models::Player* const generico{bench.addGenericPlayer("carga")};

   const auto players = app::discoverPlayers(bench.world);

   ASSERT_EQ(players.size(), 2u);
   EXPECT_NE(std::find(players.begin(), players.end(), falcon1), players.end());
   EXPECT_NE(std::find(players.begin(), players.end(), generico), players.end())
      << "um player nao-AirVehicle nao pode ficar de fora -- e o widening documentado em Fleet.hpp";
}

TEST(DiscoverPlayers, WorldModelSemPlayersDevolveVazio)
{
   const auto world = new models::WorldModel();
   EXPECT_TRUE(app::discoverPlayers(world).empty());
   world->unref();
}

TEST(DiscoverFleet, EAliasDeDiscoverPlayers)
{
   FleetBench bench;
   bench.addAircraft("falcon1");
   bench.addGenericPlayer("carga");

   EXPECT_EQ(app::discoverFleet(bench.world).size(), app::discoverPlayers(bench.world).size());
}

//------------------------------------------------------------------------------
// applyCruiseThrottle() -- so' AirVehicle tem manete; um Player generico
// (ex.: um Paratrooper) tem que ser ignorado, nunca causar crash.
//------------------------------------------------------------------------------
TEST(ApplyCruiseThrottle, IgnoraPlayerQueNaoEhAirVehicleSemCrashar)
{
   FleetBench bench;
   const app::Fleet fleet{bench.addAircraft("falcon1"), bench.addGenericPlayer("carga")};

   EXPECT_NO_FATAL_FAILURE(app::applyCruiseThrottle(fleet, 0.95));
}

} // namespace
