#include "app/ComponentTreePanel.hpp"
#include "app/FleetPanel.hpp"   // modeLabel()/modeColor() -- reusadas no card de detalhe

#include "xboard/Board.hpp"

#include "mixr/simulation/AbstractPlayer.hpp"

#include "ftxui/dom/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;

// Espacamento entre coluna/linha, em PIXEL DE CANVAS (braille: 2px/celula
// na horizontal, 4px/celula na vertical -- mesma unidade de
// app::MapViewState::canvasWidthPx/HeightPx). Generoso o bastante pra caber
// um rotulo tipo "FlightAgentTC" ao lado do no sem colidir com a PROXIMA
// coluna -- pedido explicito de nao precisar ser sofisticado, so legivel; o
// usuario zoom in/out (mesma tecla/roda do Mapa) se um cenario tiver arvore
// mais hunda/larga.
const int kColSpacingPx{64};
const int kRowSpacingPx{12};

struct ProjectedNode { int px{}; int py{}; bool onCanvas{}; };

ProjectedNode projectTreeNode(const double depth, const double row, const ComponentTreeViewState& view)
{
   const int cx{view.canvasWidthPx / 2};
   const int cy{view.canvasHeightPx / 2};
   const double spacingX{kColSpacingPx * view.zoom};
   const double spacingY{kRowSpacingPx * view.zoom};

   const int px{cx + static_cast<int>(std::lround((depth - view.panDepth) * spacingX))};
   const int py{cy + static_cast<int>(std::lround((row - view.panRow) * spacingY))};
   const bool onCanvas{px >= 0 && px < view.canvasWidthPx && py >= 0 && py < view.canvasHeightPx};
   return {px, py, onCanvas};
}

// Rotulo curto pro proprio ponto (o nome do slot quando existe -- e o que
// aparece no .epp -- senao a classe). O card de detalhe (mais abaixo) mostra
// os dois.
std::string shortNodeLabel(const ComponentTreeLayoutNode& node)
{
   return node.slotName.empty() ? node.className : node.slotName;
}

}   // namespace

Color phaseColor(const EstimatedPhase phase)
{
   switch (phase) {
      case EstimatedPhase::Structural:         return Color::White;
      case EstimatedPhase::DynamicsPhase0:      return Color::Yellow;
      case EstimatedPhase::SensorPhase1And2:    return Color::Cyan;
      case EstimatedPhase::DecisionPhase3:      return Color::Green;
      case EstimatedPhase::DecisionBackground:  return Color::GreenLight;
      case EstimatedPhase::Background:          return Color::Magenta;
      case EstimatedPhase::Unknown:
      default:                                 return Color::GrayDark;
   }
}

namespace {

// DFS em pos-ordem: cada FOLHA recebe a proxima linha inteira disponivel
// ('nextRow', incrementada); cada no INTERNO recebe a MEDIA da linha dos
// filhos -- e o que centraliza um pai verticalmente em relacao a prole dele,
// o mesmo principio de qualquer desenho simples de arvore. 'depth' e so a
// profundidade da recursao (a raiz de app::discoverComponentTree() e
// depth=0).
double layoutSubtree(const ComponentTreeNode& node, const int depth, double& nextRow,
                     std::vector<ComponentTreeLayoutNode>& out, const int parentIndex)
{
   const int myIndex{static_cast<int>(out.size())};
   ComponentTreeLayoutNode laid;
   laid.slotName = node.slotName;
   laid.className = node.className;
   laid.isPlayer = node.isPlayer;
   laid.playerId = node.playerId;
   laid.phase = node.phase;
   laid.depth = depth;
   laid.parentIndex = parentIndex;
   out.push_back(std::move(laid));

   if (node.children.empty()) {
      const double row{nextRow};
      nextRow += 1.0;
      out[static_cast<std::size_t>(myIndex)].row = row;
      return row;
   }

   double sum{};
   for (const auto& child : node.children) {
      sum += layoutSubtree(child, depth + 1, nextRow, out, myIndex);
   }
   const double row{sum / static_cast<double>(node.children.size())};
   out[static_cast<std::size_t>(myIndex)].row = row;
   return row;
}

}   // namespace

ComponentTreeLayout layoutComponentTree(const ComponentTreeNode& root)
{
   ComponentTreeLayout layout;
   double nextRow{0.0};
   layoutSubtree(root, 0, nextRow, layout.nodes, -1);
   return layout;
}

void fitComponentTreeCanvasToBox(ComponentTreeViewState& view, const Box& box)
{
   // Box e INCLUSIVO nos dois extremos -- mesma ressalva de
   // app::fitMapCanvasToBox() (CLAUDE.md, "decima sexta passada").
   const int cellsW{box.x_max - box.x_min + 1};
   const int cellsH{box.y_max - box.y_min + 1};
   if (cellsW < kTreeCanvasMinCellsW || cellsH < kTreeCanvasMinCellsH) return;

   view.canvasWidthPx = cellsW * 2;
   view.canvasHeightPx = cellsH * 4;
}

void zoomComponentTree(ComponentTreeViewState& view, const bool zoomIn)
{
   view.zoom *= zoomIn ? kTreeZoomStep : (1.0 / kTreeZoomStep);
   view.zoom = std::clamp(view.zoom, kTreeMinZoom, kTreeMaxZoom);
}

void panComponentTree(ComponentTreeViewState& view, const double screenRightPx, const double screenDownPx)
{
   // Sinal invertido de proposito -- "o CONTEUDO segue o cursor" (mesmo
   // efeito ja validado em app::panMap()/DashboardLoop.cpp): arrastar pra
   // direita/baixo tem de mover os nos pra direita/baixo na tela, e como a
   // projecao usa '(depth - panDepth)'/'(row - panRow)', isso exige
   // DIMINUIR o pan, nao aumentar.
   view.panDepth -= screenRightPx / (kColSpacingPx * view.zoom);
   view.panRow -= screenDownPx / (kRowSpacingPx * view.zoom);
}

void centerComponentTreeOn(ComponentTreeViewState& view, const ComponentTreeLayoutNode& node)
{
   view.panDepth = node.depth;
   view.panRow = node.row;
}

void fitComponentTreeToContent(ComponentTreeViewState& view, const ComponentTreeLayout& layout)
{
   if (layout.nodes.empty()) return;

   double minDepth{static_cast<double>(layout.nodes.front().depth)};
   double maxDepth{static_cast<double>(layout.nodes.front().depth)};
   double minRow{layout.nodes.front().row};
   double maxRow{layout.nodes.front().row};
   for (const auto& node : layout.nodes) {
      minDepth = std::min(minDepth, static_cast<double>(node.depth));
      maxDepth = std::max(maxDepth, static_cast<double>(node.depth));
      minRow = std::min(minRow, node.row);
      maxRow = std::max(maxRow, node.row);
   }

   // 90% do canvas -- sobra uma margem pra rotulo/borda, mesma folga que
   // outras vistas responsivas deste app costumam deixar.
   const double usableW{view.canvasWidthPx * 0.9};
   const double usableH{view.canvasHeightPx * 0.9};
   const double spanDepth{std::max(1.0, maxDepth - minDepth)};
   const double spanRow{std::max(1.0, maxRow - minRow)};

   const double zoomForWidth{usableW / (spanDepth * kColSpacingPx)};
   const double zoomForHeight{usableH / (spanRow * kRowSpacingPx)};
   view.zoom = std::clamp(std::min(zoomForWidth, zoomForHeight), kTreeMinZoom, kTreeMaxZoom);

   view.panDepth = (minDepth + maxDepth) / 2.0;
   view.panRow = (minRow + maxRow) / 2.0;
}

Element renderComponentTree(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                            Box& outCanvasBox, const EstimatedPhase activeFlowPhase)
{
   const int canvasW{view.canvasWidthPx};
   const int canvasH{view.canvasHeightPx};
   auto c = Canvas(canvasW, canvasH);

   // Linhas pai->filho primeiro (ficam por BAIXO dos pontos/rotulos).
   for (const auto& node : layout.nodes) {
      if (node.parentIndex < 0) continue;
      const auto& parent{layout.nodes[static_cast<std::size_t>(node.parentIndex)]};
      const ProjectedNode a{projectTreeNode(parent.depth, parent.row, view)};
      const ProjectedNode b{projectTreeNode(node.depth, node.row, view)};
      if (!a.onCanvas && !b.onCanvas) continue;
      c.DrawPointLine(a.px, a.py, b.px, b.py, [](Cell& cell) {
         cell.foreground_color = Color::GrayDark;
         cell.dim = true;
      });
   }

   // Pontos + rotulos numa segunda passada -- mesma razao de
   // app::renderMap(): um rotulo desenhado cedo demais ficaria por baixo do
   // ponto de um no vizinho desenhado depois.
   for (std::size_t i = 0; i < layout.nodes.size(); i++) {
      const auto& node{layout.nodes[i]};
      const ProjectedNode p{projectTreeNode(node.depth, node.row, view)};
      if (!p.onCanvas) continue;

      const Color col{phaseColor(node.phase)};
      // "Pulso" do ciclo de fluxo (SEGUNDA METADE, ver
      // app/ComponentFlowState.hpp) -- anel AMARELO, raio 3, deliberadamente
      // diferente do anel branco (raio 4) de selecao logo abaixo, pra "esta
      // no ciclo agora" e "esta selecionado" nunca se confundirem mesmo
      // quando calham no MESMO no. So desenha quando a fase e conhecida --
      // 'Unknown' nunca "pulsa" (nao ha no de verdade nela, ver o
      // comentario de kComponentFlowCycle).
      if (node.phase == activeFlowPhase && activeFlowPhase != EstimatedPhase::Unknown) {
         c.DrawPointCircle(p.px, p.py, 3, Color::YellowLight);
      }
      if (static_cast<int>(i) == view.selectedIndex) c.DrawPointCircle(p.px, p.py, 4, Color::White);
      c.DrawPointCircleFilled(p.px, p.py, 2, col);
      c.DrawText(p.px + 4, p.py - 2, shortNodeLabel(node), col);
   }

   c.DrawText(2, 2, "Componentes  [zoom " + std::to_string(static_cast<int>(std::lround(view.zoom * 100)))
      + "%]  (estimado = fase INFERIDA, ver card de detalhe)", Color::GrayDark);

   return canvas(std::move(c)) | reflect(outCanvasBox) | flex | border;
}

int hitTestComponentTreeNode(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                             const int clickCellX, const int clickCellY)
{
   int bestIndex{-1};
   int bestDist{-1};
   for (std::size_t i = 0; i < layout.nodes.size(); i++) {
      const auto& node{layout.nodes[i]};
      const ProjectedNode p{projectTreeNode(node.depth, node.row, view)};
      if (!p.onCanvas) continue;
      const int cellX{p.px / 2};
      const int cellY{p.py / 4};
      const int dist{std::abs(cellX - clickCellX) + std::abs(cellY - clickCellY)};
      if (dist > 1) continue;
      if (bestDist < 0 || dist < bestDist) { bestDist = dist; bestIndex = static_cast<int>(i); }
   }
   return bestIndex;
}

Element renderComponentDetail(const ComponentTreeLayoutNode& node)
{
   const Color badge{phaseColor(node.phase)};

   Elements lines;
   lines.push_back(hbox({
      text(" " + shortNodeLabel(node) + " ") | bgcolor(badge) | color(Color::Black) | bold,
      filler(),
   }));
   lines.push_back(separator());

   lines.push_back(hbox({text("slot  ") | dim, text(node.slotName.empty() ? "(raiz)" : node.slotName)}));
   lines.push_back(hbox({text("classe ") | dim, text(node.className)}));

   // A fase e SEMPRE anotada como estimativa quando nao e 'Structural' --
   // e o UNICO dado desta aba que nao e medido (ver o comentario grande de
   // app::EstimatedPhase). O resto do dashboard sempre mostrou dado real;
   // esta linha nao pode deixar essa diferenca passar despercebida.
   lines.push_back(text(""));
   std::ostringstream phaseLine;
   phaseLine << "fase: " << phaseLabel(node.phase);
   lines.push_back(text(phaseLine.str()) | color(badge) | bold);
   if (isHeuristicPhase(node.phase)) {
      lines.push_back(text("(ESTIMADO por heuristica de nome/classe -- NAO medido; ver CLAUDE.md)")
                      | color(Color::YellowLight) | dim);
   } else {
      lines.push_back(text("(estrutural -- inferido pelo TIPO C++ exato, nao por heuristica de nome)")
                      | dim);
   }

   // Dado REAL, quando o no e um Player -- MESMO xboard::Readout que
   // app/DashboardState.cpp/app/FleetPanel.cpp ja usam (nao duplicado: e a
   // mesma chamada, so lida aqui tambem), pra nao divergir de vocabulario
   // entre abas.
   if (node.isPlayer && node.playerId >= 0) {
      lines.push_back(text(""));
      lines.push_back(text("player id " + std::to_string(node.playerId)) | bold);

      const mixr::xboard::Readout board{mixr::xboard::get(node.playerId)};
      lines.push_back(hbox({text("bt=") | dim, text(board.label)
                            | bgcolor(behaviorColor(board.label)) | color(Color::Black)}));
      lines.push_back(text("decisoes " + std::to_string(board.decisions)
                           + "   thread " + (board.threadTag >= 0
                              ? ("T" + std::to_string(board.threadTag)) : std::string{"-"})));
   }

   return vbox(std::move(lines)) | border | color(badge) | size(WIDTH, EQUAL, 56);
}

Element renderComponentFlowLegend()
{
   Elements swatches;
   for (std::size_t i = 0; i < kComponentFlowCycleLen; i++) {
      const EstimatedPhase phase{kComponentFlowCycle[i]};
      swatches.push_back(hbox({
         text("  ") | bgcolor(phaseColor(phase)),
         text(" " + phaseLabel(phase) + "  ") | dim,
      }));
   }
   return hbox(std::move(swatches));
}

Element renderComponentFlowStatus(const ComponentFlowState& flow)
{
   const EstimatedPhase phase{currentFlowPhase(flow)};
   std::ostringstream line;
   line << "ciclo de fluxo: fase atual " << (flow.cycleIndex + 1) << "/" << kComponentFlowCycleLen
        << " -- " << phaseLabel(phase) << "   [" << (flow.playing ? "tocando" : "pausado")
        << " @ " << flow.stepsPerSecond << "x/s]";

   return vbox({
      hbox({text(line.str()) | color(phaseColor(phase)) | bold}),
      text("MODELO CONCEITUAL do ciclo do frame (fase 0 -> 1/2 -> 3 -> decisao de fundo -> fundo -> "
           "volta pra fase 0) -- avanca por um relogio de ANIMACAO PROPRIO, NAO e um tracado ao vivo "
           "de chamadas reais (o MIXR e dependencia binaria; ver CLAUDE.md/app/ComponentFlowState.hpp)")
         | dim,
   });
}

} // namespace app
