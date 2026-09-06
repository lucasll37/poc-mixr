#include "app/MapPanel.hpp"
#include "app/FleetPanel.hpp"

#include <gtest/gtest.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <algorithm>
#include <string>
#include <vector>

// O canvas da aba Mapa tinha tamanho FIXO (120x30 celulas) -- num terminal
// maior o desenho ocupava so um pedaco do quadro reservado a ele, e num
// menor saia cortado em silencio (relatado rodando o app numa tela de
// resolucao diferente). Agora ele acompanha a caixa que o layout de fato
// deu ao mapa (app::fitMapCanvasToBox()).
//
// Dois niveis, no mesmo arquivo de proposito: a conversao em si (celula ->
// pixel de braille, e o piso que protege do Box zerado do primeiro quadro)
// e a propriedade que o usuario de fato ve -- depois de um quadro, o
// canvas tem EXATAMENTE o tamanho da area que o layout reservou, em varios
// tamanhos de terminal. O segundo e a versao permanente da verificacao que
// o historico deste projeto vinha fazendo com "main.cpp descartavel" (ver
// CLAUDE.md, secao "./app").

using namespace app;
using namespace ftxui;

namespace {

Box makeBox(const int x, const int y, const int cellsW, const int cellsH)
{
   Box b;
   b.x_min = x;
   b.y_min = y;
   b.x_max = x + cellsW - 1;   // ftxui::Box e INCLUSIVO nos dois extremos
   b.y_max = y + cellsH - 1;
   return b;
}

std::vector<EntityState> makeEntities()
{
   std::vector<EntityState> v;
   for (int i = 0; i < 4; i++) {
      EntityState e;
      e.id = i + 1;
      e.name = "falcon" + std::to_string(i + 1);
      e.northM = 500.0 * i;
      e.eastM = -300.0 * i;
      e.altitudeM = 1500.0 + 50.0 * i;
      e.headingDeg = 45.0 * i;
      v.push_back(e);
   }
   return v;
}

// A MESMA composicao da aba Mapa em app/DashboardLoop.cpp -- mapa com
// 'flex' ao lado de um card de largura fixa, dentro de um vbox com barra e
// rodape. E o layout que decide a caixa do canvas; por isso o teste passa
// por ele em vez de chamar renderMap() solto.
void renderFrame(const std::vector<EntityState>& entities, MapViewState& view, Box& mapCanvasBox,
                 const int termW, const int termH)
{
   fitMapCanvasToBox(view, mapCanvasBox);

   const int detailW{std::clamp(termW - (kMapCanvasWidthCells + 6),
                                kDetailPanelMinWidth, kDetailPanelMaxWidth)};
   Element detail{text("card") | border
                  | size(WIDTH, EQUAL, detailW) | size(HEIGHT, EQUAL, kDetailPanelHeight)};
   Element doc{vbox({
      text("toolbar"),
      text("abas"),
      hbox({renderMap(entities, view, 1, mapCanvasBox, {}) | flex, separator(), detail}) | flex,
      text("rodape"),
   })};

   auto screen{Screen::Create(Dimension::Fixed(termW), Dimension::Fixed(termH))};
   Render(screen, doc);
}

} // namespace

TEST(MapCanvasFit, ConverteCelulaEmPixelDeBraille)
{
   MapViewState view;
   fitMapCanvasToBox(view, makeBox(3, 7, 80, 25));
   EXPECT_EQ(view.canvasWidthPx, 160);    // 2 pixels por celula na horizontal
   EXPECT_EQ(view.canvasHeightPx, 100);   // 4 na vertical
}

TEST(MapCanvasFit, CaixaDegeneradaEhNoOp)
{
   // Box zerado -- o que existe no PRIMEIRO quadro, antes de qualquer
   // reflect. Nao pode zerar o canvas: mantem o tamanho anterior.
   MapViewState view;
   const int w{view.canvasWidthPx};
   const int h{view.canvasHeightPx};
   fitMapCanvasToBox(view, Box{});
   EXPECT_EQ(view.canvasWidthPx, w);
   EXPECT_EQ(view.canvasHeightPx, h);

   // Abaixo do piso, idem -- nao encolhe pra nada.
   fitMapCanvasToBox(view, makeBox(0, 0, kMapCanvasMinCellsW - 1, kMapCanvasMinCellsH - 1));
   EXPECT_EQ(view.canvasWidthPx, w);
   EXPECT_EQ(view.canvasHeightPx, h);
}

TEST(MapCanvasFit, CanvasPreencheAAreaQueOLayoutReservou)
{
   const auto entities{makeEntities()};
   const int sizes[][2]{{80, 24}, {100, 30}, {160, 45}, {210, 60}, {300, 80}};

   for (const auto& sz : sizes) {
      MapViewState view;
      Box mapCanvasBox{};

      // Dois quadros: o primeiro so preenche 'mapCanvasBox' (a caixa de um
      // elemento so existe DEPOIS do layout), o segundo ja desenha
      // ajustado. Um quadro de atraso ao redimensionar, e so.
      renderFrame(entities, view, mapCanvasBox, sz[0], sz[1]);
      renderFrame(entities, view, mapCanvasBox, sz[0], sz[1]);

      const int cellsW{mapCanvasBox.x_max - mapCanvasBox.x_min + 1};
      const int cellsH{mapCanvasBox.y_max - mapCanvasBox.y_min + 1};
      EXPECT_EQ(view.canvasWidthPx / 2, cellsW) << "terminal " << sz[0] << "x" << sz[1];
      EXPECT_EQ(view.canvasHeightPx / 4, cellsH) << "terminal " << sz[0] << "x" << sz[1];

      // E PONTO FIXO: um terceiro quadro no mesmo tamanho nao muda mais
      // nada (dimensionar pela caixa nao realimenta o layout, porque o
      // elemento do canvas leva 'flex' -- quem manda na caixa e o layout,
      // nao a exigencia do canvas).
      const int stableW{view.canvasWidthPx};
      const int stableH{view.canvasHeightPx};
      renderFrame(entities, view, mapCanvasBox, sz[0], sz[1]);
      EXPECT_EQ(view.canvasWidthPx, stableW) << "terminal " << sz[0] << "x" << sz[1];
      EXPECT_EQ(view.canvasHeightPx, stableH) << "terminal " << sz[0] << "x" << sz[1];
   }
}
