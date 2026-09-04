#include "app/SpeedLadder.hpp"

#include <gtest/gtest.h>

// A escada de velocidade e a decisao de rotulo/cor do cabecalho --
// extraidas de app/DashboardLoop.cpp para app/SpeedLadder.hpp.

using namespace app;

TEST(SpeedLadder, FormatScale)
{
   EXPECT_EQ(formatSpeedScale(1.0), "1x");
   EXPECT_EQ(formatSpeedScale(4.0), "4x");
   EXPECT_EQ(formatSpeedScale(0.25), "0.25x");
   EXPECT_EQ(formatSpeedScale(0.1), "0.10x");
}

TEST(SpeedLadder, ComecaNoTempoReal)
{
   SpeedLadder ladder;
   EXPECT_EQ(ladder.index(), ladder.realTimeIndex());
   EXPECT_DOUBLE_EQ(ladder.scale(), 1.0);
}

TEST(SpeedLadder, AcelerarEFrearNosLimites)
{
   SpeedLadder ladder;
   EXPECT_EQ(ladder.size(), 10);

   int steps{};
   while (ladder.accelerate()) steps++;
   EXPECT_EQ(steps, 6);
   EXPECT_DOUBLE_EQ(ladder.scale(), 64.0);
   EXPECT_FALSE(ladder.accelerate());

   steps = 0;
   while (ladder.decelerate()) steps++;
   EXPECT_EQ(steps, 9);
   EXPECT_DOUBLE_EQ(ladder.scale(), 0.1);
   EXPECT_FALSE(ladder.decelerate());
}

TEST(SpeedLadder, ToRealTimeVoltaDireto)
{
   SpeedLadder ladder;
   ladder.accelerate();
   ladder.accelerate();
   EXPECT_DOUBLE_EQ(ladder.scale(), 4.0);
   ladder.toRealTime();
   EXPECT_DOUBLE_EQ(ladder.scale(), 1.0);
}

TEST(SpeedLadder, SeedFromScaleAncoraNoMaisProximo)
{
   SpeedLadder ladder;
   ladder.seedFromScale(0.3);
   EXPECT_DOUBLE_EQ(ladder.scale(), 0.25);

   ladder.seedFromScale(100.0);
   EXPECT_DOUBLE_EQ(ladder.scale(), 64.0);

   ladder.seedFromScale(-5.0);
   EXPECT_DOUBLE_EQ(ladder.scale(), 0.1);
}

TEST(SpeedLadder, MaxScaleEODegrauMaisAlto)
{
   SpeedLadder ladder;
   EXPECT_DOUBLE_EQ(ladder.maxScale(), 64.0);
}

TEST(SpeedLadder, SpeedDisplayBranches)
{
   auto normal{speedDisplay(false, false, 1.0, 1.0)};
   EXPECT_EQ(normal.label, "1x");
   EXPECT_EQ(normal.tone, SpeedTone::Green);

   EXPECT_EQ(speedDisplay(false, false, 4.0, 3.9).tone, SpeedTone::Yellow);
   EXPECT_EQ(speedDisplay(false, false, 0.5, 0.5).tone, SpeedTone::Cyan);

   const auto paused{speedDisplay(false, true, 2.0, 0.0)};
   EXPECT_EQ(paused.label, "PAUSADO (2x)");
   EXPECT_EQ(paused.tone, SpeedTone::Red);

   const auto fastBp{speedDisplay(true, false, 1.0, 47.3)};
   EXPECT_EQ(fastBp.label, "MAX (~47x real)");
   EXPECT_EQ(fastBp.tone, SpeedTone::Magenta);

   // fastBreakpointRun tem prioridade sobre paused.
   EXPECT_EQ(speedDisplay(true, true, 1.0, 10.0).tone, SpeedTone::Magenta);
}

TEST(SpeedLadder, SpeedDisplayMostraFactualAoLadoDoComandado)
{
   // Fora de 1x, o rotulo traz os DOIS numeros -- o comandado (da escada)
   // e o FACTUAL medido -- porque os dois podem divergir (pacing sob carga
   // de CPU) e o comandado sozinho escondia isso.
   const auto accel{speedDisplay(false, false, 4.0, 3.7)};
   EXPECT_EQ(accel.label, "4x (~3.7x real)");

   const auto decel{speedDisplay(false, false, 0.25, 0.24)};
   EXPECT_EQ(decel.label, "0.25x (~0.2x real)");

   // Em 1x os dois coincidem por definicao -- omitir o factual evita ruido.
   EXPECT_EQ(speedDisplay(false, false, 1.0, 0.97).label, "1x");
}
