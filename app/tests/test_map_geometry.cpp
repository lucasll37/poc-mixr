#include "app/MapGeometry.hpp"

#include <gtest/gtest.h>

#include <cmath>

// A geometria pura da aba Mapa (app/MapGeometry.hpp/.cpp, promovida de um
// namespace anonimo dentro de MapPanel.cpp) -- ver o "porque" no cabecalho
// daquele header: o proprio historico do projeto documenta ter verificado
// esta geometria varias vezes com um "main.cpp descartavel", nunca
// commitado como teste permanente.

using namespace app;
using namespace app::mapgeometry;

namespace {

// project() arredonda pro pixel mais proximo (std::lround) -- a inversa
// nao recupera o ponto EXATO, so dentro de meio pixel de canvas (em
// metros: metersPerCell/2 por eixo rotacionado; a rotacao inversa
// preserva a norma do erro, entao o limite combinado e
// metersPerCell/sqrt(2) -- uso uma folga de metersPerCell*0.75).
double roundTripTolerance(const double metersPerCell) { return metersPerCell * 0.75; }

} // namespace

TEST(MapGeometry, RoundTripTopDownSemRotacao)
{
   MapViewState view;
   view.panNorthM = 1000.0;
   view.panEastM = 2000.0;
   view.metersPerCell = 50.0;
   view.viewYawDeg = 0.0;
   view.perspective = Perspective::TopDown;

   const double northM{1350.0}, eastM{1820.0};
   const auto p{project(northM, eastM, 0.0, view)};

   double outN{}, outE{};
   worldFromCanvasTopDown(p.px, p.py, view, outN, outE);

   const double tol{roundTripTolerance(view.metersPerCell)};
   EXPECT_NEAR(outN, northM, tol);
   EXPECT_NEAR(outE, eastM, tol);
}

TEST(MapGeometry, RoundTripTopDownComRotacaoEPan)
{
   // Bateria de views (pan/escala/rotacao) x pontos -- prova algebrica de
   // que project()/worldFromCanvasTopDown() sao inversas, nao so "parece
   // certo" numa inspecao visual.
   const double yaws[]{0.0, 30.0, 45.0, 90.0, 135.0, 180.0, 227.0, 315.0};
   const double scales[]{5.0, 50.0, 500.0};
   const double points[][2]{
      {0.0, 0.0}, {500.0, -300.0}, {-1200.0, 4000.0}, {10000.0, 10000.0}, {-1.0, -1.0},
   };

   for (const double yaw : yaws) {
      for (const double scale : scales) {
         MapViewState view;
         view.panNorthM = 837.0;
         view.panEastM = -412.0;
         view.metersPerCell = scale;
         view.viewYawDeg = yaw;
         view.perspective = Perspective::TopDown;

         for (const auto& pt : points) {
            const double northM{view.panNorthM + pt[0]}, eastM{view.panEastM + pt[1]};
            const auto p{project(northM, eastM, 0.0, view)};
            double outN{}, outE{};
            worldFromCanvasTopDown(p.px, p.py, view, outN, outE);
            const double tol{roundTripTolerance(scale)};
            EXPECT_NEAR(outN, northM, tol) << "yaw=" << yaw << " scale=" << scale;
            EXPECT_NEAR(outE, eastM, tol) << "yaw=" << yaw << " scale=" << scale;
         }
      }
   }
}

TEST(MapGeometry, RoundTripLateralNaLinhaDeVisada)
{
   // worldFromCanvasLateral() e uma inversa PARCIAL, de proposito -- so
   // recupera pontos com profundidade ZERO na linha de visada (ver o
   // comentario do header). Construo aqui um ponto NESSA linha.
   MapViewState view;
   view.panNorthM = 100.0;
   view.panEastM = 200.0;
   view.metersPerCell = 25.0;
   view.viewYawDeg = 60.0;
   view.perspective = Perspective::Lateral;

   const double yaw{view.viewYawDeg * kDeg2Rad};
   const double rotEOriginal{300.0};
   const double relE{rotEOriginal * std::cos(yaw)};
   const double relN{-rotEOriginal * std::sin(yaw)};
   const double northM{view.panNorthM + relN}, eastM{view.panEastM + relE};

   const auto p{project(northM, eastM, 5000.0, view)};
   double outN{}, outE{};
   worldFromCanvasLateral(p.px, view, outN, outE);

   const double tol{roundTripTolerance(view.metersPerCell)};
   EXPECT_NEAR(outN, northM, tol);
   EXPECT_NEAR(outE, eastM, tol);
}

TEST(MapGeometry, LateralDescartaProfundidadePorDesign)
{
   // O oposto do teste acima: um ponto FORA da linha de visada
   // (profundidade != 0) NAO round-tripa -- comportamento pretendido, nao
   // defeito.
   MapViewState view;
   view.metersPerCell = 25.0;
   view.viewYawDeg = 60.0;
   view.perspective = Perspective::Lateral;

   const double northM{450.0}, eastM{-300.0};   // fora da linha de visada
   const auto p{project(northM, eastM, 5000.0, view)};
   double outN{}, outE{};
   worldFromCanvasLateral(p.px, view, outN, outE);

   const double tol{roundTripTolerance(view.metersPerCell)};
   const bool discardedDepth{std::fabs(outN - northM) > tol || std::fabs(outE - eastM) > tol};
   EXPECT_TRUE(discardedDepth);
}

TEST(MapGeometry, OnCanvasNoCentroDoPan)
{
   MapViewState view;
   view.metersPerCell = 10.0;
   EXPECT_TRUE(project(0.0, 0.0, 0.0, view).onCanvas);
}

TEST(MapGeometry, OnCanvasFalsoLongeDoPan)
{
   MapViewState view;
   view.metersPerCell = 10.0;
   EXPECT_FALSE(project(1'000'000.0, 1'000'000.0, 0.0, view).onCanvas);
}

TEST(MapGeometry, ContourIntervalEscolheODegrauCerto)
{
   EXPECT_DOUBLE_EQ(contourIntervalFor(0.0, 30.0), 10.0);
   EXPECT_DOUBLE_EQ(contourIntervalFor(0.0, 4.0), 5.0);
   EXPECT_DOUBLE_EQ(contourIntervalFor(0.0, 100.0), 25.0);
   EXPECT_DOUBLE_EQ(contourIntervalFor(0.0, 100000.0), 5000.0);
   EXPECT_DOUBLE_EQ(contourIntervalFor(50.0, 40.0), 5.0);   // min > max: range clampado em 1.0
}
