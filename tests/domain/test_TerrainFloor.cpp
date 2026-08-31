// O piso anti-CFIT. Duas camadas, e a regra que importa e que a camada
// absoluta NUNCA sai do caminho -- ver o comentario longo em
// domain/TerrainFloor.hpp sobre o Player::updateElevation() do framework
// marcar elevacao valida com valor 0.0 fora da celula do tile.

#include "domain/TerrainFloor.hpp"

#include <gtest/gtest.h>

namespace {

constexpr double TOL{1e-9};

domain::GroundReference solo(const double elevM)
{
   domain::GroundReference g;
   g.valid = true;
   g.elevationM = elevM;
   return g;
}

TEST(TerrainFloor, SemBancoDeElevacaoCaiNoPisoAbsoluto)
{
   const domain::GroundReference semDado{};   // valid == false
   EXPECT_NEAR(domain::terrainFloorM(semDado, 800.0, 200.0), 200.0, TOL);
}

// terrainClearance: ( Meters 0 ) e o controle negativo do cenario: desliga a
// camada de terreno SEM desligar a rede absoluta.
TEST(TerrainFloor, FolgaZeroDesligaSoACamadaDeTerreno)
{
   EXPECT_NEAR(domain::terrainFloorM(solo(1500.0), 0.0, 200.0), 200.0, TOL);
   EXPECT_NEAR(domain::terrainFloorM(solo(1500.0), -10.0, 200.0), 200.0, TOL);
}

TEST(TerrainFloor, ComTerrenoOPisoEhElevacaoMaisFolga)
{
   EXPECT_NEAR(domain::terrainFloorM(solo(1000.0), 800.0, 200.0), 1800.0, TOL);
}

// Sobre o mar (elevacao 0) a soma terreno+folga fica ABAIXO do piso absoluto:
// quem tem de vencer e o absoluto.
TEST(TerrainFloor, PisoAbsolutoVenceQuandoOTerrenoEhMaisBaixo)
{
   EXPECT_NEAR(domain::terrainFloorM(solo(0.0), 100.0, 200.0), 200.0, TOL);
}

TEST(TerrainFloor, ClampNuncaAbaixaUmComandoQueJaEstaAlto)
{
   EXPECT_NEAR(domain::clampToTerrain(5000.0, solo(1000.0), 800.0, 200.0), 5000.0, TOL);
   EXPECT_NEAR(domain::clampToTerrain(5000.0, domain::GroundReference{}, 800.0, 200.0), 5000.0, TOL);
}

TEST(TerrainFloor, ClampLevantaUmComandoAbaixoDoPiso)
{
   EXPECT_NEAR(domain::clampToTerrain(500.0, solo(1000.0), 800.0, 200.0), 1800.0, TOL);
   EXPECT_NEAR(domain::clampToTerrain(-100.0, domain::GroundReference{}, 800.0, 200.0), 200.0, TOL);
}

// Invariante varrido: para qualquer combinacao de elevacao, folga e comando,
// a saida do clamp nunca fica abaixo do piso que a propria funcao declara.
TEST(TerrainFloor, InvarianteClampNuncaViolaOPiso)
{
   for (double elev = 0.0; elev <= 3000.0; elev += 250.0) {
      for (double folga = 0.0; folga <= 1000.0; folga += 200.0) {
         for (double cmd = -500.0; cmd <= 5000.0; cmd += 500.0) {
            for (const bool valido : {false, true}) {
               domain::GroundReference g;
               g.valid = valido;
               g.elevationM = elev;

               const double piso{domain::terrainFloorM(g, folga, 200.0)};
               const double saida{domain::clampToTerrain(cmd, g, folga, 200.0)};

               EXPECT_GE(saida, piso) << "elev=" << elev << " folga=" << folga
                                      << " cmd=" << cmd << " valido=" << valido;
               EXPECT_GE(saida, cmd) << "o clamp abaixou um comando";
            }
         }
      }
   }
}

} // namespace
