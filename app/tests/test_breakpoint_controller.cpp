#include "app/BreakpointController.hpp"

#include <gtest/gtest.h>

#include <cctype>

// A maquina de estados do breakpoint de arvore de comportamento --
// extraida de app/DashboardLoop.cpp (struct Breakpoint + doArmBreakpoint/
// doCancelBreakpoint/o laco de checagem de simThread/buildBreakpointStatus)
// pra app/BreakpointController.hpp. Testada isolada, sem Station/
// ClockStation/FTXUI -- o comportamento do breakpoint hoje so era
// validado manualmente, rodando o dashboard sob pty.

using namespace app;

namespace {

bool fakeMatches(const std::string& tag, const std::string& label)
{
   if (label.empty() || label == "--") return false;
   auto upper = [](std::string s) {
      for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      return s;
   };
   const std::string t{upper(tag)}, l{upper(label)};
   return t.find(l) != std::string::npos || l.find(t) != std::string::npos;
}

} // namespace

TEST(BreakpointController, TickSemArmarNuncaDispara)
{
   BreakpointController bp;
   const auto result{bp.tick({{1, "PATROL"}}, fakeMatches, 10.0)};
   EXPECT_EQ(result.outcome, BreakpointOutcome::None);
   EXPECT_FALSE(bp.isArmed());
}

TEST(BreakpointController, ArmarEDispararNoHit)
{
   BreakpointController bp;
   bp.arm(42, "falcon1", "SupportAlert", false, 1.0);
   EXPECT_TRUE(bp.isArmed());
   EXPECT_TRUE(bp.isArmedOn("SupportAlert"));

   auto miss{bp.tick({{99, "PATROL"}}, fakeMatches, 5.5)};
   EXPECT_EQ(miss.outcome, BreakpointOutcome::None);
   EXPECT_TRUE(bp.isArmed());

   miss = bp.tick({{42, "PATROL"}}, fakeMatches, 5.6);
   EXPECT_EQ(miss.outcome, BreakpointOutcome::None);

   const auto hit{bp.tick({{42, "SUPPORT"}}, fakeMatches, 6.0)};
   EXPECT_EQ(hit.outcome, BreakpointOutcome::Hit);
   EXPECT_TRUE(hit.shouldPause);
   EXPECT_FALSE(hit.shouldRestoreScale);
   EXPECT_DOUBLE_EQ(hit.simSecAtOutcome, 6.0);
   EXPECT_FALSE(bp.isArmed());

   const auto st{bp.status(true, true, "SupportAlert")};
   EXPECT_EQ(st.branch, BreakpointStatusBranch::Hit);
}

TEST(BreakpointController, ModoRapidoRestauraEscalaNoHit)
{
   BreakpointController bp;
   bp.arm(1, "falcon1", "Evade", true, 1.0);
   EXPECT_DOUBLE_EQ(bp.restoreTimeScale(), 1.0);

   const auto hit{bp.tick({{1, "EVADE"}}, fakeMatches, 1.0)};
   EXPECT_EQ(hit.outcome, BreakpointOutcome::Hit);
   EXPECT_TRUE(hit.shouldRestoreScale);
}

TEST(BreakpointController, NuncaExpiraSozinho)
{
   // Pedido explicito: nao ha mais desarme automatico por tempo -- o
   // breakpoint so sai do ar por HIT ou por cancelamento manual, mesmo
   // apos uma janela de simulacao bem maior que o antigo timeout de 300s.
   BreakpointController bp;
   bp.arm(7, "bandit1", "RTB", false, 1.0);

   const auto stillWaiting{bp.tick({{7, "PATROL"}}, fakeMatches, 1000.0)};
   EXPECT_EQ(stillWaiting.outcome, BreakpointOutcome::None);
   EXPECT_TRUE(bp.isArmed());
   EXPECT_EQ(bp.status(true, true, "RTB").branch, BreakpointStatusBranch::Armed);
}

TEST(BreakpointController, CancelamentoManual)
{
   BreakpointController bp;
   bp.arm(1, "falcon1", "Patrol", true, 2.0);
   EXPECT_TRUE(bp.cancel());   // fastMode -> pede restauracao de escala
   EXPECT_FALSE(bp.isArmed());

   BreakpointController bp2;
   bp2.arm(1, "falcon1", "Patrol", false, 2.0);
   EXPECT_FALSE(bp2.cancel());   // fora de fastMode -> nao pede
}

TEST(BreakpointController, StatusSemArmar)
{
   BreakpointController bp;
   EXPECT_EQ(bp.status(false, false, "").branch, BreakpointStatusBranch::NoTreeSelection);
   EXPECT_EQ(bp.status(true, false, "").branch, BreakpointStatusBranch::NonLeafSelected);
   EXPECT_EQ(bp.status(true, true, "Patrol").branch, BreakpointStatusBranch::LeafSelected);
}

TEST(BreakpointController, RearmarLimpaHitAnterior)
{
   BreakpointController bp;
   bp.arm(1, "falcon1", "Support", false, 1.0);
   bp.tick({{1, "SUPPORT"}}, fakeMatches, 1.0);
   EXPECT_EQ(bp.status(true, true, "Support").branch, BreakpointStatusBranch::Hit);

   bp.arm(2, "falcon2", "Evade", false, 1.0);
   EXPECT_EQ(bp.status(true, true, "Evade").branch, BreakpointStatusBranch::Armed);
}

// ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): re-armar SEM cancelar
// o breakpoint anterior sobrescrevia restoreTimeScale_ com a escala JA
// elevada do primeiro arm() (ex.: 64x), em vez de preservar a escala
// ORIGINAL (a que valia antes do primeiro arm()). Sintoma medido antes da
// correcao: apertar 'G' (breakpoint em modo rapido) duas vezes seguidas,
// sem cancelar entre as duas, travava a simulacao em 64x para sempre --
// mesmo apos o hit, porque o 'hit' subsequente restaurava para o valor
// (ja errado) capturado no segundo arm().
TEST(BreakpointController, RearmarSemCancelarPreservaEscalaOriginal)
{
   BreakpointController bp;

   // Primeiro arm: escala nominal 1.0, entra em modo rapido -- currentTimeScale
   // passado e' JA a escala elevada (64.0), exatamente como DashboardLoop.cpp
   // faz (crava setTimeScale(64) e SO DEPOIS chama arm() com o valor NOMINAL
   // de antes -- aqui simulado direto com o valor nominal correto, 1.0).
   bp.arm(1, "falcon1", "Evade", true, 1.0);
   EXPECT_DOUBLE_EQ(bp.restoreTimeScale(), 1.0);

   // Segundo arm, SEM cancelar antes -- o caso do bug: currentTimeScale
   // aqui e' 64.0 (a escala JA elevada pelo primeiro arm), reproduzindo
   // fielmente o que aconteceria se doArmBreakpoint() fosse chamado de novo
   // com clockStation->getTimeScale() (que ja estaria em 64x).
   bp.arm(1, "falcon1", "Support", true, 64.0);

   // O ponto de restauracao tem que continuar 1.0 -- NAO 64.0.
   EXPECT_DOUBLE_EQ(bp.restoreTimeScale(), 1.0);

   // E o hit subsequente tem que pedir a restauracao do valor CERTO.
   const auto hit{bp.tick({{1, "SUPPORT"}}, fakeMatches, 5.0)};
   EXPECT_EQ(hit.outcome, BreakpointOutcome::Hit);
   EXPECT_TRUE(hit.shouldRestoreScale);
   EXPECT_DOUBLE_EQ(bp.restoreTimeScale(), 1.0);
}

// Contraste: CANCELAR entre os dois arms (o caminho ja correto) continua
// recapturando a escala corretamente -- o fix nao deve quebrar esse caso.
TEST(BreakpointController, RearmarAposCancelarRecapturaEscalaNova)
{
   BreakpointController bp;
   bp.arm(1, "falcon1", "Evade", true, 1.0);
   bp.cancel();

   // Agora desarmado -- um novo arm() com uma escala nominal DIFERENTE
   // (2.0, ex.: usuario acelerou manualmente antes de armar de novo) tem
   // que capturar esse valor novo, nao o antigo (1.0).
   bp.arm(1, "falcon1", "Support", true, 2.0);
   EXPECT_DOUBLE_EQ(bp.restoreTimeScale(), 2.0);
}
