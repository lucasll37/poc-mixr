// A logica pura por tras da animacao de fluxo da aba "Componentes" (F6) --
// app/ComponentFlowState.hpp. Sem MIXR, sem FTXUI, sem Station: so o
// avanco do ciclo (ordem certa, da a volta no fim) e o relogio de
// animacao (avanca por CONTAGEM DE REDESENHOS, nao por tempo de parede --
// ver o comentario grande do header sobre por que isto e um MODELO
// CONCEITUAL do ciclo do frame MIXR, nao uma medicao ao vivo).

#include "app/ComponentFlowState.hpp"

#include <gtest/gtest.h>

namespace {

using app::advanceComponentFlowStep;
using app::ComponentFlowState;
using app::currentFlowPhase;
using app::cycleComponentFlowSpeed;
using app::EstimatedPhase;
using app::kComponentFlowCycle;
using app::kComponentFlowCycleLen;
using app::tickComponentFlowAnimation;
using app::toggleComponentFlowPlaying;

TEST(ComponentFlowState, ComecaNaPrimeiraFaseDoCiclo)
{
   const ComponentFlowState flow;
   EXPECT_EQ(currentFlowPhase(flow), kComponentFlowCycle[0]);
}

TEST(ComponentFlowState, AvancaNaOrdemCertaEDaAVolta)
{
   ComponentFlowState flow;
   for (std::size_t i = 0; i < kComponentFlowCycleLen; i++) {
      EXPECT_EQ(currentFlowPhase(flow), kComponentFlowCycle[i]);
      advanceComponentFlowStep(flow);
   }
   // Depois de kComponentFlowCycleLen avancos, voltou pro comeco -- e um
   // CICLO, nao uma sequencia que termina.
   EXPECT_EQ(currentFlowPhase(flow), kComponentFlowCycle[0]);
}

TEST(ComponentFlowState, TodaFaseDoCicloEDistintaDeUnknown)
{
   // 'Unknown' fica DE FORA do ciclo de proposito -- nenhum no de verdade
   // "esta" nela (ver o comentario do header).
   for (std::size_t i = 0; i < kComponentFlowCycleLen; i++) {
      EXPECT_NE(kComponentFlowCycle[i], EstimatedPhase::Unknown);
   }
}

TEST(ComponentFlowState, TogglePlayingAlternaEZeraContador)
{
   ComponentFlowState flow;
   EXPECT_TRUE(flow.playing);
   flow.redrawsSincePlay = 3;
   toggleComponentFlowPlaying(flow);
   EXPECT_FALSE(flow.playing);
   EXPECT_EQ(flow.redrawsSincePlay, 0);
   toggleComponentFlowPlaying(flow);
   EXPECT_TRUE(flow.playing);
}

TEST(ComponentFlowState, CycleSpeedPercorre1249)
{
   ComponentFlowState flow;
   EXPECT_EQ(flow.stepsPerSecond, 1);
   cycleComponentFlowSpeed(flow);
   EXPECT_EQ(flow.stepsPerSecond, 2);
   cycleComponentFlowSpeed(flow);
   EXPECT_EQ(flow.stepsPerSecond, 4);
   cycleComponentFlowSpeed(flow);
   EXPECT_EQ(flow.stepsPerSecond, 1);
}

TEST(ComponentFlowState, TickPausadoNuncaAvanca)
{
   ComponentFlowState flow;
   flow.playing = false;
   for (int i = 0; i < 100; i++) tickComponentFlowAnimation(flow, 10);
   EXPECT_EQ(currentFlowPhase(flow), kComponentFlowCycle[0]);
}

TEST(ComponentFlowState, TickAvancaUmaVezPorSegundoA1PorSegundo)
{
   // redrawsPerSecond=10, stepsPerSecond=1 -> um avanco a cada 10 redesenhos.
   ComponentFlowState flow;
   flow.stepsPerSecond = 1;
   for (int i = 0; i < 9; i++) tickComponentFlowAnimation(flow, 10);
   EXPECT_EQ(currentFlowPhase(flow), kComponentFlowCycle[0]);   // ainda nao avancou
   tickComponentFlowAnimation(flow, 10);                        // o 10o redesenho avanca
   EXPECT_EQ(currentFlowPhase(flow), kComponentFlowCycle[1]);
}

TEST(ComponentFlowState, VelocidadeMaiorAvancaMaisRapido)
{
   // Com stepsPerSecond=4 a 10 redesenhos/s, avanca a cada ~2-3 redesenhos --
   // bem antes dos 10 que 1x levaria.
   ComponentFlowState flow;
   flow.stepsPerSecond = 4;
   int steps{};
   for (int i = 0; i < 10; i++) {
      const std::size_t before{flow.cycleIndex};
      tickComponentFlowAnimation(flow, 10);
      if (flow.cycleIndex != before) steps++;
   }
   EXPECT_GT(steps, 1);
}

TEST(ComponentFlowState, PassoManualFuncionaMesmoTocando)
{
   ComponentFlowState flow;
   flow.playing = true;
   const EstimatedPhase before{currentFlowPhase(flow)};
   advanceComponentFlowStep(flow);
   EXPECT_NE(currentFlowPhase(flow), before);
}

}   // namespace
