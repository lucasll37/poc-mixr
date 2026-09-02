#include "app/MapPanel.hpp"
#include "app/FleetPanel.hpp"

#include "mixr/models/player/Player.hpp"

#include "mixr/base/units/distance_utils.hpp"

#include "ftxui/dom/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;

// Dimensao FIXA do canvas, em "pixels" de braille (ver o comentario de
// ftxui/dom/canvas.hpp: multiplique por 2/4 para celulas de terminal --
// 240x120 pixels ocupam ~120x30 celulas, cabe numa tela cheia tipica sem
// depender do Box calculado em tempo de render, que so fica disponivel
// DEPOIS do layout). A largura deriva de 'kMapCanvasWidthCells' (header) --
// fonte unica, DashboardLoop.cpp usa a MESMA constante pra calcular a
// largura do card de detalhe.
const int kCanvasW{kMapCanvasWidthCells * 2};
const int kCanvasH{120};

const double kPi{3.14159265358979323846};
const double kDeg2Rad{kPi / 180.0};

// Espacamento (em pixel de canvas) entre linhas de grade/marcas de eixo --
// 40px = 20 celulas de terminal na horizontal, 10 na vertical.
const int kGridStepPx{40};

// Limiar minimo do eixo Y na vista Lateral (pedido explicito) -- a grade de
// altitude nao desce abaixo disto.
const double kMapAltitudeFloorFt{-1000.0};

std::string formatScale(const double metersPerCell)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(metersPerCell < 10.0 ? 1 : 0)
       << metersPerCell << " m/cel";
   return oss.str();
}

// So a vista de cima usa milhas nauticas (pedido explicito) -- a de lado
// ja usa pes no eixo Y (pedido da rodada anterior) e continua em metros no
// resto. Precisao adaptativa: zoom bem de perto (< 10 NM de passo) pede
// duas casas, senao vira ruido de digito sem sentido nenhum.
std::string formatScaleNm(const double metersPerCell)
{
   const double nmPerCell{metersPerCell * mixr::base::distance::M2NM};
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(nmPerCell < 10.0 ? 2 : (nmPerCell < 100.0 ? 1 : 0))
       << nmPerCell << " NM/cel";
   return oss.str();
}

struct Projected { int px{}; int py{}; bool onCanvas{}; };

// A MESMA rotacao serve pras duas projecoes -- ver o comentario de
// Perspective no header: o eixo livre (vertical) e compartilhado.
//
// TopDown: (rotE, rotN) sao N/E girados por 'viewYawDeg' -- px anda com
// rotE (largura da tela), py anda com -rotN (profundidade vira "pra cima").
// Lateral: px continua sendo rotE (largura -- distancia lateral vista de
// 'viewYawDeg'), py passa a ser a ALTITUDE relativa ao pan.
Projected project(const double northM, const double eastM, const double altitudeM,
                  const MapViewState& view)
{
   const double relN{northM - view.panNorthM};
   const double relE{eastM - view.panEastM};
   const double yaw{view.viewYawDeg * kDeg2Rad};

   const double rotE{relE * std::cos(yaw) - relN * std::sin(yaw)};
   const double rotN{relE * std::sin(yaw) + relN * std::cos(yaw)};

   const int cx{kCanvasW / 2};
   const int cy{kCanvasH / 2};

   Projected p;
   p.px = cx + static_cast<int>(std::lround(rotE / view.metersPerCell));
   if (view.perspective == Perspective::TopDown) {
      p.py = cy - static_cast<int>(std::lround(rotN / view.metersPerCell));
   } else {
      p.py = cy - static_cast<int>(std::lround((altitudeM - view.panAltM) / view.metersPerCell));
   }
   p.onCanvas = (p.px >= 0 && p.px < kCanvasW && p.py >= 0 && p.py < kCanvasH);
   return p;
}

// Direcao da LINHA de rumo, em espaco de tela (pixel do canvas, y pra
// baixo). TopDown: projeta o vetor de rumo (compasso) pela mesma rotacao
// das posicoes. Lateral: a componente horizontal e a mesma projecao lateral
// do rumo: a vertical usa o angulo de arfagem como aproximacao visual de
// subida/descida (nao e fisica rigorosa -- e só a pista de direcao na
// tela, o mesmo papel que a linha faz no TopDown).
std::pair<double, double> headingLineDir(const EntityState& e, const MapViewState& view)
{
   const double hdg{e.headingDeg * kDeg2Rad};
   const double yaw{view.viewYawDeg * kDeg2Rad};
   const double dirE{std::sin(hdg)};
   const double dirN{std::cos(hdg)};
   const double rotDirE{dirE * std::cos(yaw) - dirN * std::sin(yaw)};
   const double rotDirN{dirE * std::sin(yaw) + dirN * std::cos(yaw)};

   if (view.perspective == Perspective::TopDown) return {rotDirE, -rotDirN};

   const double pitch{e.pitchDeg * kDeg2Rad};
   return {rotDirE, -std::sin(pitch)};
}

// So "voa" quem nao e uma construcao estatica -- BUILDING e o unico
// majorType sem dinamica de movimento no bitmask nativo de Player. Todo
// resto (aviao, veiculo terrestre, navio, arma, nave, forma de vida,
// generico) ganha seta.
bool hasMovementDynamics(const EntityState& e)
{
   return (e.majorType & static_cast<unsigned int>(mixr::models::Player::BUILDING)) == 0;
}

// So a linha, sem ponta de seta -- pedido explicito: "ao inves de seta, use
// somente uma linha orientada com a direcao em curso".
void drawHeadingLine(Canvas& c, const int px, const int py, const double dx, const double dy,
                     const Color color)
{
   const double len{std::hypot(dx, dy)};
   if (len < 1e-6) return;
   const int shaftLen{7};
   const int tipX{px + static_cast<int>(std::lround(dx / len * shaftLen))};
   const int tipY{py + static_cast<int>(std::lround(dy / len * shaftLen))};
   c.DrawPointLine(px, py, tipX, tipY, color);
}

// O nome da entidade dentro de uma caixa com moldura, ligada ao ponto por
// uma linha -- pedido explicito, no lugar do texto solto de antes (que
// disputava espaco visual com o proprio ponto). A caixa fica pro
// nordeste do ponto, deslocada e recortada pra nao estourar o canvas; a
// linha sai do ponto ate o canto mais proximo da caixa.
void drawLabelCallout(Canvas& c, const int px, const int py, const std::string& label,
                      const Color color)
{
   const int textW{static_cast<int>(label.size()) * 2};
   const int boxW{textW + 4};
   const int boxH{8};

   int boxX{std::clamp(px + 5, 0, std::max(0, kCanvasW - boxW))};
   int boxY{std::clamp(py - boxH - 3, 0, std::max(0, kCanvasH - boxH))};

   c.DrawPointLine(px, py, boxX, boxY + boxH, color);

   c.DrawPointLine(boxX, boxY, boxX + boxW, boxY, color);
   c.DrawPointLine(boxX, boxY + boxH, boxX + boxW, boxY + boxH, color);
   c.DrawPointLine(boxX, boxY, boxX, boxY + boxH, color);
   c.DrawPointLine(boxX + boxW, boxY, boxX + boxW, boxY + boxH, color);

   c.DrawText(boxX + 2, boxY + 2, label, color);
}

}

void panMap(MapViewState& view, const double screenRightM, const double screenUpM)
{
   // Inverte a MESMA rotacao de project(): desloca o referencial do pan em
   // E/N de modo que o conteudo ja na tela pareca andar (screenRightM,
   // screenUpM) -- valido nas duas perspectivas, porque a horizontal da
   // tela e sempre rotE. So a vertical muda: no TopDown ela e rotN (giro
   // ao redor do eixo vertical), no Lateral e altitude -- e por isso que
   // so ali 'vertRotN' fica zero e 'panAltM' entra no lugar.
   const double yaw{view.viewYawDeg * kDeg2Rad};
   const double vertRotN{view.perspective == Perspective::TopDown ? screenUpM : 0.0};
   const double deltaE{screenRightM * std::cos(yaw) + vertRotN * std::sin(yaw)};
   const double deltaN{-screenRightM * std::sin(yaw) + vertRotN * std::cos(yaw)};
   view.panEastM += deltaE;
   view.panNorthM += deltaN;
   if (view.perspective == Perspective::Lateral) view.panAltM += screenUpM;
}

void zoomMap(MapViewState& view, const bool zoomIn)
{
   view.metersPerCell *= zoomIn ? (1.0 / kMapZoomStep) : kMapZoomStep;
   view.metersPerCell = std::clamp(view.metersPerCell, kMapMinMetersPerCell, kMapMaxMetersPerCell);
}

void rotateMap(MapViewState& view, const bool clockwise)
{
   view.viewYawDeg += clockwise ? kMapRotateStepDeg : -kMapRotateStepDeg;
   view.viewYawDeg = std::fmod(view.viewYawDeg + 360.0, 360.0);
}

void centerMapOn(MapViewState& view, const EntityState& e)
{
   view.panNorthM = e.northM;
   view.panEastM = e.eastM;
   view.panAltM = e.altitudeM;
}

void updateTrails(MapViewState& view, const std::vector<EntityState>& entities)
{
   for (const auto& e : entities) {
      auto& trail{view.trails[e.id]};
      trail.push_back({e.northM, e.eastM, e.altitudeM});
      while (trail.size() > kMapTrailLength) trail.pop_front();
   }
}

// Marca a linha/coluna de grade em 'coord' com uma cor tenue, e devolve o
// valor (metros, ja no referencial do PAN -- nao do mundo absoluto: girar
// ou mover a vista desloca a origem dos eixos junto, de proposito, pra
// sempre rotular exatamente o que aparece no meio da tela) que a etiqueta
// deveria mostrar. Ver o comentario de renderMap() sobre "x"/"y".
void drawDimLine(Canvas& c, const int x1, const int y1, const int x2, const int y2)
{
   c.DrawPointLine(x1, y1, x2, y2, [](Cell& cell) {
      cell.foreground_color = Color::GrayDark;
      cell.dim = true;
   });
}

std::string formatMeters(const double m)
{
   std::ostringstream oss;
   oss << std::showpos << std::fixed << std::setprecision(0) << m;
   return oss.str();
}

std::string formatMetersMagnitude(const double m)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(0) << m;
   return oss.str();
}

// Mesma logica de 'formatMeters'/'formatMetersMagnitude', em milhas
// nauticas -- so a vista de cima usa (ver o comentario de formatScaleNm()).
std::string formatNm(const double m)
{
   const double nm{m * mixr::base::distance::M2NM};
   std::ostringstream oss;
   oss << std::showpos << std::fixed << std::setprecision(std::fabs(nm) < 10.0 ? 2 : 1) << nm;
   return oss.str();
}

std::string formatNmMagnitude(const double m)
{
   const double nm{m * mixr::base::distance::M2NM};
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(nm < 10.0 ? 2 : 1) << nm;
   return oss.str();
}

Element renderMap(const std::vector<EntityState>& entities, const MapViewState& view,
                  const int focusedId, Box& outCanvasBox)
{
   auto c = Canvas(kCanvasW, kCanvasH);

   const int cx{kCanvasW / 2};
   const int cy{kCanvasH / 2};

   // Grade e eixos -- "x"/"y" SAO relativos ao referencial do PAN (o ponto
   // que esta no meio da tela agora), nao ao referencial absoluto da
   // simulacao: e o que continua fazendo sentido depois de arrastar/
   // centralizar/girar, sem ter que reconciliar rotulo com rotacao (ver
   // panMap() -- a horizontal da tela ja E' 'rotE' por construcao, entao o
   // valor em metros de cada coluna de grade sai direto de '(gx-cx)*mpc',
   // sem nenhuma trigonometria extra).
   if (view.perspective == Perspective::TopDown) {
      for (int gx = cx % kGridStepPx; gx < kCanvasW; gx += kGridStepPx) {
         drawDimLine(c, gx, 0, gx, kCanvasH - 1);
         const double xM{(gx - cx) * view.metersPerCell};
         c.DrawText(std::clamp(gx - 6, 0, kCanvasW - 14), kCanvasH - 4,
                    formatNm(xM), [](Cell& cell) { cell.foreground_color = Color::GrayDark; });
      }
      for (int gy = cy % kGridStepPx; gy < kCanvasH; gy += kGridStepPx) {
         drawDimLine(c, 0, gy, kCanvasW - 1, gy);
         const double yM{(cy - gy) * view.metersPerCell};
         c.DrawText(0, std::clamp(gy - 2, 0, kCanvasH - 4),
                    formatNm(yM), [](Cell& cell) { cell.foreground_color = Color::GrayDark; });
      }
      c.DrawText(kCanvasW - 20, 2, "x/y (NM)", Color::GrayDark);

      // Barra de escala explicita, alem das marcas de grade -- um segmento
      // de comprimento CONHECIDO (kGridStepPx, o mesmo passo da grade) com
      // tiques nas pontas e a distancia correspondente, em NM, escrita
      // embaixo.
      const int barY{kCanvasH - 10};
      c.DrawPointLine(4, barY, 4 + kGridStepPx, barY, Color::White);
      c.DrawPointLine(4, barY - 2, 4, barY + 2, Color::White);
      c.DrawPointLine(4 + kGridStepPx, barY - 2, 4 + kGridStepPx, barY + 2, Color::White);
   } else {
      // Lateral: eixo Y = altitude em PES (pedido explicito). 'panAltM' e
      // o referencial (o "zero" que fica no meio da tela); ver project().
      // Limiar minimo de -1000 ft (pedido explicito) -- abaixo disso a
      // grade simplesmente para de aparecer (nao ha nada de util pra
      // mostrar mais fundo que isso), e uma linha SOLIDA marca o piso.
      for (int gy = cy % kGridStepPx; gy < kCanvasH; gy += kGridStepPx) {
         const double altM{view.panAltM - (gy - cy) * view.metersPerCell};
         const double altFt{altM * mixr::base::distance::M2FT};
         if (altFt < kMapAltitudeFloorFt) continue;
         drawDimLine(c, 0, gy, kCanvasW - 1, gy);
         c.DrawText(0, std::clamp(gy - 2, 0, kCanvasH - 4),
                    formatMeters(altFt) + "ft", [](Cell& cell) { cell.foreground_color = Color::GrayDark; });
      }

      // A linha do piso em si, se estiver visivel na janela atual.
      const double floorAltM{kMapAltitudeFloorFt / mixr::base::distance::M2FT};
      const int floorGy{cy - static_cast<int>(std::lround((floorAltM - view.panAltM) / view.metersPerCell))};
      if (floorGy >= 0 && floorGy < kCanvasH) {
         c.DrawPointLine(0, floorGy, kCanvasW - 1, floorGy, Color::Red);
         c.DrawText(0, std::clamp(floorGy - 2, 0, kCanvasH - 4),
                    formatMetersMagnitude(kMapAltitudeFloorFt) + "ft (piso)", Color::Red);
      }

      c.DrawText(kCanvasW - 14, 2, "y: alt(ft)", Color::GrayDark);
   }

   // Referencia do pan -- cruz tenue, marca exatamente o "zero" dos eixos
   // acima (o ponto que esta preso ao centro da tela).
   if (cx >= 0 && cx < kCanvasW && cy >= 0 && cy < kCanvasH) {
      c.DrawPointLine(cx - 3, cy, cx + 3, cy, Color::GrayDark);
      c.DrawPointLine(cx, cy - 3, cx, cy + 3, Color::GrayDark);
   }

   if (view.showTrails) {
      for (const auto& e : entities) {
         const auto it{view.trails.find(e.id)};
         if (it == view.trails.end() || it->second.size() < 2) continue;
         const Color trailColor{sideColor(e.side)};
         const auto& pts{it->second};
         for (std::size_t i = 1; i < pts.size(); i++) {
            const Projected a{project(pts[i - 1][0], pts[i - 1][1], pts[i - 1][2], view)};
            const Projected b{project(pts[i][0], pts[i][1], pts[i][2], view)};
            if (!a.onCanvas && !b.onCanvas) continue;
            c.DrawPointLine(a.px, a.py, b.px, b.py, [trailColor](Cell& cell) {
               cell.foreground_color = trailColor;
               cell.dim = true;
            });
         }
      }
   }

   // Nome de cada entidade DEPOIS de todas as bolinhas/linhas de rumo --
   // desenhado por cima, senao uma caixa desenhada antes ficaria escondida
   // pela bolinha de uma entidade vizinha desenhada depois.
   std::vector<std::pair<Projected, const EntityState*>> onScreen;
   for (const auto& e : entities) {
      const Projected p{project(e.northM, e.eastM, e.altitudeM, view)};
      if (!p.onCanvas) continue;
      onScreen.emplace_back(p, &e);

      const Color col{sideColor(e.side)};
      if (e.id == focusedId) c.DrawPointCircle(p.px, p.py, 4, Color::White);

      // Linha orientada com o rumo em curso -- SEM ponta de seta (pedido
      // explicito). So quem tem dinamica de movimento ganha.
      if (hasMovementDynamics(e)) {
         const auto [dx, dy]{headingLineDir(e, view)};
         drawHeadingLine(c, p.px, p.py, dx, dy, col);
      }
      // A entidade em si e so uma bolinha -- pedido explicito, no lugar do
      // marcador anterior (que ja era um circulo preenchido, mas dividia
      // a atencao visual com o texto do nome solto ao lado).
      c.DrawPointCircleFilled(p.px, p.py, 2, col);
   }
   for (const auto& [p, ePtr] : onScreen) {
      drawLabelCallout(c, p.px, p.py, ePtr->name, sideColor(ePtr->side));
   }

   const bool topDown{view.perspective == Perspective::TopDown};
   c.DrawText(2, 2, (topDown ? formatScaleNm(view.metersPerCell) : formatScale(view.metersPerCell))
      + (topDown ? "  [cima]" : "  [lado]")
      + "  rumo=" + std::to_string(static_cast<int>(std::lround(view.viewYawDeg))) + "deg");
   if (topDown) {
      c.DrawText(4, kCanvasH - 6, formatNmMagnitude(kGridStepPx * view.metersPerCell) + "NM", Color::White);
   }

   return canvas(std::move(c)) | reflect(outCanvasBox) | flex | border;
}

int hitTestEntity(const std::vector<EntityState>& entities, const MapViewState& view,
                  const int clickCellX, const int clickCellY)
{
   int bestId{-1};
   int bestDist{-1};
   for (const auto& e : entities) {
      const Projected p{project(e.northM, e.eastM, e.altitudeM, view)};
      if (!p.onCanvas) continue;
      const int cellX{p.px / 2};
      const int cellY{p.py / 4};
      const int dist{std::abs(cellX - clickCellX) + std::abs(cellY - clickCellY)};
      if (dist > 1) continue;   // tolerancia de 1 celula de terminal
      if (bestDist < 0 || dist < bestDist) { bestDist = dist; bestId = e.id; }
   }
   return bestId;
}

} // namespace app
