#pragma once

#include "app/MapPanel.hpp"

//------------------------------------------------------------------------------
// A geometria PURA por tras da aba Mapa -- projecao mundo->tela e sua
// inversa, mais a escolha de intervalo de curva de nivel. Nenhum tipo de
// FTXUI aqui (nem Canvas, nem Element): so os `double`/`int` que a
// projecao de fato manipula. app/MapPanel.cpp usa este header pra
// desenhar; tests/app/test_map_geometry.cpp usa o MESMO header, com o
// MESMO .cpp, pra testar a regra isolada (sem levantar Station nem
// terminal nenhum).
//
// Promovido de um namespace anonimo dentro de MapPanel.cpp -- a regra ja
// era pura e ja estava separada do desenho; so nunca tinha ficado visivel
// fora do arquivo, entao nunca teve teste permanente (o historico do
// projeto documenta ter verificado isto varias vezes com um "main.cpp
// descartavel", nunca commitado -- ver CLAUDE.md, secao "./app").
//------------------------------------------------------------------------------
namespace app {
namespace mapgeometry {

// Dimensao FIXA do canvas, em "pixels" de braille -- fonte unica pro
// desenho (Canvas(kCanvasW, kCanvasH) em MapPanel.cpp) E pra geometria.
// A largura deriva de kMapCanvasWidthCells (MapPanel.hpp), publica porque
// DashboardLoop.cpp tambem usa ela pra calcular a largura do card de
// detalhe.
constexpr int kCanvasW{kMapCanvasWidthCells * 2};
constexpr int kCanvasH{120};

constexpr double kPi{3.14159265358979323846};
constexpr double kDeg2Rad{kPi / 180.0};

struct Projected { int px{}; int py{}; bool onCanvas{}; };

// A MESMA rotacao serve pras duas perspectivas -- ver o comentario de
// Perspective em MapPanel.hpp: o eixo livre (vertical) e compartilhado.
//
// TopDown: (rotE, rotN) sao N/E girados por 'view.viewYawDeg' -- px anda
// com rotE (largura da tela), py anda com -rotN (profundidade vira "pra
// cima"). Lateral: px continua sendo rotE (largura -- distancia lateral
// vista de 'viewYawDeg'), py passa a ser a ALTITUDE relativa ao pan.
Projected project(double northM, double eastM, double altitudeM, const MapViewState& view);

// INVERSA de project() para o plano N/E (TopDown) -- dado um pixel do
// canvas, devolve o ponto do MUNDO (northM/eastM) que ele representa.
void worldFromCanvasTopDown(int px, int py, const MapViewState& view,
                            double& northM, double& eastM);

// Mesma ideia, mas para a vista Lateral: aqui so a coluna (px) importa --
// o perfil de terreno mostrado e o do CHAO ao longo da linha de visada
// que passa pelo pan (profundidade zero, rotN=0), nao um ponto qualquer
// do plano -- e o "corte" que aparece de lado. Inversa PARCIAL, de
// proposito: um ponto fora dessa linha (profundidade != 0) nao e
// recuperado.
void worldFromCanvasLateral(int px, const MapViewState& view, double& northM, double& eastM);

// Intervalo de curva de nivel "redondo" pro alcance de elevacao amostrado
// -- mesma ideia de um mapa topografico de papel escolher 10/20/50/100m
// conforme a escala, nao um numero arbitrario.
double contourIntervalFor(double minM, double maxM);

} // namespace mapgeometry
} // namespace app
