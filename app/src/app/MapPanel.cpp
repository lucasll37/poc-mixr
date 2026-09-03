#include "app/MapPanel.hpp"
#include "app/FleetPanel.hpp"
#include "app/MapGeometry.hpp"

#include "mixr/models/player/Player.hpp"

#include "mixr/base/units/distance_utils.hpp"

#include "ftxui/dom/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace app {

namespace {
using namespace ftxui;
using namespace mapgeometry;   // kCanvasW/kCanvasH/kPi/kDeg2Rad/Projected/
                                // project()/worldFromCanvas*()/contourIntervalFor()
                                // -- ver app/MapGeometry.hpp

// Quanto do limite INFERIOR do canvas fica reservado, em pixel, quando
// 'snapPanToGroundLevel()' reancora o nivel do terreno perto de baixo --
// nao flush contra a borda: sobra espaco pra grade/rotulo de altitude
// (ex.: "-2133ft") continuarem legiveis abaixo da linha do chao.
const int kGroundMarginBottomPx{16};

// Espacamento (em pixel de canvas) entre linhas de grade/marcas de eixo --
// 40px = 20 celulas de terminal na horizontal, 10 na vertical.
const int kGridStepPx{40};


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

// Espacamento (em pixel de canvas) do GRID de amostragem de curvas de
// nivel (TopDown) -- ver o comentario grande de 'drawTerrain', abaixo. Fino
// o bastante (3px, perto da resolucao do proprio canvas de braille) pra
// dar linha continua a olho na deteccao de borda por faixa; ainda assim
// so ~3200 consultas por redesenho (kCanvasW/passo * kCanvasH/passo),
// desprezivel mesmo chamado ate ~10x/s.
const int kTerrainGridStepPx{3};

// project()/worldFromCanvasTopDown()/worldFromCanvasLateral()/
// contourIntervalFor() moraram aqui -- agora em app/MapGeometry.hpp/.cpp,
// testados isoladamente em tests/app/test_map_geometry.cpp (ver o
// "porque" no cabecalho daquele header).

// A vista de TERRENO em si -- desenhada ANTES de grade/entidades (fica por
// baixo). So chamada quando 'view.showTerrain' esta ligado E ha amostrador
// (ver app/TerrainQuery.hpp: um std::function vazio significa "sem banco
// de terreno carregado neste cenario" -- degrade em silencio, nao desenha
// nada, mesmo raciocinio do resto do app pra campo que nao se aplica).
//
// TopDown: CURVAS DE NIVEL SEM COR (pedido explicito: "linhas sem cor, mais
// finas e com espacamento maior") -- amostra um grid (passo
// 'kTerrainGridStepPx'), quantiza cada ponto pra uma "faixa" de elevacao
// (floor(elev/intervalo)) e marca um ponto onde a faixa do vizinho (direita
// ou abaixo) e DIFERENTE -- e a fronteira entre duas faixas, ou seja, uma
// curva de nivel. Deteccao de borda por comparacao de faixa e mais simples
// que 'marching squares' de verdade (nao interpola o cruzamento exato) mas
// no passo fino usado aqui (3px, a resolucao do proprio canvas de braille)
// o resultado ja sai como linha continua a olho. Um unico tom neutro
// (Color::GrayDark) em vez do gradiente por elevacao de uma passada
// anterior -- e um PONTO por vez (DrawPoint, 1px), nao mais um circulo
// preenchido (DrawPointCircleFilled) -- pra ficar fina de verdade.
//
// Lateral: LINHA DE CONTORNO EM VERMELHO (pedido explicito: "o nivel do
// terreno deve aparecer em vermelho... substituindo o threshold de
// 1000ft") -- uma amostra por coluna ao longo da linha de visada que passa
// pelo pan, ligadas em sequencia por DrawPointLine. E o perfil do chao
// como silhueta ABERTA (nao preenchida), sempre em Color::Red -- e o que
// agora marca "onde e o chao" na vista Lateral, no lugar do piso fixo de
// -1000 ft que existia antes de haver terreno de verdade pra mostrar (ver
// o comentario da secao Lateral em renderMap()).
void drawTerrain(Canvas& c, const MapViewState& view, const TerrainSampler& sample)
{
   if (!sample) return;

   if (view.perspective == Perspective::TopDown) {
      const int step{kTerrainGridStepPx};
      const int nx{kCanvasW / step + 2};
      const int ny{kCanvasH / step + 2};
      std::vector<double> elev(static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny), 0.0);
      std::vector<bool> valid(static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny), false);
      const auto at = [nx](const int ix, const int iy) {
         return static_cast<std::size_t>(iy) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(ix);
      };

      double minM{1e18};
      double maxM{-1e18};
      for (int iy = 0; iy < ny; iy++) {
         const int gy{iy * step};
         if (gy >= kCanvasH) continue;
         for (int ix = 0; ix < nx; ix++) {
            const int gx{ix * step};
            if (gx >= kCanvasW) continue;
            double northM{};
            double eastM{};
            worldFromCanvasTopDown(gx, gy, view, northM, eastM);
            double elevM{};
            if (!sample(northM, eastM, elevM)) continue;
            elev[at(ix, iy)] = elevM;
            valid[at(ix, iy)] = true;
            minM = std::min(minM, elevM);
            maxM = std::max(maxM, elevM);
         }
      }
      if (maxM < minM) return;   // nenhum ponto valido (fora do tile inteiro)

      const double interval{contourIntervalFor(minM, maxM)};
      for (int iy = 0; iy < ny; iy++) {
         const int gy{iy * step};
         if (gy >= kCanvasH) continue;
         for (int ix = 0; ix < nx; ix++) {
            const int gx{ix * step};
            if (gx >= kCanvasW || !valid[at(ix, iy)]) continue;
            const double e{elev[at(ix, iy)]};
            const int band{static_cast<int>(std::floor(e / interval))};

            bool onContour{false};
            if (ix + 1 < nx && (ix + 1) * step < kCanvasW && valid[at(ix + 1, iy)]) {
               if (static_cast<int>(std::floor(elev[at(ix + 1, iy)] / interval)) != band) onContour = true;
            }
            if (!onContour && iy + 1 < ny && (iy + 1) * step < kCanvasH && valid[at(ix, iy + 1)]) {
               if (static_cast<int>(std::floor(elev[at(ix, iy + 1)] / interval)) != band) onContour = true;
            }
            if (onContour) {
               c.DrawPoint(gx, gy, true, Color::GrayDark);
            }
         }
      }
   } else {
      std::vector<int> px;
      std::vector<int> py;
      const int cy{kCanvasH / 2};
      for (int x = 0; x < kCanvasW; x += 2) {
         double northM{};
         double eastM{};
         worldFromCanvasLateral(x, view, northM, eastM);
         double elevM{};
         if (!sample(northM, eastM, elevM)) continue;
         const int groundPy{cy - static_cast<int>(
            std::lround((elevM - view.panAltM) / view.metersPerCell))};
         px.push_back(x);
         py.push_back(std::clamp(groundPy, 0, kCanvasH - 1));
      }
      for (std::size_t i = 1; i < px.size(); i++) {
         c.DrawPointLine(px[i - 1], py[i - 1], px[i], py[i], Color::Red);
      }
   }
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

// O nome da entidade ligado ao ponto por uma linha -- SEM moldura ao redor
// do texto (pedido explicito: "remova o quadro ao redor do nome"; a moldura
// de uma passada anterior competia visualmente com o proprio mapa quando
// havia varias entidades perto umas das outras). O texto fica a nordeste do
// ponto, deslocado e recortado pra nao estourar o canvas; a linha sai do
// ponto ate o inicio do texto.
void drawLabelCallout(Canvas& c, const int px, const int py, const std::string& label,
                      const Color color)
{
   const int textW{static_cast<int>(label.size()) * 2};
   const int textX{std::clamp(px + 5, 0, std::max(0, kCanvasW - textW))};
   const int textY{std::clamp(py - 11, 0, std::max(0, kCanvasH - 4))};

   c.DrawPointLine(px, py, textX, textY + 4, color);
   c.DrawText(textX, textY, label, color);
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

void snapPanToGroundLevel(MapViewState& view, const TerrainSampler& terrainSampler)
{
   if (view.perspective != Perspective::Lateral) return;
   if (!terrainSampler) return;

   double groundElevM{};
   if (!terrainSampler(view.panNorthM, view.panEastM, groundElevM)) return;

   // Mesma equacao de project() pro ramo Lateral, resolvida pra panAltM
   // dado um 'py' ALVO (perto do fundo do canvas): py = cy - (elev -
   // panAltM)/mpc  =>  panAltM = elev - (cy - py) * mpc.
   const int cy{kCanvasH / 2};
   const int targetPy{kCanvasH - kGroundMarginBottomPx};
   view.panAltM = groundElevM - static_cast<double>(cy - targetPy) * view.metersPerCell;
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

// Mesma logica de 'formatMeters', em milhas nauticas -- so a vista de cima
// usa (ver o comentario de formatScaleNm()).
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
                  const int focusedId, Box& outCanvasBox, const TerrainSampler& terrainSampler)
{
   auto c = Canvas(kCanvasW, kCanvasH);

   // Terreno primeiro -- fica por BAIXO da grade/entidades (desenhadas
   // depois, por cima). So(t) faz algo se 'view.showTerrain' estiver ligado
   // e houver amostrador (ver drawTerrain()).
   if (view.showTerrain) drawTerrain(c, view, terrainSampler);

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
      // Lateral: eixo Y = altitude em PES. 'panAltM' e o referencial (o
      // "zero" que fica no meio da tela); ver project(). O piso fixo de
      // -1000 ft de uma passada anterior SAIU (pedido explicito) -- quem
      // marca "onde e o chao" agora e o proprio nivel do terreno, em
      // VERMELHO (ver drawTerrain(), mais abaixo: a linha de contorno na
      // perspectiva Lateral usa Color::Red em vez do gradiente por
      // elevacao). Sem terreno carregado no cenario a grade so nao tem
      // marca de chao nenhuma -- nao ha mais um piso arbitrario pra cair
      // de volta.
      for (int gy = cy % kGridStepPx; gy < kCanvasH; gy += kGridStepPx) {
         const double altM{view.panAltM - (gy - cy) * view.metersPerCell};
         const double altFt{altM * mixr::base::distance::M2FT};
         drawDimLine(c, 0, gy, kCanvasW - 1, gy);
         c.DrawText(0, std::clamp(gy - 2, 0, kCanvasH - 4),
                    formatMeters(altFt) + "ft", [](Cell& cell) { cell.foreground_color = Color::GrayDark; });
      }

      // "y: alt(ft)" tem 10 caracteres; Canvas::DrawText anda 2px POR
      // caractere (canvas.cpp: 'x += 2' por glifo) -- precisa de 20px de
      // largura. Com 'kCanvasW - 14' a string estourava 6px alem da borda
      // direita e saia cortada ("y: alt(", sem o "ft)") -- IsIn() descarta
      // em silencio os glifos fora do canvas. Corrigido com a mesma folga
      // (~4px) que "x/y (NM)" ja usa no TopDown.
      c.DrawText(kCanvasW - 24, 2, "y: alt(ft)", Color::GrayDark);
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
