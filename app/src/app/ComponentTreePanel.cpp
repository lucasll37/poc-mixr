#include "app/ComponentTreePanel.hpp"
#include "app/FleetPanel.hpp"   // behaviorColor() -- reusada no card de detalhe

#include "xboard/Board.hpp"

#include "ftxui/dom/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;

// Espaçamento entre NÍVEIS, em PIXEL DE CANVAS (braille: 4px por célula na
// vertical). 20px = 5 linhas de terminal por nível: cabe o ponto do nó, a
// linha de rótulo logo abaixo dele e o cotovelo que desce para os filhos,
// sem o rótulo de um nível encostar no ponto do seguinte.
const int kRowSpacingPx{20};

// Folga horizontal ENTRE dois rótulos vizinhos (px de canvas). A largura de
// cada folha é a do próprio rótulo (2px por caractere, ver Canvas::DrawText)
// mais esta folga -- é o que garante que dois nomes nunca se sobreponham em
// zoom 1.0, por mais desiguais que sejam.
const int kColGapPx{6};

// Onde o cotovelo pai->filho vira na horizontal: fração de kRowSpacingPx
// abaixo do pai. 0.7 põe a barra ABAIXO da linha de rótulo do pai (que
// ocupa +4..+8px) e ACIMA do ponto do filho (+20px).
const double kElbowFraction{0.7};

// Abaixo deste zoom os rótulos deixam de ser desenhados (só os pontos e os
// cotovelos ficam) -- exceto o do nó SELECIONADO, que é a referência de
// "onde eu estou". O layout reserva a largura de cada rótulo em zoom 1.0;
// afastando-se dele o texto continua com a largura da FONTE do terminal
// enquanto o espaço reservado encolhe, e a partir de ~50% os nomes vizinhos
// se sobrepõem e viram borrão. Medido com "[o] Expandir tudo" no cenário
// intercept, que enquadra a árvore inteira em ~28%.
const double kLabelMinZoom{0.5};

// Margem (em px de canvas) que ensureComponentNodeVisible() exige em volta
// do nó antes de considerar que ele "está visível" -- generosa o bastante
// pra caber o rótulo dele, que é desenhado ao lado/abaixo do ponto.
const int kVisibilityMarginPx{24};

struct ProjectedNode { int px{}; int py{}; bool onCanvas{}; };

ProjectedNode projectTreeNode(const double x, const double y, const ComponentTreeViewState& view)
{
   const int cx{view.canvasWidthPx / 2};
   const int cy{view.canvasHeightPx / 2};

   const int px{cx + static_cast<int>(std::lround((x - view.panX) * view.zoom))};
   const int py{cy + static_cast<int>(std::lround((y - view.panY) * view.zoom))};
   const bool onCanvas{px >= 0 && px < view.canvasWidthPx && py >= 0 && py < view.canvasHeightPx};
   return {px, py, onCanvas};
}

// Nome curto do nó: o do slot quando existe (é o que aparece no .epp), senão
// a classe. O card de detalhe mostra os dois.
std::string shortNodeLabel(const ComponentTreeLayoutNode& node)
{
   return node.slotName.empty() ? node.className : node.slotName;
}

// O que de fato vai desenhado ao lado do ponto: o nome mais o marcador de
// galho. "[+N]" = retraído, escondendo N filhos; "[-]" = expandido, dá para
// retrair. Folha não ganha marcador nenhum -- não há o que alternar nela.
std::string drawnNodeLabel(const ComponentTreeLayoutNode& node)
{
   const std::string name{shortNodeLabel(node)};
   if (node.childCount <= 0) return name;
   if (node.collapsed) return name + " [+" + std::to_string(node.childCount) + "]";
   return name + " [-]";
}

int labelWidthPx(const std::string& label)
{
   // Canvas::DrawText avança 2px por caractere (uma célula de terminal).
   return static_cast<int>(label.size()) * 2;
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

// DFS: 'y' sai direto da profundidade; 'x' de uma varredura da esquerda para
// a direita em que cada FOLHA (ou nó retraído, que se comporta como folha)
// consome a largura do próprio rótulo, e cada nó INTERNO fica centrado entre
// o primeiro e o último filho -- o desenho clássico de organograma. Centrar
// pelos EXTREMOS (e não pela média de todos) é o que mantém o pai no meio
// visual do bloco de filhos mesmo quando a prole é assimétrica.
double layoutSubtree(const ComponentTreeNode& node, const int depth, double& cursorX,
                     std::vector<ComponentTreeLayoutNode>& out, const int parentIndex,
                     const CollapsedNodes& collapsed)
{
   const int myIndex{static_cast<int>(out.size())};

   ComponentTreeLayoutNode laid;
   laid.nodeKey = node.nodeKey;
   laid.slotName = node.slotName;
   laid.className = node.className;
   laid.isPlayer = node.isPlayer;
   laid.playerId = node.playerId;
   laid.phase = node.phase;
   laid.state = node.state;
   laid.depth = depth;
   laid.parentIndex = parentIndex;
   laid.childCount = static_cast<int>(node.children.size());
   laid.collapsed = !node.children.empty() && collapsed.count(node.nodeKey) > 0;
   laid.y = static_cast<double>(depth) * kRowSpacingPx;
   out.push_back(std::move(laid));

   const bool drawChildren{!node.children.empty() && collapsed.count(node.nodeKey) == 0};
   if (!drawChildren) {
      const double width{static_cast<double>(labelWidthPx(drawnNodeLabel(out[static_cast<std::size_t>(myIndex)])))};
      const double x{cursorX + width / 2.0};
      cursorX += width + kColGapPx;
      out[static_cast<std::size_t>(myIndex)].x = x;
      return x;
   }

   double firstChildX{};
   double lastChildX{};
   for (std::size_t i = 0; i < node.children.size(); i++) {
      const double childX{layoutSubtree(node.children[i], depth + 1, cursorX, out, myIndex, collapsed)};
      if (i == 0) firstChildX = childX;
      lastChildX = childX;
   }
   const double x{(firstChildX + lastChildX) / 2.0};
   out[static_cast<std::size_t>(myIndex)].x = x;
   return x;
}

void collectCollapsible(const ComponentTreeNode& node, const int depth, const int maxDepth,
                        CollapsedNodes& collapsed)
{
   if (!node.children.empty() && depth >= maxDepth) collapsed.insert(node.nodeKey);
   for (const auto& child : node.children) collectCollapsible(child, depth + 1, maxDepth, collapsed);
}

}   // namespace

ComponentTreeLayout layoutComponentTree(const ComponentTreeNode& root, const CollapsedNodes& collapsed)
{
   ComponentTreeLayout layout;
   double cursorX{0.0};
   layoutSubtree(root, 0, cursorX, layout.nodes, -1, collapsed);

   if (layout.nodes.empty()) return layout;

   layout.minX = layout.maxX = layout.nodes.front().x;
   layout.minY = layout.maxY = layout.nodes.front().y;
   for (const auto& node : layout.nodes) {
      // A extensão inclui a METADE do rótulo de cada lado -- senão o
      // enquadramento cortaria o texto dos nós das pontas.
      const double half{labelWidthPx(drawnNodeLabel(node)) / 2.0};
      layout.minX = std::min(layout.minX, node.x - half);
      layout.maxX = std::max(layout.maxX, node.x + half);
      layout.minY = std::min(layout.minY, node.y);
      layout.maxY = std::max(layout.maxY, node.y);
   }
   return layout;
}

int findComponentNodeIndex(const ComponentTreeLayout& layout, const std::string& nodeKey)
{
   if (nodeKey.empty()) return -1;
   for (std::size_t i = 0; i < layout.nodes.size(); i++) {
      if (layout.nodes[i].nodeKey == nodeKey) return static_cast<int>(i);
   }
   return -1;
}

void fitComponentTreeCanvasToBox(ComponentTreeViewState& view, const Box& box)
{
   // Box é INCLUSIVO nos dois extremos -- mesma ressalva de
   // app::fitMapCanvasToBox() (CLAUDE.md, "décima sexta passada").
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
   // Sinal invertido de propósito -- "o CONTEÚDO segue o cursor" (mesmo
   // efeito já validado em app::panMap()): arrastar para a direita/baixo tem
   // de mover os nós para a direita/baixo na tela, e como a projeção usa
   // '(x - panX)', isso exige DIMINUIR o pan, não aumentar.
   view.panX -= screenRightPx / view.zoom;
   view.panY -= screenDownPx / view.zoom;
}

void centerComponentTreeOn(ComponentTreeViewState& view, const ComponentTreeLayoutNode& node)
{
   view.panX = node.x;
   view.panY = node.y;
}

void ensureComponentNodeVisible(ComponentTreeViewState& view, const ComponentTreeLayoutNode& node)
{
   const ProjectedNode p{projectTreeNode(node.x, node.y, view)};
   const bool visible{p.px >= kVisibilityMarginPx
                      && p.px < view.canvasWidthPx - kVisibilityMarginPx
                      && p.py >= kVisibilityMarginPx
                      && p.py < view.canvasHeightPx - kVisibilityMarginPx};
   if (!visible) centerComponentTreeOn(view, node);
}

void fitComponentTreeToContent(ComponentTreeViewState& view, const ComponentTreeLayout& layout)
{
   if (layout.nodes.empty()) return;

   // 90% do canvas -- sobra uma margem pra rótulo/borda, mesma folga que
   // outras vistas responsivas deste app costumam deixar.
   const double usableW{view.canvasWidthPx * 0.9};
   const double usableH{view.canvasHeightPx * 0.9};
   const double spanX{std::max(1.0, layout.maxX - layout.minX)};
   // '+ kRowSpacingPx' porque o rótulo do último nível é desenhado ABAIXO do
   // ponto dele -- sem essa linha extra, a fileira de baixo sai cortada.
   const double spanY{std::max(1.0, layout.maxY - layout.minY + kRowSpacingPx)};

   // Teto de 1.0: o enquadramento so REDUZ, nunca amplia. Uma arvore recem
   // aberta (raiz + 3 filhos) caberia com folga em 180%, e ai o primeiro
   // galho que o usuario expandisse ja nasceria estourando o canvas -- pior
   // que comecar em 100% e sobrar tela.
   view.zoom = std::clamp(std::min({usableW / spanX, usableH / spanY, 1.0}),
                          kTreeMinZoom, kTreeMaxZoom);
   view.panX = (layout.minX + layout.maxX) / 2.0;

   // Na vertical a raiz fica ANCORADA perto do topo, nao no meio: uma arvore
   // se le de cima para baixo, e centrar verticalmente uma arvore rasa
   // deixaria uma faixa vazia acima da raiz do tamanho da propria arvore.
   // Isto e project() (ramo vertical) resolvido para 'panY' com
   // 'py == kTopMarginPx'.
   const double kTopMarginPx{12.0};
   view.panY = layout.minY + ((view.canvasHeightPx / 2.0) - kTopMarginPx) / view.zoom;
}

void toggleComponentNodeCollapsed(CollapsedNodes& collapsed, const ComponentTreeLayoutNode& node)
{
   if (node.childCount <= 0) return;
   if (collapsed.count(node.nodeKey) > 0) collapsed.erase(node.nodeKey);
   else collapsed.insert(node.nodeKey);
}

void collapseDeeperThan(const ComponentTreeNode& root, const int maxDepth, CollapsedNodes& collapsed)
{
   collectCollapsible(root, 0, maxDepth, collapsed);
}

void collapseAllComponentNodes(const ComponentTreeNode& root, CollapsedNodes& collapsed)
{
   collectCollapsible(root, 0, 0, collapsed);
}

bool navigateComponentTree(const ComponentTreeLayout& layout, ComponentTreeViewState& view,
                           const TreeNavigation dir, CollapsedNodes& collapsed)
{
   if (layout.nodes.empty()) return false;

   const int current{findComponentNodeIndex(layout, view.selectedKey)};
   if (current < 0) {
      // Sem seleção: qualquer direção "entra" na árvore pela raiz.
      view.selectedKey = layout.nodes.front().nodeKey;
      return true;
   }

   const auto& node{layout.nodes[static_cast<std::size_t>(current)]};

   const auto select = [&](const int index) {
      view.selectedKey = layout.nodes[static_cast<std::size_t>(index)].nodeKey;
   };

   switch (dir) {
      case TreeNavigation::Parent: {
         if (node.parentIndex < 0) return false;
         select(node.parentIndex);
         return true;
      }
      case TreeNavigation::FirstChild: {
         if (node.childCount <= 0) return false;
         // Descer para dentro de um galho retraído EXPANDE ele -- é o
         // comportamento de qualquer navegador de árvore, e sem isso a seta
         // para baixo simplesmente não faria nada num nó retraído.
         if (node.collapsed) {
            collapsed.erase(node.nodeKey);
            return true;   // o layout do próximo quadro já traz os filhos
         }
         // Em pré-ordem o primeiro filho é sempre o nó seguinte.
         const int child{current + 1};
         if (child >= static_cast<int>(layout.nodes.size())) return false;
         if (layout.nodes[static_cast<std::size_t>(child)].parentIndex != current) return false;
         select(child);
         return true;
      }
      case TreeNavigation::PrevSibling:
      case TreeNavigation::NextSibling: {
         const int step{dir == TreeNavigation::NextSibling ? 1 : -1};
         for (int i = current + step; i >= 0 && i < static_cast<int>(layout.nodes.size()); i += step) {
            if (layout.nodes[static_cast<std::size_t>(i)].parentIndex == node.parentIndex) {
               select(i);
               return true;
            }
         }
         return false;
      }
   }
   return false;
}

Element renderComponentTree(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                            Box& outCanvasBox, const EstimatedPhase activeFlowPhase)
{
   const int canvasW{view.canvasWidthPx};
   const int canvasH{view.canvasHeightPx};
   auto c = Canvas(canvasW, canvasH);

   const int selectedIndex{findComponentNodeIndex(layout, view.selectedKey)};

   // Cotovelos pai->filho primeiro (ficam por BAIXO dos pontos/rótulos):
   // desce do pai até a barra horizontal, corre até a coluna do filho, desce
   // até ele. É o que dá a leitura de organograma -- uma diagonal direta,
   // com os irmãos lado a lado, viraria um leque ilegível.
   for (const auto& node : layout.nodes) {
      if (node.parentIndex < 0) continue;
      const auto& parent{layout.nodes[static_cast<std::size_t>(node.parentIndex)]};
      const ProjectedNode a{projectTreeNode(parent.x, parent.y, view)};
      const ProjectedNode b{projectTreeNode(node.x, node.y, view)};
      if (!a.onCanvas && !b.onCanvas) continue;

      const int elbowY{a.py + static_cast<int>(std::lround(kRowSpacingPx * kElbowFraction * view.zoom))};
      const auto style = [](Cell& cell) {
         cell.foreground_color = Color::GrayDark;
         cell.dim = true;
      };
      c.DrawPointLine(a.px, a.py, a.px, elbowY, style);
      c.DrawPointLine(a.px, elbowY, b.px, elbowY, style);
      c.DrawPointLine(b.px, elbowY, b.px, b.py, style);
   }

   // Pontos + rótulos numa segunda passada -- mesma razão de
   // app::renderMap(): um rótulo desenhado cedo demais ficaria por baixo do
   // ponto de um nó vizinho desenhado depois.
   for (std::size_t i = 0; i < layout.nodes.size(); i++) {
      const auto& node{layout.nodes[i]};
      const ProjectedNode p{projectTreeNode(node.x, node.y, view)};
      if (!p.onCanvas) continue;

      const Color col{phaseColor(node.phase)};
      // "Pulso" do ciclo de fluxo (ver app/ComponentFlowState.hpp) -- anel
      // AMARELO, raio 3, deliberadamente diferente do anel branco (raio 4)
      // de seleção logo abaixo, pra "está no ciclo agora" e "está
      // selecionado" nunca se confundirem mesmo quando calham no MESMO nó.
      if (node.phase == activeFlowPhase && activeFlowPhase != EstimatedPhase::Unknown) {
         c.DrawPointCircle(p.px, p.py, 3, Color::YellowLight);
      }
      if (static_cast<int>(i) == selectedIndex) c.DrawPointCircle(p.px, p.py, 4, Color::White);
      c.DrawPointCircleFilled(p.px, p.py, 2, col);

      // Rótulo CENTRADO logo abaixo do ponto (na vertical não há espaço à
      // direita: o próximo irmão está ali). 'textY' é encaixado num múltiplo
      // de 4 porque Canvas::DrawText escreve em célula de terminal, e uma
      // coordenada fora da grade seria arredondada de forma inconsistente
      // entre linhas vizinhas.
      const bool isSelected{static_cast<int>(i) == selectedIndex};
      if (view.zoom >= kLabelMinZoom || isSelected) {
         const std::string label{drawnNodeLabel(node)};
         const int textX{p.px - labelWidthPx(label) / 2};
         const int textY{((p.py + 6) / 4) * 4};
         c.DrawText(textX, textY, label, isSelected ? Color::White : col);
      }
   }

   std::string header{"Componentes  [zoom "
      + std::to_string(static_cast<int>(std::lround(view.zoom * 100)))
      + "%]  [-]/[+N] = galho expandido/retraido"};
   if (view.zoom < kLabelMinZoom) header += "  -- rotulos ocultos neste zoom, use []] pra ampliar";
   c.DrawText(2, 2, header, Color::GrayDark);

   return canvas(std::move(c)) | reflect(outCanvasBox) | flex | border;
}

int hitTestComponentTreeNode(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                             const int clickCellX, const int clickCellY)
{
   int bestIndex{-1};
   int bestDist{-1};
   for (std::size_t i = 0; i < layout.nodes.size(); i++) {
      const auto& node{layout.nodes[i]};
      const ProjectedNode p{projectTreeNode(node.x, node.y, view)};
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
      text(node.childCount > 0
           ? (" " + std::to_string(node.childCount) + (node.collapsed ? " filhos (retraido)" : " filhos"))
           : std::string{" (folha)"}) | dim,
      filler(),
   }));
   lines.push_back(separator());

   lines.push_back(hbox({text("slot   ") | dim, text(node.slotName.empty() ? "(raiz)" : node.slotName)}));
   lines.push_back(hbox({text("classe ") | dim, text(node.className)}));

   // A fase é SEMPRE anotada como estimativa quando não é 'Structural' -- é
   // o UNICO dado desta aba que não é medido (ver o comentário grande de
   // app::EstimatedPhase). O resto do card, daqui para baixo, é fato.
   lines.push_back(hbox({text("fase   ") | dim, text(phaseLabel(node.phase)) | color(badge) | bold}));
   if (isHeuristicPhase(node.phase)) {
      // Curto de proposito: no piso de 40 colunas (kDetailPanelMinWidth, a
      // largura que F1/F2 tambem usam num terminal estreito) uma linha mais
      // longa que ~36 caracteres sai cortada no meio da palavra.
      lines.push_back(text("^ estimada por heuristica, nao medida")
                      | color(Color::YellowLight) | dim);
   }

   // ---- estado VIVO: tudo daqui para baixo saiu de getter publico do
   //      proprio objeto MIXR, no instante da descoberta (ver
   //      captureLiveState() em app/ComponentTreeQuery.cpp) ----
   lines.push_back(separator());
   lines.push_back(hbox({
      text(" estado atual ") | bgcolor(Color::Blue) | color(Color::White) | bold,
      text(" (lido, nao estimado)") | dim,
      filler(),
   }));

   if (node.state.empty()) {
      lines.push_back(text("(esta classe nao expoe estado por getter publico)") | dim);
   } else {
      for (const auto& field : node.state) {
         lines.push_back(hbox({
            text(field.label) | dim | size(WIDTH, EQUAL, 22),
            text(field.value),
         }));
      }
   }

   // Dado REAL, quando o nó é um Player -- MESMO xboard::Readout que
   // app/DashboardState.cpp/app/FleetPanel.cpp já usam (não duplicado: é a
   // mesma chamada, só lida aqui também), pra não divergir de vocabulário
   // entre abas.
   if (node.isPlayer && node.playerId >= 0) {
      const mixr::xboard::Readout board{mixr::xboard::get(node.playerId)};
      lines.push_back(separator());
      lines.push_back(hbox({
         text(" decisao (xboard) ") | bgcolor(Color::Blue) | color(Color::White) | bold,
         filler(),
      }));
      lines.push_back(hbox({
         text("player id") | dim | size(WIDTH, EQUAL, 22),
         text(std::to_string(node.playerId)),
      }));
      lines.push_back(hbox({
         text("comportamento") | dim | size(WIDTH, EQUAL, 22),
         text(" " + board.label + " ") | bgcolor(behaviorColor(board.label)) | color(Color::Black),
      }));
      lines.push_back(hbox({
         text("decisoes") | dim | size(WIDTH, EQUAL, 22),
         text(std::to_string(board.decisions)),
      }));
      lines.push_back(hbox({
         text("thread") | dim | size(WIDTH, EQUAL, 22),
         text(board.threadTag >= 0 ? ("T" + std::to_string(board.threadTag)) : std::string{"-"}),
      }));
   }

   return vbox(std::move(lines)) | border | color(badge);
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
