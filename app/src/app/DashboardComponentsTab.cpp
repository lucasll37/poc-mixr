#include "app/DashboardWiring.hpp"

#include "app/ComponentTreePanel.hpp"

#include "xclock/ClockStation.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstddef>
#include <iomanip>
#include <sstream>

//------------------------------------------------------------------------------
// A aba "Componentes" (F6): a arvore de componentes REAL da Station em
// execucao, navegavel, mais a animacao de fluxo entre fases -- ver o
// cabecalho de app/ComponentTreePanel.hpp/app/ComponentFlowState.hpp para a
// PRIMEIRA/SEGUNDA metade da feature, e o de app/DashboardLoop.cpp para o
// desenho geral (CatchEvent unico).
//
// Todas as acoes desta aba viram campos 'std::function' em 'w' porque o
// CatchEvent global (em app/DashboardLoop.cpp, bloco 'activeTab == 5') as
// chama tambem -- pelo MESMO motivo ja documentado em
// app/DashboardMapTab.cpp: Container::Vertical so encaminha teclado ao filho
// FOCADO, entao tratar tecla/mouse desta aba num CatchEvent aninhado ficaria
// refem de qual filho de 'root' esta focado no momento.
//------------------------------------------------------------------------------
namespace app {

namespace {
using namespace ftxui;
} // namespace

Component buildComponentsTab(DashboardWiring& w)
{
   // ---- acoes da aba "Componentes" -- mesma regra de sempre: uma lambda,
   // usada por tecla E por botao ----
   w.doCompZoomIn = [&w] { zoomComponentTree(w.componentsView, true); };
   w.doCompZoomOut = [&w] { zoomComponentTree(w.componentsView, false); };
   w.doCompCenterOnSelected = [&w] {
      const int idx{findComponentNodeIndex(w.componentsLayout, w.componentsView.selectedKey)};
      if (idx < 0) return;
      centerComponentTreeOn(w.componentsView, w.componentsLayout.nodes[static_cast<std::size_t>(idx)]);
   };

   // ---- retrair/expandir (o galho selecionado, ou a arvore toda) ----
   w.doCompToggleCollapse = [&w] {
      const int idx{findComponentNodeIndex(w.componentsLayout, w.componentsView.selectedKey)};
      if (idx < 0) return;
      toggleComponentNodeCollapsed(w.componentsCollapsed,
                                   w.componentsLayout.nodes[static_cast<std::size_t>(idx)]);
   };
   w.doCompExpandAll = [&w] {
      w.componentsCollapsed.clear();
      w.componentsRefit = true;
   };
   w.doCompCollapseAll = [&w] {
      w.componentsCollapsed.clear();
      collapseAllComponentNodes(w.componentsRoot, w.componentsCollapsed);
      w.componentsRefit = true;
   };

   // Navegacao por teclado entre os nos -- e o que torna retrair/expandir
   // usavel sem mouse. Reenquadra SO quando o no escolhido saiu do canvas.
   w.doCompNavigate = [&w](const TreeNavigation dir) {
      if (!navigateComponentTree(w.componentsLayout, w.componentsView, dir, w.componentsCollapsed))
         return;
      const int idx{findComponentNodeIndex(w.componentsLayout, w.componentsView.selectedKey)};
      if (idx >= 0)
         ensureComponentNodeVisible(w.componentsView, w.componentsLayout.nodes[static_cast<std::size_t>(idx)]);
   };

   // ---- animacao de fluxo (SEGUNDA METADE, ver app/ComponentFlowState.hpp)
   // -- [Espaco] nesta aba pausa a SIMULACAO de verdade (o mesmo efeito do
   // botao/tecla globais de pausa, so que redefinido aqui contra 'w.clockStation'
   // -- a acao global em si fica local a DashboardLoop.cpp, ver o cabecalho
   // la' sobre o que NAO mora neste wiring). ----
   w.doCompTogglePlay = [&w] { if (w.clockStation != nullptr) w.clockStation->togglePaused(); };

   // [n] = UM Station::tcFrame(dt) de verdade. Pausa antes, se estiver
   // rodando. NAO executado aqui nem na 'simThread': so acumula um pedido em
   // ClockStation, que a PROPRIA thread de tempo critico drena (ver o
   // comentario grande de ClockStation::requestStep()).
   w.doCompStep = [&w] {
      if (w.clockStation == nullptr) return;
      if (!w.clockStation->isPaused()) w.clockStation->setPaused(true);
      w.clockStation->requestStep();
      advanceComponentFlowStep(w.componentsFlow);
   };
   w.doCompCycleSpeed = [&w] { cycleComponentFlowSpeed(w.componentsFlow); };

   const Component componentsCanvasArea{Renderer([&w]() -> Element {
      // MESMO tamanho do card de detalhe das abas F1/F2 (pedido explicito).
      Element detail{text("(clique num no da arvore, ou navegue com as setas)")
                     | dim | center
                     | size(WIDTH, EQUAL, w.detailPanelWidth) | size(HEIGHT, EQUAL, kDetailPanelHeight)};
      const int selected{findComponentNodeIndex(w.componentsLayout, w.componentsView.selectedKey)};
      if (selected >= 0) {
         detail = renderComponentDetail(w.componentsLayout.nodes[static_cast<std::size_t>(selected)],
                                        w.componentsFlow, w.frameCallParams)
                  | size(WIDTH, EQUAL, w.detailPanelWidth) | size(HEIGHT, EQUAL, kDetailPanelHeight);
      }
      // Mesma tecnica de fitMapCanvasToBox() -- 'w.componentsCanvasBox' e a
      // caixa do quadro ANTERIOR.
      fitComponentTreeCanvasToBox(w.componentsView, w.componentsCanvasBox);

      // Uma vez so, assim que ha arvore E canvas de tamanho de verdade --
      // sem isto, o pan/zoom DEFAULT deixa quase toda a arvore fora do
      // canvas. Depois desta primeira vez, pan/zoom manual do usuario nao e
      // mais sobrescrito.
      if ((!w.componentsAutoFitted || w.componentsRefit) && !w.componentsLayout.nodes.empty()) {
         fitComponentTreeToContent(w.componentsView, w.componentsLayout);
         w.componentsAutoFitted = true;
         w.componentsRefit = false;
      }
      return hbox({
                renderComponentTree(w.componentsLayout, w.componentsView, w.componentsCanvasBox,
                                    w.componentsFlow) | flex,
                separator(),
                detail,
             })
             | flex;
   })};

   const Component btnCompZoomOut{makeButton("[[] Zoom-", w.doCompZoomOut)};
   const Component btnCompZoomIn{makeButton("[]] Zoom+", w.doCompZoomIn)};
   const Component btnCompCenter{makeButton("[c] Centralizar", w.doCompCenterOnSelected)};

   // Play/pause com rotulo dinamico -- o rotulo diz SIMULACAO de proposito:
   // e a simulacao que para, nao um relogio de animacao a parte.
   ButtonOption compPlayOpt;
   compPlayOpt.on_click = w.doCompTogglePlay;
   compPlayOpt.transform = [&w](const EntryState&) {
      const bool running{!w.frameCallParams.paused};
      return text(std::string(" [Espaco] ") + (running ? "Pausar" : "Rodar") + " ")
         | (running ? (bgcolor(Color::Green) | color(Color::Black) | bold)
                     : (bgcolor(Color::Yellow) | color(Color::Black) | bold));
   };
   const Component btnCompPlay{Button(compPlayOpt)};

   // O rotulo carrega o dt de verdade -- e a resposta curta pra "quanto vale
   // um passo", sem ter de ler o painel inteiro.
   ButtonOption compStepOpt;
   compStepOpt.on_click = w.doCompStep;
   compStepOpt.transform = [&w](const EntryState&) {
      std::ostringstream os;
      os << " [n] Passo " << std::fixed << std::setprecision(4)
         << frameStepSeconds(w.frameCallParams) << "s ";
      return text(os.str()) | bgcolor(Color::Blue) | color(Color::White) | bold;
   };
   const Component btnCompStep{Button(compStepOpt)};

   // Retrair/expandir -- o rotulo do primeiro muda com o estado do no
   // selecionado.
   ButtonOption compToggleOpt;
   compToggleOpt.on_click = w.doCompToggleCollapse;
   compToggleOpt.transform = [&w](const EntryState&) {
      const int idx{findComponentNodeIndex(w.componentsLayout, w.componentsView.selectedKey)};
      const bool canToggle{idx >= 0
         && w.componentsLayout.nodes[static_cast<std::size_t>(idx)].childCount > 0};
      const bool isCollapsed{canToggle
         && w.componentsLayout.nodes[static_cast<std::size_t>(idx)].collapsed};
      if (!canToggle) return text(" [Enter] Retrair ") | dim;
      return text(std::string(" [Enter] ") + (isCollapsed ? "Expandir" : "Retrair") + " ")
         | bgcolor(Color::Blue) | bold;
   };
   const Component btnCompToggle{Button(compToggleOpt)};

   const Component btnCompExpandAll{makeButton("[o] Abrir tudo", w.doCompExpandAll)};
   const Component btnCompCollapseAll{makeButton("[f] Fechar tudo", w.doCompCollapseAll)};

   ButtonOption compSpeedOpt;
   compSpeedOpt.on_click = w.doCompCycleSpeed;
   compSpeedOpt.transform = [&w](const EntryState&) {
      return text(" [v] " + std::to_string(w.componentsFlow.stepsPerSecond) + "x/s ")
         | bgcolor(Color::Blue) | bold;
   };
   const Component btnCompSpeed{Button(compSpeedOpt)};

   const Component componentsButtons{Container::Horizontal(
      {btnCompPlay, btnCompStep, btnCompSpeed,
       btnCompToggle, btnCompExpandAll, btnCompCollapseAll,
       btnCompZoomOut, btnCompZoomIn, btnCompCenter})};

   const Component componentsBody{Container::Vertical({componentsCanvasArea, componentsButtons})};
   return Renderer(componentsBody, [&w, componentsCanvasArea, componentsButtons]() -> Element {
      // MESMA altura da versao anterior a esta feature: uma linha em cima
      // (a faixa de fases), o canvas com todo o resto, e o rodape de
      // sempre.
      return vbox({
         renderFramePhaseStrip(w.componentsFlow, w.frameCallParams),
         componentsCanvasArea->Render() | flex,
         separator(),
         renderComponentFlowStatus(w.componentsFlow, w.frameCallParams),
         renderComponentFlowLegend(),
         componentsButtons->Render(),
      });
   });
}

} // namespace app
