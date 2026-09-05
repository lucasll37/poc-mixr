#include "xtrack/TrackQuery.hpp"

#include <gtest/gtest.h>

// xtrack::selectNearestHostileIndex() -- a regra de selecao de pista
// (filtro por lado + desempate deterministico), separada da travessia
// AirVehicle->OnboardComputer->TrackManager->Track que a alimenta. Regra
// pura: sem Station, sem Player/Track ao vivo -- so os tres campos que a
// decisao de fato le (ver o "porque" em xtrack/TrackQuery.hpp). Alimenta
// models/A4/src/ubf/FlightState.cpp (a percepcao do modelo, que por
// sua vez alimenta a arvore de comportamento inteira).

using mixr::xtrack::TrackCandidate;
using mixr::xtrack::selectNearestHostileIndex;
using Side = mixr::models::Player::Side;

namespace {
constexpr Side kBlue{mixr::models::Player::BLUE};
constexpr Side kRed{mixr::models::Player::RED};
}

TEST(TrackSelection, ListaVaziaNaoEncontraNada)
{
   EXPECT_EQ(selectNearestHostileIndex({}, kBlue), -1);
}

TEST(TrackSelection, SoAmigosNaoEncontraNada)
{
   const std::vector<TrackCandidate> tracks{
      {1, 1000.0, true, kBlue}, {2, 500.0, true, kBlue},
   };
   EXPECT_EQ(selectNearestHostileIndex(tracks, kBlue), -1);
}

TEST(TrackSelection, EscolheOHostilMaisProximoIgnorandoAmigoMaisPerto)
{
   const std::vector<TrackCandidate> tracks{
      {1, 5000.0, true, kRed},    // hostil, longe
      {2, 500.0,  true, kBlue},   // amigo, perto -- tem que ser ignorado
      {3, 1200.0, true, kRed},    // hostil, mais perto que o 1
   };
   EXPECT_EQ(selectNearestHostileIndex(tracks, kBlue), 2);
}

TEST(TrackSelection, EmpateDeAlcanceVenceOMenorIdDePista)
{
   const std::vector<TrackCandidate> tracks{
      {50, 2000.0, true, kRed},
      {7,  2000.0, true, kRed},   // mesmo alcance, id MENOR -- deve vencer
      {99, 2000.0, true, kRed},
   };
   const int idx{selectNearestHostileIndex(tracks, kBlue)};
   ASSERT_GE(idx, 0);
   EXPECT_EQ(tracks[static_cast<std::size_t>(idx)].trackId, 7);
}

TEST(TrackSelection, DesempateEIndependenteDaOrdemDaLista)
{
   // A razao documentada da regra existir: a lista de pistas do
   // TrackManager nao tem ordem garantida entre execucoes com numeros
   // diferentes de threads T/C.
   const std::vector<TrackCandidate> shuffled{
      {99, 2000.0, true, kRed},
      {50, 2000.0, true, kRed},
      {7,  2000.0, true, kRed},
   };
   const int idx{selectNearestHostileIndex(shuffled, kBlue)};
   ASSERT_GE(idx, 0);
   EXPECT_EQ(shuffled[static_cast<std::size_t>(idx)].trackId, 7);
}

TEST(TrackSelection, PistaSemAlvoResolvidoNuncaEFiltradaPorLado)
{
   // 'hasResolvedTarget=false' equivale a 'trk->getTarget() == nullptr' no
   // codigo real -- o filtro de lado so se aplica com alvo resolvido.
   const std::vector<TrackCandidate> tracks{
      {1, 800.0, false, kBlue},   // sem alvo resolvido -- side irrelevante
   };
   EXPECT_EQ(selectNearestHostileIndex(tracks, kBlue), 0);
}

TEST(TrackSelection, PistaSemAlvoResolvidoCompeteNormalmenteContraHostilResolvido)
{
   // Gap de cobertura: nenhum teste ate aqui misturava as tres categorias
   // (amigo resolvido, hostil resolvido, sem alvo resolvido) na MESMA
   // lista. A pista sem alvo resolvido tem de competir por ALCANCE como
   // qualquer hostil -- nao e nem privilegiada nem penalizada por nao ter
   // lado conhecido.
   const std::vector<TrackCandidate> tracks{
      {1, 500.0,  true,  kBlue},   // amigo, mais perto de todos -- ignorado
      {2, 3000.0, true,  kRed},    // hostil, longe
      {3, 1500.0, false, kBlue},   // sem alvo resolvido, mais perto que o hostil
   };
   EXPECT_EQ(selectNearestHostileIndex(tracks, kBlue), 2);
}
