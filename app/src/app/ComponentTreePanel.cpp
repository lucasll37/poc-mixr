#include "app/ComponentTreePanel.hpp"
#include "app/FleetPanel.hpp"   // behaviorColor() -- reusada no card de detalhe

#include "xboard/Board.hpp"

#include "ftxui/dom/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
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

// Nome curto do nó: o do slot quando existe (é o que aparece no .edl), senão
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
   laid.subtreePhaseMask = node.subtreePhaseMask;
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

namespace {

// Marca todo no que PARTICIPA da fase corrente ou que tem algum descendente
// que participa -- ou seja, o CAMINHO que a recursao percorre nesta fase.
// Como 'layout.nodes' esta em pre-ordem (pai sempre antes do filho), uma
// passada de tras pra frente propaga a marca pra cima em O(n).
std::vector<bool> markDescentPath(const ComponentTreeLayout& layout, const EstimatedPhase phase)
{
   std::vector<bool> onPath(layout.nodes.size(), false);
   if (phase == EstimatedPhase::Unknown) return onPath;

   for (std::size_t i = layout.nodes.size(); i-- > 0;) {
      const auto& node{layout.nodes[i]};
      // 'subtreePhaseMask' cobre tambem o que esta ESCONDIDO num galho
      // retraido -- sem ele, fechar os players apagaria a fase 3 inteira,
      // que e justamente quando o usuario mais quer ver por onde a chamada
      // desceria.
      if ((node.subtreePhaseMask & phaseBit(phase)) != 0u) onPath[i] = true;
      if (onPath[i] && node.parentIndex >= 0) onPath[static_cast<std::size_t>(node.parentIndex)] = true;
   }
   return onPath;
}

// Fracao [0,1) do passo de animacao ja percorrida -- e a ONDA descendo. Sai
// do MESMO contador de redesenhos que tickComponentFlowAnimation() usa pra
// decidir quando trocar de fase (nao ha relogio novo aqui), entao ela e
// exatamente "o quanto falta pra proxima fase".
double flowSubStep(const ComponentFlowState& flow, const int redrawsPerSecond = 10)
{
   if (!flow.playing) return 0.0;
   const int speed{std::clamp(flow.stepsPerSecond, 1, 4)};
   const int redrawsPerStep{std::max(1, redrawsPerSecond / speed)};
   const double f{static_cast<double>(flow.redrawsSincePlay) / static_cast<double>(redrawsPerStep)};
   return std::clamp(f, 0.0, 1.0);
}

}   // namespace

Element renderComponentTree(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                            Box& outCanvasBox, const ComponentFlowState& flow,
                            const FrameCallParams& params)
{
   const int canvasW{view.canvasWidthPx};
   const int canvasH{view.canvasHeightPx};
   auto c = Canvas(canvasW, canvasH);

   const EstimatedPhase activeFlowPhase{currentFlowPhase(flow)};
   const int selectedIndex{findComponentNodeIndex(layout, view.selectedKey)};

   // O CAMINHO DA RECURSAO nesta fase, e ate que profundidade ele vai -- a
   // onda desce um nivel por passo de animacao ao longo dele.
   const std::vector<bool> onPath{markDescentPath(layout, activeFlowPhase)};
   const Color pathColor{phaseColor(activeFlowPhase)};
   int maxPathDepth{};
   for (std::size_t i = 0; i < layout.nodes.size(); i++) {
      if (onPath[i]) maxPathDepth = std::max(maxPathDepth, layout.nodes[i].depth);
   }
   const double wave{flowSubStep(flow) * static_cast<double>(maxPathDepth + 1)};

   // Cotovelos pai->filho primeiro (ficam por BAIXO dos pontos/rótulos):
   // desce do pai até a barra horizontal, corre até a coluna do filho, desce
   // até ele. É o que dá a leitura de organograma -- uma diagonal direta,
   // com os irmãos lado a lado, viraria um leque ilegível.
   for (std::size_t i = 0; i < layout.nodes.size(); i++) {
      const auto& node{layout.nodes[i]};
      if (node.parentIndex < 0) continue;
      const auto& parent{layout.nodes[static_cast<std::size_t>(node.parentIndex)]};
      const ProjectedNode a{projectTreeNode(parent.x, parent.y, view)};
      const ProjectedNode b{projectTreeNode(node.x, node.y, view)};
      if (!a.onCanvas && !b.onCanvas) continue;

      const int elbowY{a.py + static_cast<int>(std::lround(kRowSpacingPx * kElbowFraction * view.zoom))};
      // Aceso = esta aresta faz parte da descida desta fase; e literalmente
      // Component::updateTC() chamando obj->tcFrame(dt) neste filho.
      const bool lit{onPath[i]};
      const auto style = [lit, pathColor](Cell& cell) {
         cell.foreground_color = lit ? pathColor : Color::GrayDark;
         cell.dim = !lit;
      };
      c.DrawPointLine(a.px, a.py, a.px, elbowY, style);
      c.DrawPointLine(a.px, elbowY, b.px, elbowY, style);
      c.DrawPointLine(b.px, elbowY, b.px, b.py, style);

      // A frente da onda, quando ela esta atravessando ESTE nivel.
      if (lit && flow.playing && static_cast<int>(wave) == parent.depth) {
         const double f{wave - static_cast<double>(parent.depth)};
         const int wx{a.px + static_cast<int>(std::lround((b.px - a.px) * f))};
         const int wy{a.py + static_cast<int>(std::lround((b.py - a.py) * f))};
         c.DrawPointCircleFilled(wx, wy, 1, Color::White);
      }
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
         // Um galho RETRAIDO que esconde um participante desta fase sai na
         // cor da fase (e nao na dele) -- junto com a aresta acesa, e o
         // convite pra abrir ali. O no que participa ele mesmo ja tem o anel
         // amarelo e o rotulo de chamada, entao nao ha ambiguidade.
         const bool hidesParticipant{node.collapsed && node.phase != activeFlowPhase
            && (node.subtreePhaseMask & phaseBit(activeFlowPhase)) != 0u
            && activeFlowPhase != EstimatedPhase::Unknown};
         const Color labelColor{isSelected ? Color::White : (hidesParticipant ? pathColor : col)};
         c.DrawText(textX, textY, label, labelColor);

         // A CHAMADA que este componente executa nesta fase, com o argumento
         // de verdade -- desenhada logo abaixo do nome, no proprio no. Cabe
         // porque kRowSpacingPx reserva 20 px por nivel e o nome ocupa so a
         // faixa +6..+10.
         const std::string call{nodeCallLabel(node.phase, activeFlowPhase, params)};
         if (!call.empty()) {
            c.DrawText(p.px - labelWidthPx(call) / 2, textY + 4, call, Color::YellowLight);
         }
      }
   }

   std::string header{"Componentes  [zoom "
      + std::to_string(static_cast<int>(std::lround(view.zoom * 100)))
      + "%]  [-]/[+N] = galho expandido/retraido"};
   if (view.zoom < kLabelMinZoom) header += "  -- rotulos ocultos neste zoom, use []] pra ampliar";
   c.DrawText(2, 2, header, Color::GrayDark);

   // Legenda do que o desenho esta dizendo AGORA -- dentro do proprio canvas,
   // pra nao gastar altura fora dele (foi exatamente o erro da versao com
   // painel de texto separado).
   if (activeFlowPhase != EstimatedPhase::Unknown) {
      c.DrawText(2, 6, "aceso = caminho da recursao nesta fase (mesmo por dentro de galho retraido)   "
                 "amarelo = chamada que o no executa", pathColor);
   }

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

Element renderComponentDetail(const ComponentTreeLayoutNode& node, const ComponentFlowState& flow,
                              const FrameCallParams& params)
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

   // ---- "nesta fase": a resposta por-COMPONENTE de "que funcao roda aqui,
   //      com que argumento, e por onde a chamada chega". Fica no card (que
   //      ja tinha espaco sobrando) e nao num painel proprio -- um painel
   //      roubaria altura justamente do desenho da arvore.
   const EstimatedPhase activePhase{currentFlowPhase(flow)};
   const std::string call{nodeCallLabel(node.phase, activePhase, params)};

   lines.push_back(separator());
   lines.push_back(hbox({
      text(" nesta fase ") | bgcolor(phaseColor(activePhase)) | color(Color::Black) | bold,
      text(" " + phaseLabel(activePhase)) | dim,
      filler(),
   }));

   if (call.empty()) {
      lines.push_back(text("este componente nao participa da fase corrente") | dim);
      lines.push_back(hbox({text("participa em ") | dim, text(phaseLabel(node.phase))}));
   } else {
      lines.push_back(hbox({text("executa ") | dim,
                            text(call) | color(Color::YellowLight) | bold}));
      lines.push_back(text("como a chamada chega ate aqui:") | dim);

      // O caminho da recursao, do ponto de entrada ate a chamada desta fase.
      // Mostra a CAUDA, nao a cabeca: o card tem altura fixa (a mesma de
      // F1/F2) e o comeco da cadeia e sempre o mesmo preambulo generico --
      // cortar pelo fim escondia justamente a linha que interessa, a chamada
      // que este componente executa. Medido: com a cabeca, `case 3: process`
      // ficava de fora do card.
      const auto path{frameDescentPath(buildFrameCallChain(activePhase, params))};
      const std::size_t kMaxDescent{10};
      const std::size_t first{path.size() > kMaxDescent ? path.size() - kMaxDescent : 0};
      if (first > 0) {
         lines.push_back(text("  (" + std::to_string(first) + " chamadas antes, ate a thread)") | dim);
      }
      for (std::size_t i = first; i < path.size(); i++) {
         const auto& l{path[i]};
         Elements row{text(std::string(static_cast<std::size_t>(l.depth - path[first].depth), ' '))};
         row.push_back(text(l.text) | color(l.active ? Color::YellowLight : Color::CyanLight));
         if (!l.args.empty()) {
            row.push_back(text((l.args.front() == '(' ? "" : " ") + l.args) | color(Color::Yellow));
         }
         // O arquivo:linha so na linha ATIVA -- nas outras ele competia por
         // largura com o nome da funcao e cortava os dois.
         if (l.active && !l.sourceRef.empty()) {
            row.push_back(filler());
            row.push_back(text(l.sourceRef) | color(Color::Blue) | dim);
         }
         lines.push_back(hbox(std::move(row)));
      }
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

Element renderComponentFlowStatus(const ComponentFlowState& flow, const FrameCallParams& params)
{
   const EstimatedPhase phase{currentFlowPhase(flow)};

   std::ostringstream state;
   state << "SIMULACAO " << (params.paused ? "PAUSADA" : "RODANDO");

   std::ostringstream step;
   step << "[n] passo = Station::tcFrame(dt = " << std::fixed << std::setprecision(6)
        << frameStepSeconds(params) << " s) -- UM frame de verdade: 'sim=' no cabecalho avanca "
        << "esse tanto a cada toque";

   return vbox({
      hbox({
         text(params.paused ? " || " : " |> ")
            | bgcolor(params.paused ? Color::Yellow : Color::Green) | color(Color::Black) | bold,
         text(" " + state.str() + " ") | bold
            | color(params.paused ? Color::Yellow : Color::Green),
         separator(),
         text(" fase " + std::to_string(flow.cycleIndex + 1) + "/"
              + std::to_string(kComponentFlowCycleLen) + ": " + phaseLabel(phase) + " ")
            | color(phaseColor(phase)) | bold,
         filler(),
      }),
      text(step.str()) | color(Color::CyanLight),
   });
}

namespace {

// Rotulo curto de cada bloco da faixa -- o nome do que RODA naquela fase,
// nao o nome da fase (que ja esta na linha de status logo acima).
std::string phaseStripLabel(const EstimatedPhase phase)
{
   switch (phase) {
      case EstimatedPhase::Structural:         return "tcFrame";
      case EstimatedPhase::DynamicsPhase0:      return "0 dynamics";
      case EstimatedPhase::SensorPhase1And2:    return "1 transmit / 2 receive";
      case EstimatedPhase::DecisionPhase3:      return "3 process";
      case EstimatedPhase::DecisionBackground:  return "Agent::controller";
      case EstimatedPhase::Background:          return "updateData";
      case EstimatedPhase::Unknown:
      default:                                 return "?";
   }
}

}   // namespace

Element renderFramePhaseStrip(const ComponentFlowState& flow, const FrameCallParams& params)
{
   const EstimatedPhase active{currentFlowPhase(flow)};

   std::ostringstream tc;
   tc << " FRAME T/C  " << std::fixed << std::setprecision(1) << params.tcRateHz << " Hz  dt="
      << std::setprecision(6) << frameStepSeconds(params) << "s ";

   Elements row{text(tc.str()) | bgcolor(Color::GrayDark) | color(Color::White) | bold};

   for (std::size_t i = 0; i < kComponentFlowCycleLen; i++) {
      const EstimatedPhase phase{kComponentFlowCycle[i]};

      // A fronteira entre o grupo do frame de tempo critico e o de fundo:
      // sao THREADS diferentes, e essa e a informacao que a faixa existe pra
      // dar de um relance.
      if (phase == EstimatedPhase::DecisionBackground) {
         std::ostringstream bg;
         bg << "  FUNDO  " << std::fixed << std::setprecision(1) << params.bgRateHz << " Hz ";
         row.push_back(text(" =>") | color(Color::GrayDark) | bold);
         row.push_back(text(bg.str()) | bgcolor(Color::GrayDark) | color(Color::White) | bold);
      } else if (i > 0) {
         row.push_back(text("->") | color(Color::GrayDark));
      }

      const bool isActive{phase == active};
      Element block{text(" " + phaseStripLabel(phase) + " ")};
      if (isActive) block = block | bgcolor(phaseColor(phase)) | color(Color::Black) | bold;
      else          block = block | color(phaseColor(phase)) | dim;
      row.push_back(block);
   }
   row.push_back(filler());
   return hbox(std::move(row));
}


} // namespace app
