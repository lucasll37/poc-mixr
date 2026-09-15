#include "app/DashboardWiring.hpp"

#include "app/MapPanel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstddef>

//------------------------------------------------------------------------------
// A aba "Mapa" (F2): navegavel (arrastar/setas move, zoom por [ ]/roda, girar
// por ,/., trocar perspectiva/rastro/terreno/seguir por botao ou tecla) +
// painel de detalhe lateral (o MESMO 'w.buildDetailPanel()' da aba Players --
// clicar numa entidade no mapa muda 'w.selectedEntityIndex', a mesma variavel
// que a aba Players usa, entao as duas abas sempre concordam sobre "quem
// esta selecionada"). Ver o cabecalho de app/DashboardLoop.cpp.
//
// PRECISA que buildFleetTab(w) ja tenha rodado -- reusa 'w.treeMenuMap' e
// 'w.buildDetailPanel', montados la'.
//
// O tratamento de tecla/mouse do mapa NAO fica num CatchEvent local aqui --
// ver o comentario grande sobre 'withKeys' em app/DashboardLoop.cpp:
// Container::Vertical so encaminha TECLADO para o filho FOCADO, entao um
// CatchEvent aninhado dentro de 'contentTab' ficaria refem de qual dos dois
// (toolbar/contentTab) esta focado no momento. A solucao robusta -- tratar
// tudo no CatchEvent MAIS EXTERNO -- e' o motivo de todas as acoes abaixo
// virarem campos 'std::function' em 'w': o CatchEvent (em DashboardLoop.cpp)
// as chama pelo nome, sem duplicar a logica.
//------------------------------------------------------------------------------
namespace app {

namespace {
using namespace ftxui;
} // namespace

Component buildMapTab(DashboardWiring& w)
{
   // ---- acoes do MAPA -- mesma regra: uma lambda, usada por tecla E por
   // botao (ver a barra de botoes da aba, mais abaixo, e o CatchEvent em
   // DashboardLoop.cpp) ----
   w.doMapZoomIn = [&w] { zoomMap(w.mapView, true); };
   w.doMapZoomOut = [&w] { zoomMap(w.mapView, false); };
   w.doMapRotateLeft = [&w] { rotateMap(w.mapView, false); };
   w.doMapRotateRight = [&w] { rotateMap(w.mapView, true); };
   w.doMapToggleTrails = [&w] { w.mapView.showTrails = !w.mapView.showTrails; };

   // Reancora o nivel do terreno perto do limite inferior da janela (ver
   // MapPanel.hpp::snapPanToGroundLevel) -- so tem efeito na perspectiva
   // Lateral com terreno ligado.
   w.doMapSnapGroundIfApplicable = [&w] {
      if (!w.mapView.showTerrain) return;
      // Com o "seguir" ligado o follow VENCE: os dois escrevem em 'panAltM'
      // e o pedido e a entidade centralizada nas duas perspectivas. Sem esta
      // guarda o snap escreveria panAltM aqui e o proximo redesenho o
      // sobrescreveria de qualquer forma -- um piscar sem nenhum efeito
      // util.
      if (w.mapView.followSelected) return;
      snapPanToGroundLevel(w.mapView, w.terrainSampler);
   };
   // "Seguir" a entidade selecionada -- o pan passa a ser recolocado sobre
   // ela a cada redesenho da aba (ver applyMapFollow(), chamada no Renderer
   // de 'mapCanvasArea'), nas duas perspectivas.
   w.doMapToggleFollow = [&w] { w.mapView.followSelected = !w.mapView.followSelected; };
   w.doMapToggleTerrain = [&w] {
      w.mapView.showTerrain = !w.mapView.showTerrain;
      w.doMapSnapGroundIfApplicable();
   };
   w.doMapTogglePerspective = [&w] {
      w.mapView.perspective = (w.mapView.perspective == Perspective::TopDown)
         ? Perspective::Lateral : Perspective::TopDown;
      w.doMapSnapGroundIfApplicable();
   };
   w.doMapCenterOnSelected = [&w] {
      if (w.displayedEntities.empty()) return;
      const std::size_t idx{static_cast<std::size_t>(
         std::clamp(w.selectedEntityIndex, 0, static_cast<int>(w.displayedEntities.size()) - 1))};
      centerMapOn(w.mapView, w.displayedEntities[idx]);
      w.doMapSnapGroundIfApplicable();
   };

   // 'w.treeMenuMap' entra como FILHO deste Renderer (nao Renderer solto) --
   // e o que faz o clique nele chegar de verdade.
   const Component mapCanvasArea{Renderer(w.treeMenuMap, [&w]() -> Element {
      int focusedId{-1};
      // Indice JA limitado a faixa valida -- o mesmo para o card de detalhe,
      // para o realce no canvas e para o "seguir", pra os tres nunca
      // discordarem sobre quem e a entidade selecionada.
      int focusedIndex{-1};
      Element detail{text("(clique numa entidade no mapa, ou selecione nos Players)")
                     | dim | center
                     | size(WIDTH, EQUAL, w.detailPanelWidth) | size(HEIGHT, EQUAL, kDetailPanelHeight)};
      if (!w.displayedEntities.empty()) {
         focusedIndex = std::clamp(w.selectedEntityIndex, 0,
                                   static_cast<int>(w.displayedEntities.size()) - 1);
         const std::size_t idx{static_cast<std::size_t>(focusedIndex)};
         focusedId = w.displayedEntities[idx].id;
         detail = w.buildDetailPanel(w.displayedEntities[idx], w.treeMenuMap);
      }

      // O canvas acompanha a area que o layout DE FATO reservou pro mapa, em
      // vez de um tamanho fixo. 'w.mapCanvasBox' e do quadro ANTERIOR.
      fitMapCanvasToBox(w.mapView, w.mapCanvasBox);

      // "Seguir" a entidade selecionada -- ANTES de renderMap(), pra ESTE
      // quadro ja sair centralizado. NAO se reancora o terreno perto do
      // fundo (Lateral) depois disto, de proposito: o snap disputaria o
      // mesmo 'panAltM' que o follow acabou de escrever.
      applyMapFollow(w.mapView, w.displayedEntities, focusedIndex);

      return hbox({
                renderMap(w.displayedEntities, w.mapView, focusedId, w.mapCanvasBox, w.terrainSampler)
                   | flex,
                separator(),
                detail,
             })
             | flex;
   })};

   const Component btnMapZoomOut{makeButton("[[] Zoom-", w.doMapZoomOut)};
   const Component btnMapZoomIn{makeButton("[]] Zoom+", w.doMapZoomIn)};
   const Component btnMapRotL{makeButton("[,] Girar<", w.doMapRotateLeft)};
   const Component btnMapRotR{makeButton("[.] Girar>", w.doMapRotateRight)};
   const Component btnMapCenter{makeButton("[c] Centralizar", w.doMapCenterOnSelected)};

   // Os quatro de alternancia (seguir/rastro/terreno/perspectiva) precisam
   // de um rotulo que MUDA com o estado (ON/OFF, Cima/Lado) -- 'transform'
   // roda a cada redesenho (nao so no clique), entao basta ler 'w.mapView'
   // direto nele.
   ButtonOption trailsOpt;
   trailsOpt.label = "[t] Rastro";
   trailsOpt.on_click = w.doMapToggleTrails;
   trailsOpt.transform = [&w](const EntryState&) {
      return text(std::string(" [t] Rastro: ") + (w.mapView.showTrails ? "ON" : "OFF") + " ")
         | (w.mapView.showTrails ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapTrails{Button(trailsOpt)};

   ButtonOption terrainOpt;
   terrainOpt.label = "[e] Terreno";
   terrainOpt.on_click = w.doMapToggleTerrain;
   terrainOpt.transform = [&w](const EntryState&) {
      return text(std::string(" [e] Terreno: ") + (w.mapView.showTerrain ? "ON" : "OFF") + " ")
         | (w.mapView.showTerrain ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapTerrain{Button(terrainOpt)};

   ButtonOption followOpt;
   followOpt.label = "[f] Seguir";
   followOpt.on_click = w.doMapToggleFollow;
   followOpt.transform = [&w](const EntryState&) {
      return text(std::string(" [f] Seguir: ") + (w.mapView.followSelected ? "ON" : "OFF") + " ")
         | (w.mapView.followSelected ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapFollow{Button(followOpt)};

   ButtonOption perspectiveOpt;
   perspectiveOpt.label = "[v] Vista";
   perspectiveOpt.on_click = w.doMapTogglePerspective;
   perspectiveOpt.transform = [&w](const EntryState&) {
      const bool lateral{w.mapView.perspective == Perspective::Lateral};
      return text(std::string(" [v] Vista: ") + (lateral ? "Lado" : "Cima") + " ")
         | (lateral ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapPerspective{Button(perspectiveOpt)};

   const Component mapButtons{Container::Horizontal({
      btnMapZoomOut, btnMapZoomIn, btnMapRotL, btnMapRotR,
      btnMapCenter, btnMapFollow, btnMapTrails, btnMapTerrain, btnMapPerspective,
   })};

   const Component mapBody{Container::Vertical({mapCanvasArea, mapButtons})};
   return Renderer(mapBody, [mapCanvasArea, mapButtons]() -> Element {
      return vbox({
         mapCanvasArea->Render() | flex,
         text("[setas/arraste] mover (desliga o seguir)  [clique] selecionar entidade") | dim,
         mapButtons->Render(),
      });
   });
}

} // namespace app
