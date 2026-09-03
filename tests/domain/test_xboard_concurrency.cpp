#include "xboard/Board.hpp"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

// shared/xboard e a UNICA shared_library() de shared/ (as outras cinco sao
// estaticas) -- precisamente porque e escrita pelo MODELO (num .so aberto
// com dlopen, ate N threads do pool T/C) e lida pelo HOST (thread de
// background). Ver o "porque" completo em xboard/Board.hpp. Esta e a
// UNICA responsabilidade do modulo -- "ser um mapa seguro sob N
// escritores concorrentes e 1 leitor" -- nunca tinha teste direto: so
// validacao indireta pelos numeros 'dec='/'bt='/'thread=' nos dumps
// end-to-end.

namespace {
constexpr int kThreads{8};
constexpr int kIncrementsPerThread{200000};
constexpr long kExpectedTotal{static_cast<long>(kThreads) * kIncrementsPerThread};
}

TEST(XBoardConcurrency, IncrementosConcorrentesSaoExatos)
{
   constexpr int playerId{7001};   // id proprio deste teste -- nao colide com outros
   std::vector<std::thread> workers;
   for (int t = 0; t < kThreads; t++) {
      workers.emplace_back([] {
         for (int i = 0; i < kIncrementsPerThread; i++) mixr::xboard::bumpDecisionCount(playerId);
      });
   }
   for (auto& w : workers) w.join();

   EXPECT_EQ(mixr::xboard::get(playerId).decisions, kExpectedTotal)
      << kThreads << " threads x " << kIncrementsPerThread
      << " incrementos concorrentes tem que somar EXATO, sem incremento perdido.";
}

TEST(XBoardConcurrency, EscritoresConcorrentesEmChavesDiferentesNaoInterferem)
{
   // Cada thread escreve SO no seu proprio playerId -- prova que insercoes
   // concorrentes em chaves DIFERENTES do std::map (que pode reequilibrar
   // a arvore rubro-negra internamente) nao se corrompem entre si sob o
   // mutex unico.
   constexpr int kBase{7100};
   constexpr int kNumPlayers{8};
   std::vector<std::thread> workers;
   for (int p = kBase; p < kBase + kNumPlayers; p++) {
      workers.emplace_back([p] {
         for (int i = 0; i < 1000; i++) {
            mixr::xboard::setThreadTag(p, p);   // valor previsivel: tag == id
            mixr::xboard::bumpDecisionCount(p);
         }
      });
   }
   for (auto& w : workers) w.join();

   for (int p = kBase; p < kBase + kNumPlayers; p++) {
      const auto r{mixr::xboard::get(p)};
      EXPECT_EQ(r.threadTag, p);
      EXPECT_EQ(r.decisions, 1000);
   }
}

TEST(XBoardConcurrency, GetEmPlayerIdDesconhecidoDevolveDefault)
{
   const auto r{mixr::xboard::get(-999999)};
   EXPECT_EQ(r.label, "--");
   EXPECT_EQ(r.decisions, 0);
   EXPECT_EQ(r.threadTag, -1);
}
