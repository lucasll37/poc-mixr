#pragma once

#include "app/BehaviorTreeView.hpp"
#include "app/BreakpointController.hpp"
#include "app/ComponentFlowState.hpp"
#include "app/ComponentTreePanel.hpp"
#include "app/ComponentTreeQuery.hpp"
#include "app/DashboardLoop.hpp"
#include "app/DashboardState.hpp"
#include "app/FleetPanel.hpp"
#include "app/FrameCallChain.hpp"
#include "app/MapPanel.hpp"
#include "app/MetaObjectSnapshot.hpp"
#include "app/SpeedLadder.hpp"
#include "app/TerrainQuery.hpp"

#include "xlog/Log.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/box.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace mixr {
namespace simulation { class Station; }
namespace models { class WorldModel; }
namespace xclock { class ClockStation; }
namespace xtacview { class TacviewOutput; }
}

namespace mixr { namespace linkage { class IoHandler; } }

namespace app {

// Fabrica de Button comum a TODAS as barras deste dashboard (a principal, em
// DashboardLoop.cpp, e a de cada aba) -- nao capturava nada do escopo de
// runDashboard() mesmo quando era uma lambda local ali, entao virou funcao
// livre de verdade ao se espalhar por sete arquivos.
inline ftxui::Component makeButton(const std::string& label, const std::function<void()>& onClick)
{
   ftxui::ButtonOption opt{ftxui::ButtonOption::Ascii()};
   opt.label = label;
   opt.on_click = onClick;
   return ftxui::Button(opt);
}

//------------------------------------------------------------------------------
// O estado que ANTES vivia todo local a app::runDashboard() (~60 variaveis +
// dezenas de lambdas), agregado por VALOR num struct so -- ver a Fase 7 do
// plano de melhoria deste repositorio (2026-09).
//
// PORQUE POR VALOR, NAO POR REFERENCIA: 'DashboardWiring w;' e' declarado
// como variavel LOCAL de runDashboard() (mesma pratica das ~60 variaveis que
// ela substitui), entao vive exatamente pelo tempo que runDashboard() vive --
// o MESMO tempo de vida que todo Component/lambda construido a partir dela
// precisa. Guardar aqui os campos por VALOR (em vez de ponteiro/referencia
// pra uma variavel em OUTRO escopo) elimina o risco de referencia pendurada:
// nao ha "outro escopo" -- 'w' E' o escopo.
//
// Cada app/Dashboard<Aba>Tab.cpp e' dono de uma fatia deste struct (os campos
// comentados por secao abaixo correspondem 1:1 as sete abas) e de uma funcao
// 'buildXTab(DashboardWiring&)' que monta e devolve o ftxui::Component
// daquela aba, mais as acoes (lambdas) daquela aba que precisam ser chamadas
// de FORA do proprio arquivo -- pelo CatchEvent unico em DashboardLoop.cpp
// (ver o "porque" desse CatchEvent ser um so' no cabecalho de
// DashboardLoop.cpp) OU por outra aba (Mapa/Frota compartilham o card de
// detalhe + a arvore de BT + o breakpoint). Essas acoes viram campos
// 'std::function' aqui, atribuidos DENTRO do respectivo buildXTab() antes de
// devolver o Component -- por isso a ORDEM de chamada em runDashboard()
// importa (ver o comentario de ordem em cada bloco abaixo).
//
// O que NAO mora aqui, de proposito -- fica local a DashboardLoop.cpp porque
// nenhuma aba precisa (so o proprio laco/CatchEvent/toolbar/dialogo de
// confirmacao): 'stateMutex'/'latest'/'running'/'wantComponentTree' (a ponte
// com 'simThread'), 'uiDepth'/'pendingAction'/'doRestart'/'doQuit'/
// 'cancelAnyDrag'/'gotoTab' (o dialogo de confirmacao e a troca de aba), e as
// acoes globais de velocidade ('doAccelerate'/'doDecelerate'/'doTogglePause'/
// 'doRealTime') -- usadas so pela barra de ferramentas e pelo CatchEvent,
// nunca por dentro de uma aba especifica.
//------------------------------------------------------------------------------
struct DashboardWiring
{
   //---- parametros de runDashboard(), repassados como vieram --------------
   mixr::simulation::Station* station{};
   mixr::models::WorldModel* worldModel{};
   mixr::xclock::ClockStation* clockStation{};
   mixr::xtacview::TacviewOutput* tacviewOutput{};
   mixr::linkage::IoHandler* ioHandler{};
   int numTcThreads{};
   std::string scenarioLabel;
   std::string generatedEdlPath;

   // Ponteiro pra 'screen' (ftxui::ScreenInteractive), que continua vivendo
   // em runDashboard() -- so' 'doEdlRun' (aba EDL) precisa dele fora dali,
   // pra sair do Loop() no mesmo caminho de 'r'/'q'.
   ftxui::ScreenInteractive* screen{};

   // O que main.cpp le no final -- 'r'/'q' (locais a DashboardLoop.cpp)
   // escrevem aqui direto; 'doEdlRun' (aba EDL) tambem escreve, por isso o
   // campo mora no wiring compartilhado, nao local ao CatchEvent.
   DashboardExit action{DashboardExit::Quit};

   //---- compartilhado entre Frota/Mapa/breakpoint/CatchEvent ---------------
   // Preenchido por buildFleetTab() -- ver a ORDEM DE CHAMADA obrigatoria no
   // comentario de runDashboard(): Fleet ANTES de Mapa, porque Mapa consome
   // 'treeMenuMap'/'buildDetailPanel' que so' existem depois desta chamada.
   int activeTab{};
   int selectedEntityIndex{};
   std::vector<EntityState> displayedEntities;
   std::vector<std::string> entityLabels;

   std::mutex bpMutex;
   BreakpointController bp;
   std::atomic<bool> fastRunToBreakpoint{false};
   SpeedLadder ladder;

   std::vector<BtTreeLine> treeLines;   // = flattenBehaviorTree(behaviorTree), atribuido uma vez
   std::vector<std::string> treeLineLabels;
   int selectedBtLineIndex{};

   // Botoes/menus/lambdas montados por buildFleetTab() e reusados por
   // buildMapTab() -- ver o comentario grande acima.
   ftxui::Component treeMenuFleet;
   ftxui::Component treeMenuMap;
   ftxui::Component btnRunToBreakpoint;
   ftxui::Component btnRunToBreakpointMax;
   ftxui::Component btnCancelBreakpoint;

   std::function<void(bool fast)> doArmBreakpoint;
   std::function<void()> doCancelBreakpoint;
   std::function<ftxui::Element(const EntityState&, const ftxui::Component&)> buildDetailPanel;
   std::function<ftxui::Element()> buildBreakpointStatus;

   // Copia "pra desenho" de DashboardState::breakpoint* -- ver o comentario
   // de onde estes campos nascem em DashboardLoop.cpp (o Renderer mais
   // externo, 'withRenderer').
   bool displayedBreakpointArmed{};
   bool displayedBreakpointHit{};
   std::string displayedBreakpointHitMessage;

   // Recalculada a cada redesenho por 'withRenderer' (o terminal pode ser
   // redimensionado em qualquer frame) -- lida pelas tres abas com card de
   // detalhe (Frota/Mapa/Componentes).
   int detailPanelWidth{kDetailPanelMinWidth};

   //---- aba "Mapa" (F2) -----------------------------------------------------
   TerrainSampler terrainSampler;
   MapViewState mapView;
   ftxui::Box mapCanvasBox;

   std::function<void()> doMapZoomIn;
   std::function<void()> doMapZoomOut;
   std::function<void()> doMapRotateLeft;
   std::function<void()> doMapRotateRight;
   std::function<void()> doMapToggleTrails;
   std::function<void()> doMapToggleTerrain;
   std::function<void()> doMapTogglePerspective;
   std::function<void()> doMapToggleFollow;
   std::function<void()> doMapCenterOnSelected;
   std::function<void()> doMapSnapGroundIfApplicable;

   //---- aba "Memoria" (F3) ---------------------------------------------------
   int selectedClassIndex{};
   std::vector<ClassStat> displayedClasses;
   std::vector<std::string> classLabels;

   //---- aba "Tempo Nao-Critico" (F4) -----------------------------------------
   BackgroundInfo displayedBackground;

   //---- aba "Log" (F5) --------------------------------------------------------
   int selectedLogIndex{};
   bool logFollowTail{true};
   mixr::xlog::Level logMinLevel{mixr::xlog::Level::DEBUG};
   mixr::xlog::Level lastLogFilter{mixr::xlog::Level::DEBUG};
   std::uint64_t lastLogSeq{};
   std::vector<mixr::xlog::Entry> displayedLogs;
   std::vector<std::string> logLabels;

   std::function<void()> doCycleLogFilter;
   std::function<void()> doToggleLogFollow;

   //---- aba "Componentes" (F6) -------------------------------------------------
   ComponentTreeViewState componentsView;
   ComponentTreeNode componentsRoot;
   ComponentTreeLayout componentsLayout;
   CollapsedNodes componentsCollapsed;
   ftxui::Box componentsCanvasBox;
   bool componentsAutoFitted{};
   bool componentsRefit{};
   ComponentFlowState componentsFlow;
   FrameCallParams frameCallParams;

   std::function<void()> doCompZoomIn;
   std::function<void()> doCompZoomOut;
   std::function<void()> doCompCenterOnSelected;
   std::function<void()> doCompToggleCollapse;
   std::function<void()> doCompExpandAll;
   std::function<void()> doCompCollapseAll;
   std::function<void(TreeNavigation)> doCompNavigate;
   std::function<void()> doCompTogglePlay;
   std::function<void()> doCompStep;
   std::function<void()> doCompCycleSpeed;

   //---- aba "EDL" (F7) ----------------------------------------------------------
   std::string originalEdlText;
   std::string editedEdlText;
   bool edlHasStatus{};
   bool edlStatusOk{};
   std::string edlStatusMessage;
   int edlCursorPos{};
   ftxui::Component edlInput;   // guardado aqui so' porque 'appRoot' (DashboardLoop.cpp)
                                // precisa chamar edlInput->TakeFocus() -- ver o comentario la'.

   std::function<void()> doEdlRevert;
   std::function<void()> doEdlValidate;
   std::function<void()> doEdlRun;
};

//------------------------------------------------------------------------------
// Uma funcao por aba -- monta o ftxui::Component daquela aba (a lista/canvas
// +botoes que 'app/DashboardLoop.cpp' encaixa dentro do Container::Tab
// 'contentTab') e, quando a aba tem acao usada por FORA dela (CatchEvent
// global, ou outra aba), atribui os campos 'std::function'/Component
// correspondentes em 'w' antes de devolver.
//
// ORDEM DE CHAMADA OBRIGATORIA em runDashboard(): buildFleetTab(w) tem que
// rodar ANTES de buildMapTab(w) -- Mapa reusa 'w.treeMenuMap'/
// 'w.buildDetailPanel'/'w.doArmBreakpoint'/etc, que so' existem depois que
// Frota os monta (ver o comentario grande no topo de DashboardFleetTab.cpp).
// As demais cinco nao tem dependencia de ordem entre si.
//------------------------------------------------------------------------------
ftxui::Component buildFleetTab(DashboardWiring& w);
ftxui::Component buildMapTab(DashboardWiring& w);
ftxui::Component buildMemoryTab(DashboardWiring& w);
ftxui::Component buildBackgroundTab(DashboardWiring& w);
ftxui::Component buildLogTab(DashboardWiring& w);
ftxui::Component buildComponentsTab(DashboardWiring& w);
ftxui::Component buildEdlTab(DashboardWiring& w);

} // namespace app
