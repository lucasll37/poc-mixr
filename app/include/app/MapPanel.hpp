#pragma once

#include "app/DashboardState.hpp"
#include "app/TerrainQuery.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <array>
#include <deque>
#include <map>

//------------------------------------------------------------------------------
// A aba "Mapa": vista navegavel da simulacao -- de cima (N/E) ou de lado
// (distancia x altitude), com pan, zoom e rotacao. Fonte de posicao e
// SOMENTE EntityState::northM/eastM/altitudeM (mixr::models::Player::
// getPosition()/getAltitudeM(), generico -- ver app/DashboardState.cpp) --
// funciona para qualquer tipo de player sem mudanca nenhuma aqui, inclusive
// um modelo desconhecido futuro.
//
// O estado de navegacao (pan/zoom/arrasto/rotacao/rastro) fica em
// MapViewState, mantido pelo CHAMADOR (app/DashboardLoop.cpp -- e quem tem a
// vida do laco de eventos); este arquivo so tem funcoes puras: desenhar dado
// o estado, aplicar pan/zoom/centralizar/girar SOBRE o estado, e achar qual
// entidade esta sob um clique (hitTestEntity).
//------------------------------------------------------------------------------
namespace app {

// TopDown: plano N/E, olhando de cima -- o eixo livre (em torno do qual da
// pra girar) e o vertical, e girar troca qual rumo do mundo aponta pra cima
// da tela (como girar uma rosa dos ventos).
//
// Lateral: plano "largura x altitude" -- a horizontal da tela e a projecao
// de N/E sobre o eixo PERPENDICULAR ao rumo de visada (o mesmo 'viewYawDeg'
// de cima, reaproveitado: e o mesmo eixo vertical livre, agora escolhendo de
// que rumo do mundo se esta olhando a formacao), a vertical da tela e
// altitude. As duas projecoes compartilham a MESMA rotacao por design --
// "em todos os casos" o eixo livre e o vertical.
enum class Perspective { TopDown, Lateral };

struct MapViewState
{
   double panNorthM{};
   double panEastM{};
   double panAltM{};
   double metersPerCell{50.0};
   double viewYawDeg{};
   Perspective perspective{Perspective::TopDown};
   bool showTrails{};

   // Vista de terreno (de cima: sombreado por elevacao amostrada num grid;
   // de lado: perfil/silhueta do chao) -- pedido explicito de poder
   // habilitar/desabilitar. Desligada por padrao: nem todo cenario tem
   // banco de terreno carregado, e o sombreamento compete visualmente com
   // entidades quando nao e o que se quer ver.
   bool showTerrain{};

   // Arrasto em andamento (botao esquerdo). 'pressX/pressY' fica FIXO no
   // pixel do Pressed original -- serve pra distinguir CLIQUE (selecionar a
   // entidade sob o cursor) de ARRASTO (mover o mapa): se o Released
   // acontece perto o bastante do Pressed, foi clique. 'dragLastX/Y' anda a
   // cada Moved, para calcular so o DELTA do passo mais recente (FTXUI
   // entrega coordenada ABSOLUTA de terminal, nao relativa ao canvas).
   bool dragging{};
   int pressX{};
   int pressY{};
   int dragLastX{};
   int dragLastY{};

   // Rastro por entidade (chave = Player::getID()) -- janela deslizante de
   // posicoes recentes, so mantida/desenhada quando 'showTrails'. Fica AQUI
   // (nao em DashboardState) porque e estado de UI que sobrevive entre
   // amostras, nao um fato instantaneo da simulacao.
   std::map<int, std::deque<std::array<double, 3>>> trails;   // {northM, eastM, altitudeM}
};

const double kMapZoomStep{1.25};
const double kMapMinMetersPerCell{2.0};
const double kMapMaxMetersPerCell{200000.0};
const double kMapRotateStepDeg{15.0};
const std::size_t kMapTrailLength{80};   // ~8s a 10 Hz

// Largura do canvas, em CELULAS de terminal -- publica porque
// DashboardLoop.cpp usa ela como referencia pra calcular a largura do card
// de detalhe ("ocupando por referencia ate onde o mapa acaba", pedido
// explicito). MapPanel.cpp deriva o tamanho do Canvas (em pixel de
// braille) DESTA constante, nao o contrario -- fonte unica.
const int kMapCanvasWidthCells{120};

// Desloca o pan em termos de TELA (direita/cima, em metros de canvas), nao
// de mundo -- ja leva em conta 'viewYawDeg' e 'perspective': a horizontal
// sempre e a mesma (rotE, ver MapPanel.cpp), so a vertical muda de sentido
// (norte girado no TopDown, altitude no Lateral). Chamado igual pelas duas
// perspectivas -- DashboardLoop.cpp nao precisa saber qual esta ativa.
void panMap(MapViewState& view, double screenRightM, double screenUpM);
void zoomMap(MapViewState& view, bool zoomIn);
void rotateMap(MapViewState& view, bool clockwise);
void centerMapOn(MapViewState& view, const EntityState& e);

// Acrescenta a posicao atual de cada entidade ao rastro dela e descarta o
// que passou de kMapTrailLength -- chamada uma vez por amostra nova (ver
// DashboardLoop.cpp), nao por redesenho (evita rastro com pontos
// duplicados).
void updateTrails(MapViewState& view, const std::vector<EntityState>& entities);

// 'outCanvasBox' recebe a caixa de tela do canvas apos o desenho (via
// ftxui::reflect) -- o chamador usa para saber se um clique caiu DENTRO do
// mapa (e nao, por exemplo, no botao de outra aba desenhado no mesmo
// quadro) antes de tratar como arrasto/selecao. 'focusedId' realca (anel) a
// entidade selecionada. 'terrainSampler' so e CHAMADO quando
// 'view.showTerrain' esta ligado -- um std::function vazio (WorldModel sem
// terreno carregado) e seguro de passar sempre, mesmo com a vista ligada:
// simplesmente nao desenha nada (ver app/TerrainQuery.hpp).
ftxui::Element renderMap(const std::vector<EntityState>& entities, const MapViewState& view,
                         int focusedId, ftxui::Box& outCanvasBox,
                         const TerrainSampler& terrainSampler);

// Converte um clique em CELULAS de terminal relativas ao canvas (ja
// subtraido o canto do Box) na entidade mais proxima, ou -1. Usa a MESMA
// projecao de renderMap() -- ver app/MapPanel.cpp.
int hitTestEntity(const std::vector<EntityState>& entities, const MapViewState& view,
                  int clickCellX, int clickCellY);

} // namespace app
