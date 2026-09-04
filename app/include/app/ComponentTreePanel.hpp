#pragma once

#include "app/ComponentFlowState.hpp"
#include "app/ComponentTreeQuery.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <string>
#include <vector>

//------------------------------------------------------------------------------
// A aba "Componentes" (F6): desenho gráfico e navegável da árvore de
// componentes REAL de app::discoverComponentTree() -- mesmo espírito de
// app/MapPanel.hpp (pan/zoom/clique num ftxui::Canvas de tamanho
// RESPONSIVO, ver fitComponentTreeCanvasToBox() abaixo, que segue a MESMA
// receita de app::fitMapCanvasToBox() documentada no CLAUDE.md: dimensionar
// o Canvas pela caixa do quadro ANTERIOR via ftxui::reflect, nunca por uma
// constante -- senão o mapa/a árvore só ocupa parte do quadro reservado em
// terminais maiores, ou corta em terminais menores).
//
// PRIMEIRA METADE da feature: so a estrutura estatica, navegavel, com
// selecao e card de detalhe.
//
// SEGUNDA METADE (esta): a animacao de fluxo entre fases (ver
// app/ComponentFlowState.hpp para o "porque" e o aviso de que e um MODELO
// CONCEITUAL, nao uma medicao) -- renderComponentTree() ganhou o parametro
// 'activeFlowPhase' pra destacar os nos cuja fase estimada bate com a fase
// atual do ciclo, e este header ganhou renderComponentFlowLegend()/
// renderComponentFlowStatus().
//------------------------------------------------------------------------------
namespace app {

// Layout em ARVORE simples, esquerda->direita: profundidade vira coluna
// (eixo X), ordem de folha (pos-ordem) vira linha (eixo Y) -- o mesmo
// principio de qualquer desenho de arvore ('depth' determina X, a media
// das linhas dos filhos determina a linha de um no interno). Sem
// sofisticacao tipo Reingold-Tilford (sem evitar sobreposicao entre
// subarvores de tamanhos muito diferentes) -- pedido explicito: "nao
// precisa ser sofisticado, so legivel", e o cenario de producao (4 falcons,
// ~10-15 nos cada) fica confortavel mesmo com o layout ingenuo.
struct ComponentTreeLayoutNode
{
   std::string slotName;
   std::string className;
   bool isPlayer{};
   int playerId{-1};
   EstimatedPhase phase{EstimatedPhase::Unknown};

   int depth{};         // coluna (0 = raiz)
   double row{};         // linha (fracionaria para nos internos -- media dos filhos)
   int parentIndex{-1};  // indice em ComponentTreeLayout::nodes, -1 na raiz
};

struct ComponentTreeLayout
{
   std::vector<ComponentTreeLayoutNode> nodes;   // ordem de pre-ordem (pai sempre antes do filho)
};

// Achata a arvore (DFS) calculando (depth, row) de cada no -- funcao PURA,
// sem FTXUI, testavel isolada (ver o padrao de app/MapGeometry.hpp/
// app/BehaviorTreeView.hpp: geometria/estrutura separada do desenho).
ComponentTreeLayout layoutComponentTree(const ComponentTreeNode& root);

// Estado de navegacao (pan/zoom/selecao/arrasto) -- mantido pelo CHAMADOR
// (app/DashboardLoop.cpp), no MESMO espirito de app::MapViewState.
struct ComponentTreeViewState
{
   double panDepth{};   // coluna que fica no centro horizontal da tela
   double panRow{};      // linha que fica no centro vertical da tela
   double zoom{1.0};      // multiplicador de espacamento (1.0 = kColSpacingPx/kRowSpacingPx)

   int selectedIndex{-1};   // indice em ComponentTreeLayout::nodes, -1 = nenhum

   // Mesma tecnica de app::MapViewState::canvasWidthPx/HeightPx -- NAO e
   // constante, acompanha a area que o layout de fato deu a aba (ver
   // fitComponentTreeCanvasToBox()).
   int canvasWidthPx{240};
   int canvasHeightPx{120};

   // Arrasto em andamento -- mesmos quatro campos e mesmo criterio de
   // clique-vs-arrasto de app::MapViewState (ver o comentario la: deslocamento
   // total entre Pressed e Released <= 1 celula e clique).
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

// Cor por fase estimada -- so um agrupamento visual (ver renderComponentTree());
// a legenda/aviso de heuristica fica no card de detalhe, nao aqui.
ftxui::Color phaseColor(EstimatedPhase phase);

// Mesma receita de app::fitMapCanvasToBox() (ver o comentario grande la e
// no CLAUDE.md, "decima sexta passada"): dimensiona o canvas pela caixa do
// quadro ANTERIOR, nunca por uma constante. No-op se a caixa for degenerada
// (primeiro quadro, ou terminal menor que o piso).
void fitComponentTreeCanvasToBox(ComponentTreeViewState& view, const ftxui::Box& box);

void zoomComponentTree(ComponentTreeViewState& view, bool zoomIn);

// Desloca o pan em PIXEL DE CANVAS (o chamador ja converte celula->pixel,
// mesmo fator 2/4 que app::panMap() usa -- ver o comentario grande no
// CatchEvent do mapa em DashboardLoop.cpp) -- a conversao pixel->"unidade
// de layout" (dividir pelo espacamento de coluna/linha vezes o zoom) fica
// INTERNA (ver app/ComponentTreePanel.cpp), porque so o .cpp conhece o
// espacamento de coluna/linha.
void panComponentTree(ComponentTreeViewState& view, double screenRightPx, double screenDownPx);

// Centraliza a vista num no (usado ao selecionar por clique ou por
// navegacao no card de detalhe).
void centerComponentTreeOn(ComponentTreeViewState& view, const ComponentTreeLayoutNode& node);

// Enquadra a arvore INTEIRA na primeira vez que ela fica disponivel --
// sem isto, o pan/zoom DEFAULT (0,0 / 1.0x) deixa a maior parte da arvore
// fora do canvas: a raiz (depth=0) fica na linha MEDIA de toda a arvore
// (ver layoutSubtree(), a media recursiva das linhas dos filhos), que num
// cenario de producao (4 players x ~15-20 subsistemas cada) fica dezenas
// de linhas abaixo do canto onde o pan comeca. Calcula o menor zoom que
// cabe toda a extensao de profundidade/linha no canvas atual e centraliza
// no meio dela -- chamada UMA VEZ (ver 'hasAutoFitted' em
// DashboardLoop.cpp), nao a cada redesenho, para nao brigar com pan/zoom
// manual do usuario depois.
void fitComponentTreeToContent(ComponentTreeViewState& view, const ComponentTreeLayout& layout);

// Desenha a arvore inteira num ftxui::Canvas -- nos como pontos com
// rotulo, linhas ligando pai-filho, cor por fase estimada, anel branco no
// no selecionado. 'outCanvasBox' recebe a caixa de tela (ftxui::reflect),
// mesmo uso de app::renderMap(): o chamador usa pra saber se um clique caiu
// dentro do canvas antes de tratar como arrasto/selecao.
//
// 'activeFlowPhase' (SEGUNDA METADE) acrescenta um anel AMARELO -- o
// "pulso" -- em todo no cuja 'phase' bate com ela; ver
// app/ComponentFlowState.hpp para o que essa fase representa (o ciclo
// CONCEITUAL, nao uma medicao). Cor deliberadamente distinta do anel branco
// de selecao, pra "selecionado" e "ativo no ciclo agora" nunca se
// confundirem visualmente mesmo quando os dois calham no mesmo no.
ftxui::Element renderComponentTree(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                                   ftxui::Box& outCanvasBox, EstimatedPhase activeFlowPhase);

// Converte um clique em CELULAS de terminal relativas ao canvas (ja
// subtraido o canto do Box) no INDICE do no mais proximo em
// 'layout.nodes', ou -1. Mesma tolerancia de 1 celula de app::hitTestEntity().
int hitTestComponentTreeNode(const ComponentTreeLayout& layout, const ComponentTreeViewState& view,
                             int clickCellX, int clickCellY);

// O card de detalhe do no selecionado -- nome do slot, classe (factory
// name/RTTI), fase estimada (SEMPRE com o aviso de heuristica quando
// aplicavel -- ver app::isHeuristicPhase()), e o dado REAL de xboard/
// getMode() quando o no e um Player (ver app/ComponentTreePanel.cpp: reusa
// o MESMO xboard::Readout que app/DashboardState.cpp/app/FleetPanel.cpp ja
// usam, para nao divergir de vocabulario/valor entre abas).
ftxui::Element renderComponentDetail(const ComponentTreeLayoutNode& node);

// Legenda de cores -- uma linha por fase de kComponentFlowCycle, reusando
// phaseColor()/phaseLabel() (nao inventa vocabulario novo). Estatica, sem
// estado: so lista as seis fases do ciclo conceitual.
ftxui::Element renderComponentFlowLegend();

// "fase atual: fase 3 (decisao, no frame T/C)" + play/pause/velocidade +
// o aviso de MODELO CONCEITUAL (ver app/ComponentFlowState.hpp) -- fica
// pequeno de proposito, o aviso mais completo mora no comentario do header
// e no card de detalhe (isHeuristicPhase()).
ftxui::Element renderComponentFlowStatus(const ComponentFlowState& flow);

} // namespace app
