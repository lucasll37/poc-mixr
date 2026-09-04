#pragma once

#include "app/ComponentFlowState.hpp"
#include "app/ComponentTreeQuery.hpp"
#include "app/FrameCallChain.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <set>
#include <string>
#include <vector>

//------------------------------------------------------------------------------
// A aba "Componentes" (F6): desenho gráfico e navegável da árvore de
// componentes REAL de app::discoverComponentTree() -- mesmo espírito de
// app/MapPanel.hpp (pan/zoom/clique num ftxui::Canvas de tamanho
// RESPONSIVO, ver fitComponentTreeCanvasToBox() abaixo, que segue a MESMA
// receita de app::fitMapCanvasToBox() documentada no CLAUDE.md: dimensionar
// o Canvas pela caixa do quadro ANTERIOR via ftxui::reflect, nunca por uma
// constante).
//
// A animação de fluxo entre fases (ver app/ComponentFlowState.hpp para o
// "porque" e o aviso de que é um MODELO CONCEITUAL, não uma medição)
// destaca os nós cuja fase estimada bate com a fase atual do ciclo.
//
// TERCEIRA PASSADA (esta), três mudanças pedidas em conjunto:
//
//  1) A árvore é VERTICAL -- raiz no topo, filhos abaixo dela, irmãos lado a
//     lado, ligados por cotovelos (o desenho clássico de organograma). Antes
//     crescia da esquerda para a direita.
//
//  2) Dá para RETRAIR/EXPANDIR a árvore e cada galho (CollapsedNodes abaixo).
//     É o que torna a forma vertical utilizável: sem retrair, a largura de
//     uma árvore de produção (4 players x ~15 subsistemas) é dezenas de
//     vezes a largura do terminal, porque na vertical cada FOLHA custa a
//     largura do próprio rótulo -- ao contrário do layout horizontal antigo,
//     em que cada folha custava só uma LINHA.
//
//  3) O card de detalhe passou a mostrar o ESTADO VIVO do componente
//     selecionado (ComponentTreeNode::state, lido por getter público -- ver
//     captureLiveState() em ComponentTreeQuery.cpp) e deixou de fixar o
//     próprio tamanho: quem dimensiona é o chamador, com o MESMO
//     'detailPanelWidth'/'kDetailPanelHeight' das abas F1/F2.
//------------------------------------------------------------------------------
namespace app {

// Chaves (ComponentTreeNode::nodeKey) dos nós RETRAÍDOS -- os filhos deles
// não entram no layout. Guardado por CHAVE e não por índice porque a árvore
// é redescoberta a cada redesenho e o índice de um nó muda quando algo
// nasce/some nela (um míssil liberado, um fantasma DIS).
using CollapsedNodes = std::set<std::string>;

// Um nó já posicionado. 'x'/'y' são PIXEL DE CANVAS em zoom 1.0 (braille:
// 2px por célula na horizontal, 4px na vertical -- mesma unidade de
// app::MapViewState::canvasWidthPx/HeightPx); a projeção para a tela aplica
// pan e zoom por cima. São 'double' e não coluna/linha inteiras porque na
// vertical a largura de uma folha é a do PRÓPRIO RÓTULO (folhas com nomes
// de tamanhos diferentes não cabem numa grade uniforme sem ou sobrepor
// texto ou desperdiçar largura).
struct ComponentTreeLayoutNode
{
   std::string nodeKey;
   std::string slotName;
   std::string className;
   bool isPlayer{};
   int playerId{-1};
   EstimatedPhase phase{EstimatedPhase::Unknown};
   std::vector<ComponentStateField> state;

   int depth{};
   double x{};
   double y{};
   int parentIndex{-1};   // índice em ComponentTreeLayout::nodes, -1 na raiz

   // Filhos REAIS do nó na árvore descoberta -- continua > 0 mesmo com o nó
   // retraído (é o que permite desenhar "[+3]" e saber que há o que expandir).
   int childCount{};
   bool collapsed{};

   // Copiada de ComponentTreeNode -- é o que permite acender o caminho da
   // recursão até um participante que está DENTRO de um galho retraído (ver
   // o comentário lá).
   unsigned int subtreePhaseMask{};
};

struct ComponentTreeLayout
{
   std::vector<ComponentTreeLayoutNode> nodes;   // pré-ordem (pai sempre antes do filho)

   // Extensão ocupada, em px de canvas @zoom 1.0 -- usada por
   // fitComponentTreeToContent().
   double minX{};
   double maxX{};
   double minY{};
   double maxY{};
};

// Achata a árvore (DFS) calculando (x, y) de cada nó visível -- função PURA,
// sem FTXUI, testável isolada (ver o padrão de app/MapGeometry.hpp/
// app/BehaviorTreeView.hpp: geometria/estrutura separada do desenho).
// Filhos de um nó cuja chave esteja em 'collapsed' não entram no resultado.
ComponentTreeLayout layoutComponentTree(const ComponentTreeNode& root, const CollapsedNodes& collapsed);

// Índice do nó com esta chave, ou -1. É como a seleção sobrevive à
// redescoberta da árvore a cada redesenho.
int findComponentNodeIndex(const ComponentTreeLayout& layout, const std::string& nodeKey);

// Estado de navegação (pan/zoom/seleção/arrasto) -- mantido pelo CHAMADOR
// (app/DashboardLoop.cpp), no MESMO espírito de app::MapViewState.
struct ComponentTreeViewState
{
   // Ponto do layout (px @zoom 1.0) que fica no CENTRO do canvas.
   double panX{};
   double panY{};
   double zoom{1.0};

   std::string selectedKey;   // "" = nenhum

   // Mesma técnica de app::MapViewState::canvasWidthPx/HeightPx -- NÃO é
   // constante, acompanha a área que o layout de fato deu à aba (ver
   // fitComponentTreeCanvasToBox()).
   int canvasWidthPx{240};
   int canvasHeightPx{120};

   // Arrasto em andamento -- mesmos quatro campos e mesmo critério de
   // clique-vs-arrasto de app::MapViewState (ver o comentário lá:
   // deslocamento total entre Pressed e Released <= 1 célula é clique).
   bool dragging{};
   int pressX{};
   int pressY{};
   int dragLastX{};
   int dragLastY{};
};

const double kTreeZoomStep{1.25};
const double kTreeMinZoom{0.2};
const double kTreeMaxZoom{4.0};

const int kTreeCanvasMinCellsW{20};
const int kTreeCanvasMinCellsH{6};

// Profundidade até a qual a árvore nasce EXPANDIDA (ver
// collapseDeeperThan()). 3 = Station -> simulation/dataRecorder/networks ->
// players -> os players em si, cada um retraído: dá para ver de cara QUEM
// está no cenário sem estourar a largura do terminal (as subárvores de cada
// player, essas sim largas, ficam a um [Enter] de distância).
const int kTreeInitialExpandDepth{3};

// Cor por fase estimada -- só um agrupamento visual (ver renderComponentTree());
// a legenda/aviso de heurística fica no card de detalhe, não aqui.
ftxui::Color phaseColor(EstimatedPhase phase);

// Mesma receita de app::fitMapCanvasToBox() (ver o comentário grande lá e
// no CLAUDE.md, "décima sexta passada"): dimensiona o canvas pela caixa do
// quadro ANTERIOR, nunca por uma constante. No-op se a caixa for degenerada
// (primeiro quadro, ou terminal menor que o piso).
void fitComponentTreeCanvasToBox(ComponentTreeViewState& view, const ftxui::Box& box);

void zoomComponentTree(ComponentTreeViewState& view, bool zoomIn);

// Desloca o pan em PIXEL DE CANVAS (o chamador já converte célula->pixel,
// mesmo fator 2/4 que app::panMap() usa).
void panComponentTree(ComponentTreeViewState& view, double screenRightPx, double screenDownPx);

// Centraliza a vista num nó (usado ao selecionar por clique, por navegação
// de teclado, ou pelo botão [c]).
void centerComponentTreeOn(ComponentTreeViewState& view, const ComponentTreeLayoutNode& node);

// Traz o nó para dentro do canvas SE ele estiver fora (ou coladinho na
// borda) -- chamada depois de cada passo de navegação por teclado. Não mexe
// no pan quando o nó já está confortavelmente visível, senão cada seta
// arrastaria a vista inteira e o usuário perderia a referência do desenho.
void ensureComponentNodeVisible(ComponentTreeViewState& view, const ComponentTreeLayoutNode& node);

// Enquadra a árvore VISÍVEL inteira no canvas -- chamada uma vez na
// primeira descoberta e de novo a cada "expandir/retrair tudo" (aí a
// extensão muda de ordem de grandeza e o pan/zoom anterior perde sentido).
void fitComponentTreeToContent(ComponentTreeViewState& view, const ComponentTreeLayout& layout);

//---- retrair/expandir -------------------------------------------------------
// Todas operam sobre o CONJUNTO de chaves, não sobre a árvore -- é o que
// faz o estado sobreviver à redescoberta a cada redesenho.

// Alterna o nó (no-op se ele não tem filhos -- retrair uma folha não
// significaria nada e só deixaria lixo no conjunto).
void toggleComponentNodeCollapsed(CollapsedNodes& collapsed, const ComponentTreeLayoutNode& node);

// Marca como retraído todo nó COM FILHOS a partir de 'maxDepth' (inclusive),
// varrendo a árvore descoberta. 'maxDepth' 0 retrai a própria raiz.
void collapseDeeperThan(const ComponentTreeNode& root, int maxDepth, CollapsedNodes& collapsed);

// Retrai todo nó com filhos (equivale a collapseDeeperThan(root, 0)).
void collapseAllComponentNodes(const ComponentTreeNode& root, CollapsedNodes& collapsed);

//---- navegação por teclado --------------------------------------------------
enum class TreeNavigation {
   Parent,        // seta para cima
   FirstChild,    // seta para baixo (expande antes, se estiver retraído)
   PrevSibling,   // seta para a esquerda
   NextSibling,   // seta para a direita
};

// Move 'view.selectedKey' pelo layout. Devolve false quando não há para onde
// ir (já é a raiz, é folha, é o primeiro/último irmão) -- o chamador usa isso
// para decidir se precisa reenquadrar. Sem seleção nenhuma, qualquer direção
// seleciona a RAIZ (é o "entrar na árvore" pelo teclado).
bool navigateComponentTree(const ComponentTreeLayout& layout, ComponentTreeViewState& view,
                           TreeNavigation dir, CollapsedNodes& collapsed);

// Desenha a árvore inteira num ftxui::Canvas -- nós como pontos com rótulo
// centrado LOGO ABAIXO deles, cotovelos ligando pai-filho, cor por fase
// estimada, anel branco no nó selecionado. 'outCanvasBox' recebe a caixa de
// tela (ftxui::reflect), mesmo uso de app::renderMap(): o chamador usa pra
// saber se um clique caiu dentro do canvas antes de tratar como
// arrasto/seleção.
//
// 'activeFlowPhase' acrescenta um anel AMARELO -- o "pulso" -- em todo nó
// cuja 'phase' bate com ela; ver app/ComponentFlowState.hpp para o que essa
// fase representa (o ciclo CONCEITUAL, não uma medição). Cor deliberadamente
// distinta do anel branco de seleção, pra "selecionado" e "ativo no ciclo
// agora" nunca se confundirem visualmente mesmo quando os dois calham no
// mesmo nó.
// Além do pulso, o desenho responde graficamente "o que está sendo chamado
// agora, e por onde a chamada chega": (a) todo nó que participa da fase
// corrente ganha, logo abaixo do nome, o RÓTULO DA CHAMADA com o argumento
// de verdade -- `dynamics(0.020s)`, `process(0.020s)`; (b) as arestas do
// CAMINHO DA RECURSÃO até esses nós saem acesas na cor da fase, contra o
// cinza das demais -- é literalmente `Component::updateTC()` descendo por
// `obj->tcFrame(dt)` em cada filho; e (c) uma ONDA percorre esse caminho de
// cima para baixo, um nível por vez, enquanto a reprodução está tocando.
ftxui::Element renderComponentTree(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                                   ftxui::Box& outCanvasBox, const ComponentFlowState& flow,
                                   const FrameCallParams& params);

// Converte um clique em CELULAS de terminal relativas ao canvas (já
// subtraído o canto do Box) no ÍNDICE do nó mais próximo em
// 'layout.nodes', ou -1. Mesma tolerância de 1 célula de app::hitTestEntity().
int hitTestComponentTreeNode(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                             int clickCellX, int clickCellY);

// O card de detalhe do nó selecionado -- nome do slot, classe (factory
// name/RTTI), fase estimada (SEMPRE com o aviso de heurística quando
// aplicável -- ver app::isHeuristicPhase()), o ESTADO VIVO lido do objeto
// (ComponentTreeNode::state) e, quando o nó é um Player, o mesmo
// xboard::Readout que app/DashboardState.cpp/app/FleetPanel.cpp já usam.
//
// NÃO fixa largura nem altura: quem dimensiona é o chamador, com o MESMO
// 'detailPanelWidth'/'kDetailPanelHeight' das abas F1/F2 (pedido explícito:
// o subquadro lateral tem de ter o mesmo tamanho nas três abas).
// Recebe também a fase corrente para acrescentar a seção "nesta fase": o
// que ESTE componente executa agora (função, argumento, arquivo:linha) e o
// CAMINHO DA RECURSÃO que chega até ele, de `Station::tcFrame` para baixo.
// Vai aqui, e não num painel próprio, porque o card já tinha espaço
// sobrando e porque a informação é por-componente, não global.
ftxui::Element renderComponentDetail(const ComponentTreeLayoutNode& node,
                                     const ComponentFlowState& flow, const FrameCallParams& params);

// Legenda de cores -- uma linha por fase de kComponentFlowCycle, reusando
// phaseColor()/phaseLabel() (não inventa vocabulário novo).
ftxui::Element renderComponentFlowLegend();

// "fase atual: fase 3 (decisao, no frame T/C)" + play/pause/velocidade +
// o aviso de MODELO CONCEITUAL (ver app/ComponentFlowState.hpp).
ftxui::Element renderComponentFlowStatus(const ComponentFlowState& flow, const FrameCallParams& params);

// A FAIXA DE FASES: um diagrama de pipeline, um bloco por fase do ciclo, com
// a fase corrente preenchida. É a leitura de um relance ("estamos na fase
// 3") que o texto sozinho não dá -- e separa visualmente o grupo do frame de
// tempo crítico (as quatro fases, numa thread do pool) do grupo de fundo
// (updateData, noutra thread).
ftxui::Element renderFramePhaseStrip(const ComponentFlowState& flow, const FrameCallParams& params);



} // namespace app
