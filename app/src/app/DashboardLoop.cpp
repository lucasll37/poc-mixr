#include "app/DashboardLoop.hpp"

#include "app/BehaviorTreeView.hpp"
#include "app/DashboardState.hpp"
#include "app/Fleet.hpp"
#include "app/FleetPanel.hpp"
#include "app/MapPanel.hpp"
#include "app/MemoryPanel.hpp"

#include "xboard/Board.hpp"
#include "xclock/ClockStation.hpp"
#include "xtacview/TacviewOutput.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/simulation/Station.hpp"

#include "mixr/base/util/system_utils.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

//------------------------------------------------------------------------------
// O desenho vive so aqui (mais app/FleetPanel.cpp, app/MapPanel.cpp,
// app/MemoryPanel.cpp -- uma aba cada, "um arquivo uma questao") --
// app/DashboardState.hpp so carrega numeros, sem nenhum tipo do FTXUI, para
// poder ser testado/mexido sem levantar tela nenhuma.
//
// POR QUE NAO shared/xclock::TimeControls/ConsoleKeyboard: os dois mexem em
// termios (modo bruto do terminal) por fora do FTXUI, que ja e dono do
// terminal assim que ScreenInteractive::Fullscreen() comeca. As acoes de
// controle de tempo aqui chamam ClockStation::setTimeScale()/
// togglePaused()/setPaused() DIRETO -- a mesma API que TimeControls::apply()
// ja usa por baixo.
//
// CADA ACAO (acelerar/frear/pausar/trocar de aba/carregar/reiniciar/parar/
// sair) e uma UNICA lambda nomeada, chamada tanto pelo atalho de teclado
// quanto pelo Button correspondente -- e o que da "elemento clicavel com
// dica de atalho" sem duplicar logica (ver a barra de botoes no fim desta
// funcao).
//
// TRES ABAS (ftxui::Container::Tab), cada uma um ftxui::Menu dentro de
// frame()/vscroll_indicator() (padrao oficial do FTXUI para lista rolavel
// -- ver o exemplo menu_in_frame.cpp da propria lib): e o que deixa a UI
// caber QUALQUER quantidade de entidades/classes sem crescer a tela.
//
// DUAS THREADS, no molde do que app/RealTimeRun.cpp (das outras pocs) ja faz
// sozinho: a de SIMULACAO avanca a 10 Hz independente de quando o terminal
// manda evento -- station->updateData(dt), a varredura de radar pro
// Tacview, e por cima disso capturar um DashboardState sob mutex e pedir um
// redesenho com screen.PostEvent(Event::Custom). A thread PRINCIPAL so roda
// o Loop() do FTXUI (desenho + teclado + mouse).
//------------------------------------------------------------------------------

namespace app {

namespace {
using namespace ftxui;

const int bgRate{10};
const int settleMs{1000};

// As quatro acoes disruptivas (mudam de cenario ou encerram o processo) --
// pedido explicito: confirmar antes de executar. Ver 'uiDepth'/'pendingAction'
// em runDashboard().
enum class PendingAction { None, Load, Restart, Stop, Quit };

std::string pendingActionLabel(const PendingAction a)
{
   switch (a) {
      case PendingAction::Load:    return "carregar outro cenario";
      case PendingAction::Restart: return "reiniciar o cenario atual";
      case PendingAction::Stop:    return "parar e voltar a selecao de cenario";
      case PendingAction::Quit:    return "sair do dashboard";
      default:                    return "";
   }
}

//------------------------------------------------------------------------------
// Breakpoint de arvore de comportamento -- "marcar um estado da bt de um
// dado elemento e rodar a simulacao ate que aquele no seja atingido,
// devolvendo a simulacao pausada". Escrito pela thread de SIMULACAO (que
// checa a condicao a cada amostra -- ver o laco de 'simThread' em
// runDashboard()) e pela thread de UI (que arma/cancela); protegido por
// 'bpMutex' (mesmo padrao de 'stateMutex'/'latest').
//
// O alvo e (id do PLAYER, tag do no) -- nao "qualquer player nesse estado":
// o pedido foi "de um dado elemento", entao o breakpoint observa uma
// entidade especifica, capturada no momento de armar (ver 'doArmBreakpoint'
// em runDashboard()).
//------------------------------------------------------------------------------
struct Breakpoint
{
   bool armed{};
   int entityId{-1};
   std::string entityName;
   std::string nodeTag;
   bool fastMode{};        // "velocidade maxima possivel" -- simThread pula o msleep()
   double armedAtSimSec{};

   // Resultado da ultima tentativa -- pra UI mostrar o que aconteceu depois
   // que 'armed' volta a false, sem precisar de um canal separado.
   bool hit{};
   double hitSimSec{};
   bool timedOut{};
};

// "Trate os casos em que um no objetivo nunca e atingido": alem do cancelamento
// manual (sempre disponivel), um breakpoint que nunca dispara e abandonado
// sozinho depois desse tanto de tempo SIMULADO (nao de parede -- independe
// de estar em velocidade maxima ou 1x) desde que foi armado.
const double kBreakpointTimeoutSimSec{300.0};

// Mesma escada de shared/xclock/TimeControls.cpp, de proposito.
const double kLadder[]{0.1, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0};
const int kLadderSize{static_cast<int>(sizeof(kLadder) / sizeof(kLadder[0]))};
const int kRealTimeIndex{3};

int nearestLadderIndex(const double scale)
{
   int best{kRealTimeIndex};
   double bestDist{-1.0};
   for (int i = 0; i < kLadderSize; i++) {
      const double dist{std::fabs(kLadder[i] - scale)};
      if (bestDist < 0.0 || dist < bestDist) { bestDist = dist; best = i; }
   }
   return best;
}

std::string formatScale(const double scale)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(scale < 1.0 ? 2 : 0) << scale << "x";
   return oss.str();
}

Element renderHeader(const DashboardState& st)
{
   std::ostringstream tw;
   tw << "t=" << std::fixed << std::setprecision(0) << st.wallSec << "s";
   std::ostringstream ts;
   ts << "sim=" << std::fixed << std::setprecision(1) << st.simSec << "s";

   const std::string speedLabel{st.paused ? ("PAUSADO (" + formatScale(st.timeScale) + ")")
                                          : formatScale(st.timeScale)};
   const Color speedColor{st.paused ? Color::Red
                          : (st.timeScale > 1.0 ? Color::Yellow
                            : (st.timeScale < 1.0 ? Color::Cyan : Color::Green))};

   return hbox({
             text(" dashboard ") | bold | bgcolor(Color::Blue) | color(Color::White),
             text(" " + st.scenarioLabel + " ") | bold,
             text(" entidades=" + std::to_string(st.entities.size()) + " ") | dim,
             filler(),
             text(tw.str() + "  " + ts.str() + "  "),
             text(" " + speedLabel + " ") | bold | bgcolor(speedColor) | color(Color::Black),
             text("  thr=" + std::to_string(st.numTcThreads) + " "),
          })
          | border;
}

// Uma linha da arvore de BT (ja achatada por app::flattenBehaviorTree) --
// 'isActiveLeaf' e a folha vencedora AGORA (o mesmo destaque que existia
// antes, quando a arvore era so um Element estatico); 'isSelected' e a
// escolha do USUARIO na lista (pra virar alvo de breakpoint -- ver
// 'doArmBreakpoint' em runDashboard()); 'isBreakpoint' marca o no que ESTA
// armado.
Element renderBtLine(const BtTreeLine& line, const bool isActiveLeaf, const bool isSelected,
                     const bool isBreakpoint)
{
   Element e{text(line.display)};
   if (!line.leaf) e = e | dim;
   if (isActiveLeaf) e = e | bgcolor(Color::Blue) | color(Color::White) | bold;
   if (isBreakpoint) e = hbox({e, text(" [BP]") | color(Color::Red) | bold});
   if (isSelected) e = e | inverted;
   return e;
}

}

DashboardExit runDashboard(mixr::simulation::Station* const station,
                           mixr::models::WorldModel* const worldModel,
                           mixr::xclock::ClockStation* const clockStation,
                           mixr::xtacview::TacviewOutput* const tacviewOutput,
                           const int numTcThreads, const std::string& scenarioLabel,
                           const BtNode& behaviorTree)
{
   std::mutex stateMutex;
   DashboardState latest;
   std::atomic<bool> running{true};

   // Estado do breakpoint (ver o comentario da struct Breakpoint, acima) --
   // 'fastRunToBreakpoint' e atomico A PARTE (lido a cada iteracao do laco
   // de 'simThread', sem tomar 'bpMutex' so pra isso) porque decide se a
   // iteracao PULA o msleep() de pacing -- caminho quente, sem alocacao.
   std::mutex bpMutex;
   Breakpoint bp;
   std::atomic<bool> fastRunToBreakpoint{false};

   auto screen = ScreenInteractive::Fullscreen();

   std::thread simThread([&] {
      station->createTimeCriticalProcess();
      mixr::base::msleep(settleMs);

      const double dt{1.0 / static_cast<double>(bgRate)};
      double wallTimeElapsed{};
      double startTime{mixr::base::getComputerTime()};
      long frameCount{};
      std::vector<ClassStat> classHistory;
      bool wasFast{};

      while (running.load()) {
         station->updateData(dt);

         if (tacviewOutput != nullptr) {
            const double simTime{worldModel->getExecTimeSec()};
            for (auto* const player : discoverPlayers(worldModel)) {
               const mixr::xboard::Readout board{mixr::xboard::get(player->getID())};
               if (board.radarValid) {
                  tacviewOutput->updateRadarScan(static_cast<std::uint32_t>(player->getID()), simTime,
                     board.radarAzDeg, board.radarElDeg, board.radarRangeM,
                     board.radarHBeamDeg, board.radarVBeamDeg);
               }
            }
         }

         frameCount += 1;
         DashboardState next{captureState(worldModel, static_cast<double>(frameCount) * dt,
                                          worldModel->getExecTimeSec(), clockStation,
                                          numTcThreads, scenarioLabel, classHistory)};
         classHistory = next.classStats;

         // Checagem do breakpoint -- ver o comentario da struct Breakpoint.
         // Roda toda amostra, mesmo fora do modo rapido: "a velocidade que
         // eu decidir" tambem tem de parar sozinha quando o no e atingido.
         {
            const std::lock_guard<std::mutex> lock(bpMutex);
            if (bp.armed) {
               for (const auto& e : next.entities) {
                  if (e.id == bp.entityId && matchesLabel(bp.nodeTag, e.behaviorLabel)) {
                     if (clockStation != nullptr) clockStation->setPaused(true);
                     bp.armed = false;
                     bp.hit = true;
                     bp.hitSimSec = next.simSec;
                     fastRunToBreakpoint = false;
                     break;
                  }
               }
               if (bp.armed && (next.simSec - bp.armedAtSimSec) > kBreakpointTimeoutSimSec) {
                  bp.armed = false;
                  bp.timedOut = true;
                  fastRunToBreakpoint = false;
               }
            }
         }

         {
            const std::lock_guard<std::mutex> lock(stateMutex);
            latest = next;
         }
         screen.PostEvent(Event::Custom);

         // Modo rapido: pula o msleep() de pacing, deixa o laco girar o
         // mais rapido que a CPU permitir. Ao SAIR do modo (breakpoint
         // atingido, cancelado ou nunca ligado), resincroniza a referencia
         // de parede -- senao 'wallTimeElapsed' fica adiantado (ele so
         // cresce, mesmo sem dormir) e o pacing tentaria "recuperar o
         // atraso" dormindo um tempao de uma vez so, travando a tela.
         const bool fastNow{fastRunToBreakpoint.load()};
         if (wasFast && !fastNow) {
            wallTimeElapsed = 0.0;
            startTime = mixr::base::getComputerTime();
         }
         wasFast = fastNow;

         if (!fastNow) {
            wallTimeElapsed += dt;
            const double elapsedTime{mixr::base::getComputerTime() - startTime};
            const int sleepTime{static_cast<int>((wallTimeElapsed - elapsedTime) * 1000.0)};
            if (sleepTime > 0) mixr::base::msleep(sleepTime);
         }
      }
   });

   DashboardExit action{DashboardExit::Quit};
   int ladderIndex{nearestLadderIndex(clockStation != nullptr ? clockStation->getTimeScale() : 1.0)};

   // Camada de confirmacao -- 'uiDepth' 0 = UI normal, 1 = dialogo de
   // confirmacao por cima. E o MESMO padrao do exemplo oficial
   // modal_dialog_custom.cpp do FTXUI: um Container::Tab so pra ROTEAR
   // evento (so o filho ativo recebe -- ver TabContainer::OnEvent em
   // container.cpp), com a composicao visual (dbox + clear_under) feita a
   // mao no Renderer mais externo, nao no OnRender() do Tab.
   int uiDepth{};
   PendingAction pendingAction{PendingAction::None};

   int activeTab{};
   int selectedEntityIndex{};
   int selectedClassIndex{};
   MapViewState mapView;
   // Caixa de tela do canvas do mapa apos o ultimo desenho (ftxui::reflect,
   // dentro de renderMap()) -- usada pelo CatchEvent mais externo pra saber
   // se um clique caiu DENTRO do mapa antes de tratar como arrasto/selecao
   // (ver o comentario grande na secao do mapa, mais abaixo).
   Box mapCanvasBox{};
   double lastTrailSimSec{-1.0};

   // Largura do card de detalhe -- recalculada a cada redesenho (o
   // terminal pode ser redimensionado em qualquer frame), "ocupando por
   // referencia ate onde o mapa acaba": reserva 'kMapCanvasWidthCells' pro
   // canvas do Mapa mais uma folga pras bordas/separador, e da o RESTO da
   // largura do terminal pro card -- clampado pra nunca ficar
   // absurdamente estreito nem largo.
   int detailPanelWidth{kDetailPanelMinWidth};

   // Copias reconstruidas a cada redesenho -- lidas pelos 'transform' dos
   // Menu abaixo (que precisam de referencia estavel enquanto o componente
   // vive, ao contrario do DashboardState publicado sob mutex).
   std::vector<EntityState> displayedEntities;
   std::vector<std::string> entityLabels;
   std::vector<ClassStat> displayedClasses;
   std::vector<std::string> classLabels;

   // A arvore de BT achatada NAO muda (o arquivo e lido uma vez, no
   // startup -- ver main.cpp) -- calculada aqui, fora de qualquer Renderer.
   // 'selectedBtLineIndex' e a escolha do usuario (clique na "caixa da
   // arvore" -- pedido explicito), compartilhada pelas DUAS instancias de
   // Menu (Frota e Mapa, ver mais abaixo) e pela logica de breakpoint.
   const std::vector<BtTreeLine> treeLines{flattenBehaviorTree(behaviorTree)};
   std::vector<std::string> treeLineLabels;
   for (const auto& line : treeLines) treeLineLabels.push_back(line.display);
   int selectedBtLineIndex{};

   // ---- acoes nomeadas: cada uma e usada por TECLA e por BOTAO ----
   const auto doAccelerate = [&] {
      if (clockStation == nullptr || ladderIndex >= kLadderSize - 1) return;
      ladderIndex++;
      clockStation->setTimeScale(kLadder[ladderIndex]);
   };
   const auto doDecelerate = [&] {
      if (clockStation == nullptr || ladderIndex <= 0) return;
      ladderIndex--;
      clockStation->setTimeScale(kLadder[ladderIndex]);
   };
   const auto doTogglePause = [&] { if (clockStation != nullptr) clockStation->togglePaused(); };
   const auto doRealTime = [&] {
      if (clockStation == nullptr) return;
      ladderIndex = kRealTimeIndex;
      clockStation->setPaused(false);
      clockStation->setTimeScale(kLadder[ladderIndex]);
   };
   // As quatro versoes de VERDADE (o que 'l'/'r'/'s'/'q' faziam direto
   // antes) -- agora so rodam depois de confirmadas (ver 'confirmDialog'
   // mais abaixo). "Parar": derruba a Station (main.cpp faz isso ao sair do
   // laco) e volta para a tela de selecao -- diferente de pausar (congela
   // no lugar).
   const auto doLoadConfirmed = [&] { action = DashboardExit::ChangeScenario; screen.Exit(); };
   const auto doRestartConfirmed = [&] { action = DashboardExit::Restart; screen.Exit(); };
   const auto doStopConfirmed = [&] { action = DashboardExit::ChangeScenario; screen.Exit(); };
   const auto doQuitConfirmed = [&] { action = DashboardExit::Quit; screen.Exit(); };

   const auto runPendingAction = [&] {
      switch (pendingAction) {
         case PendingAction::Load:    doLoadConfirmed(); break;
         case PendingAction::Restart: doRestartConfirmed(); break;
         case PendingAction::Stop:    doStopConfirmed(); break;
         case PendingAction::Quit:    doQuitConfirmed(); break;
         default: break;
      }
   };
   const auto cancelPendingAction = [&] { pendingAction = PendingAction::None; uiDepth = 0; };

   // As quatro que TECLA/BOTAO chamam de verdade -- so ARMAM o dialogo,
   // pedido explicito de confirmacao pras quatro acoes disruptivas.
   const auto doLoad = [&] { pendingAction = PendingAction::Load; uiDepth = 1; };
   const auto doRestart = [&] { pendingAction = PendingAction::Restart; uiDepth = 1; };
   const auto doStop = [&] { pendingAction = PendingAction::Stop; uiDepth = 1; };
   const auto doQuit = [&] { pendingAction = PendingAction::Quit; uiDepth = 1; };
   const auto gotoTab = [&](const int index) { activeTab = index; };

   // Fabrica de Button comum a TODAS as barras (principal e a do mapa) --
   // precisa vir antes de qualquer barra que a use.
   const auto makeButton = [&](const std::string& label, const std::function<void()>& onClick) {
      ButtonOption opt{ButtonOption::Ascii()};
      opt.label = label;
      opt.on_click = onClick;
      return Button(opt);
   };

   // ---- acoes do MAPA -- mesma regra: uma lambda, usada por tecla E por
   // botao (ver a barra de botoes da aba, mais abaixo) ----
   const auto doMapZoomIn = [&] { zoomMap(mapView, true); };
   const auto doMapZoomOut = [&] { zoomMap(mapView, false); };
   const auto doMapRotateLeft = [&] { rotateMap(mapView, false); };
   const auto doMapRotateRight = [&] { rotateMap(mapView, true); };
   const auto doMapToggleTrails = [&] { mapView.showTrails = !mapView.showTrails; };
   const auto doMapTogglePerspective = [&] {
      mapView.perspective = (mapView.perspective == Perspective::TopDown)
         ? Perspective::Lateral : Perspective::TopDown;
   };
   const auto doMapCenterOnSelected = [&] {
      if (displayedEntities.empty()) return;
      const std::size_t idx{static_cast<std::size_t>(
         std::clamp(selectedEntityIndex, 0, static_cast<int>(displayedEntities.size()) - 1))};
      centerMapOn(mapView, displayedEntities[idx]);
   };

   // ---- breakpoint de arvore de BT -- "marcar um estado da bt de um dado
   // elemento e rodar a simulacao ate que aquele no seja atingido,
   // devolvendo a simulacao pausada" ----
   //
   // 'fast' escolhe a velocidade: false = "a velocidade que eu decidir" (so
   // arma o observador -- 'simThread' ja checa a condicao toda amostra,
   // rodando ou pausada, em qualquer escala de tempo; so garante que NAO
   // esta pausada, senao nunca chegaria a lugar nenhum); true = "maxima
   // possivel" (liga 'fastRunToBreakpoint', que faz 'simThread' pular o
   // pacing de parede -- ver o laco la em cima).
   //
   // So arma sobre uma FOLHA (control node -- Fallback/Sequence -- nao tem
   // 'matchesLabel()' fazendo sentido nenhum contra um rotulo de xboard);
   // se a linha selecionada nao e folha, nao faz nada -- 'buildBreakpointStatus()'
   // explica o motivo lendo o mesmo 'selectedBtLineIndex', sem precisar de
   // um campo de erro a parte.
   const auto doArmBreakpoint = [&](const bool fast) {
      if (displayedEntities.empty()) return;
      if (selectedBtLineIndex < 0 || selectedBtLineIndex >= static_cast<int>(treeLines.size())) return;
      const BtTreeLine& line{treeLines[static_cast<std::size_t>(selectedBtLineIndex)]};
      if (!line.leaf) return;

      double currentSimSec{};
      {
         const std::lock_guard<std::mutex> lock(stateMutex);
         currentSimSec = latest.simSec;
      }
      const std::size_t idx{static_cast<std::size_t>(
         std::clamp(selectedEntityIndex, 0, static_cast<int>(displayedEntities.size()) - 1))};
      const EntityState& target{displayedEntities[idx]};

      {
         const std::lock_guard<std::mutex> lock(bpMutex);
         bp = Breakpoint{};
         bp.armed = true;
         bp.entityId = target.id;
         bp.entityName = target.name;
         bp.nodeTag = line.tag;
         bp.fastMode = fast;
         bp.armedAtSimSec = currentSimSec;
      }
      fastRunToBreakpoint = fast;
      if (clockStation != nullptr) clockStation->setPaused(false);
   };
   const auto doCancelBreakpoint = [&] {
      const std::lock_guard<std::mutex> lock(bpMutex);
      bp.armed = false;
      fastRunToBreakpoint = false;
   };
   const Component btnRunToBreakpoint{makeButton("[g] Rodar ate aqui", [&] { doArmBreakpoint(false); })};
   const Component btnRunToBreakpointMax{makeButton("[G] Rodar (max veloc.)", [&] { doArmBreakpoint(true); })};
   const Component btnCancelBreakpoint{makeButton("[x] Cancelar breakpoint", doCancelBreakpoint)};

   // "Ver no mapa" -- so faz sentido no card da Frota (no Mapa voce ja esta
   // la). Como Frota e Mapa ja COMPARTILHAM 'selectedEntityIndex' (a mesma
   // variavel), trocar de aba e o suficiente: a entidade certa ja aparece
   // selecionada do outro lado, sem precisar re-selecionar nada.
   const auto doViewOnMap = [&] { gotoTab(1); };
   const Component btnViewOnMap{makeButton("[m] Ver no mapa", doViewOnMap)};

   // A "caixa onde aparece a bt" -- pedido explicito: clicavel, pra
   // selecionar a folha de interesse (breakpoint). PRECISA de DUAS
   // instancias (uma pra aba Frota, outra pra aba Mapa): um Container::Tab
   // so entrega evento pro filho ATIVO (a razao de existir do proprio
   // 'contentTab' logo abaixo), entao um Menu dentro da Frota nunca
   // receberia clique nenhum enquanto a aba Mapa estivesse em cena. As DUAS
   // apontam pro MESMO 'selectedBtLineIndex'/'treeLineLabels' (o
   // MenuOption e copiado, nao move -- os ponteiros dentro dele continuam
   // os mesmos), entao selecionar numa aba reflete na outra.
   MenuOption btTreeMenuOpt;
   btTreeMenuOpt.entries = &treeLineLabels;
   btTreeMenuOpt.selected = &selectedBtLineIndex;
   btTreeMenuOpt.entries_option.transform = [&](const EntryState& es) -> Element {
      if (es.index < 0 || es.index >= static_cast<int>(treeLines.size())) return text(es.label);
      const BtTreeLine& line{treeLines[static_cast<std::size_t>(es.index)]};

      std::string activeLabel{"--"};
      if (!displayedEntities.empty()) {
         const std::size_t idx{static_cast<std::size_t>(
            std::clamp(selectedEntityIndex, 0, static_cast<int>(displayedEntities.size()) - 1))};
         activeLabel = displayedEntities[idx].behaviorLabel;
      }
      const bool isActiveLeaf{line.leaf && matchesLabel(line.tag, activeLabel)};

      bool isBreakpoint{};
      {
         const std::lock_guard<std::mutex> lock(bpMutex);
         isBreakpoint = bp.armed && bp.nodeTag == line.tag;
      }

      return renderBtLine(line, isActiveLeaf, es.active, isBreakpoint);
   };
   const Component treeMenuFleet{Menu(btTreeMenuOpt)};
   const Component treeMenuMap{Menu(btTreeMenuOpt)};

   // Status do breakpoint + as acoes que dependem dele -- pedido explicito:
   // "devem ficar no quadro da propria arvore" (nao mais uma linha GLOBAL
   // separada). Por isso mora dentro de 'buildDetailPanel', logo abaixo do
   // Menu da arvore -- so chamada de la, dentro do 'if (!treeLines.empty())'
   // (por isso nao precisa mais checar isso aqui). Le 'bp' sob 'bpMutex' e
   // 'selectedBtLineIndex'/'treeLines' direto (mutacao so pelo clique do
   // proprio usuario, sem concorrencia de thread).
   const auto buildBreakpointStatus = [&]() -> Element {
      Breakpoint snap;
      {
         const std::lock_guard<std::mutex> lock(bpMutex);
         snap = bp;
      }

      if (snap.armed) {
         std::ostringstream oss;
         oss << "Aguardando: " << snap.entityName << " -> \"" << snap.nodeTag << "\""
             << (snap.fastMode ? " (velocidade maxima)" : " (velocidade atual)");
         return hbox({text(oss.str()) | color(Color::Yellow) | bold, text("  "),
                      btnCancelBreakpoint->Render()});
      }
      if (snap.hit) {
         std::ostringstream oss;
         oss << "Breakpoint atingido: " << snap.entityName << " chegou em \"" << snap.nodeTag
             << "\" (sim=" << std::fixed << std::setprecision(1) << snap.hitSimSec
             << "s) -- simulacao PAUSADA";
         return text(oss.str()) | color(Color::Green) | bold;
      }
      if (snap.timedOut) {
         std::ostringstream oss;
         oss << "Breakpoint NAO atingido em " << std::fixed << std::setprecision(0)
             << kBreakpointTimeoutSimSec << "s de simulacao -- cancelado";
         return text(oss.str()) | color(Color::Red);
      }
      if (selectedBtLineIndex >= 0 && selectedBtLineIndex < static_cast<int>(treeLines.size())) {
         const BtTreeLine& line{treeLines[static_cast<std::size_t>(selectedBtLineIndex)]};
         if (!line.leaf) {
            return text("Selecione uma FOLHA da arvore (nao um no de controle) para marcar um breakpoint")
                  | dim;
         }
         return hbox({text("Folha selecionada: \"" + line.tag + "\"  "),
                      btnRunToBreakpoint->Render(), text(" "), btnRunToBreakpointMax->Render()});
      }
      return text("Clique numa folha da arvore (Players ou Mapa) para marcar um breakpoint") | dim;
   };

   // O card de detalhe INTEIRO (campos + arvore de BT + os controles de
   // breakpoint, se houver arvore) -- usado pelas DUAS abas (Frota e Mapa),
   // sempre com o MESMO tamanho ('detailPanelWidth'/'kDetailPanelHeight',
   // de app/FleetPanel.hpp -- pedido explicito: "deve ter tamanho fixo ao
   // se navegar entre players e entre abas"). A arvore (e os controles de
   // breakpoint junto dela, tambem pedido explicito: "devem ficar no
   // quadro da propria arvore") so aparece quando a entidade TEM
   // comportamento publicado (behaviorLabel != "--") E o cenario declarou
   // 'treeFile:' que deu pra carregar -- ver app/BehaviorTreeView.hpp.
   // 'treeMenu' e qual das duas instancias (Frota/Mapa) usar.
   const auto buildDetailPanel = [&](const EntityState& e, const Component& treeMenu) -> Element {
      Elements parts{renderEntityDetail(e)};
      if (e.behaviorLabel != "--" && !treeLines.empty()) {
         parts.push_back(separator());
         parts.push_back(treeMenu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 14));
         parts.push_back(buildBreakpointStatus());
      }
      return vbox(std::move(parts)) | size(WIDTH, EQUAL, detailPanelWidth)
            | size(HEIGHT, EQUAL, kDetailPanelHeight);
   };

   // ---- aba "Frota": lista rolavel + detalhe da entidade selecionada ----
   MenuOption entityMenuOpt;
   entityMenuOpt.entries = &entityLabels;
   entityMenuOpt.selected = &selectedEntityIndex;
   entityMenuOpt.entries_option.transform = [&](const EntryState& es) -> Element {
      if (es.index >= 0 && es.index < static_cast<int>(displayedEntities.size()))
         return renderEntityRow(displayedEntities[static_cast<std::size_t>(es.index)], es.active);
      return text(es.label);
   };
   const Component entityMenu{Menu(entityMenuOpt)};

   // 'treeMenuFleet' entra como FILHO deste Renderer (nao Renderer solto) --
   // e o que faz o clique nele chegar de verdade (o broadcast de mouse
   // desce por TODOS os filhos de um Container, mas so alcanca quem esta
   // de fato na arvore de componentes).
   const Component entityDetail{Renderer(treeMenuFleet, [&]() -> Element {
      if (displayedEntities.empty()) {
         return text("(sem entidades no cenario)") | dim | center
               | size(WIDTH, EQUAL, detailPanelWidth) | size(HEIGHT, EQUAL, kDetailPanelHeight);
      }
      const std::size_t idx{static_cast<std::size_t>(
         std::clamp(selectedEntityIndex, 0, static_cast<int>(displayedEntities.size()) - 1))};
      return buildDetailPanel(displayedEntities[idx], treeMenuFleet);
   })};

   const Component fleetBody{Container::Horizontal({entityMenu, entityDetail})};
   const Component fleetTab{Renderer(fleetBody, [&]() -> Element {
      return hbox({
                entityMenu->Render() | vscroll_indicator | frame | size(WIDTH, LESS_THAN, 60) | flex,
                separator(),
                entityDetail->Render(),
             })
             | flex;
   })};

   // ---- aba "Mapa": navegavel (arrastar/setas move, zoom por [ ]/roda,
   // girar por ,/., trocar perspectiva/rastro por botao ou tecla) + painel
   // de detalhe lateral (o MESMO buildDetailPanel() da aba Frota -- clicar
   // numa entidade no mapa muda 'selectedEntityIndex', a mesma variavel que
   // a aba Frota usa, entao as duas abas sempre concordam sobre "quem esta
   // selecionada") ----
   const Component mapCanvasArea{Renderer(treeMenuMap, [&]() -> Element {
      int focusedId{-1};
      Element detail{text("(clique numa entidade no mapa, ou selecione nos Players)")
                     | dim | center
                     | size(WIDTH, EQUAL, detailPanelWidth) | size(HEIGHT, EQUAL, kDetailPanelHeight)};
      if (!displayedEntities.empty()) {
         const std::size_t idx{static_cast<std::size_t>(
            std::clamp(selectedEntityIndex, 0, static_cast<int>(displayedEntities.size()) - 1))};
         focusedId = displayedEntities[idx].id;
         detail = buildDetailPanel(displayedEntities[idx], treeMenuMap);
      }
      return hbox({
                renderMap(displayedEntities, mapView, focusedId, mapCanvasBox) | flex,
                separator(),
                detail,
             })
             | flex;
   })};

   const Component btnMapZoomOut{makeButton("[[] Zoom-", doMapZoomOut)};
   const Component btnMapZoomIn{makeButton("[]] Zoom+", doMapZoomIn)};
   const Component btnMapRotL{makeButton("[,] Girar<", doMapRotateLeft)};
   const Component btnMapRotR{makeButton("[.] Girar>", doMapRotateRight)};
   const Component btnMapCenter{makeButton("[c] Centralizar", doMapCenterOnSelected)};

   // Os dois de alternancia (rastro/perspectiva) precisam de um rotulo que
   // MUDA com o estado (ON/OFF, Cima/Lado) -- 'transform' roda a cada
   // redesenho (nao so no clique), entao basta ler 'mapView' direto nele.
   ButtonOption trailsOpt;
   trailsOpt.label = "[t] Rastro";
   trailsOpt.on_click = doMapToggleTrails;
   trailsOpt.transform = [&](const EntryState&) {
      return text(std::string("[t] Rastro: ") + (mapView.showTrails ? "ON" : "OFF"))
         | (mapView.showTrails ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapTrails{Button(trailsOpt)};

   ButtonOption perspectiveOpt;
   perspectiveOpt.label = "[v] Vista";
   perspectiveOpt.on_click = doMapTogglePerspective;
   perspectiveOpt.transform = [&](const EntryState&) {
      const bool lateral{mapView.perspective == Perspective::Lateral};
      return text(std::string("[v] Vista: ") + (lateral ? "Lado" : "Cima"))
         | (lateral ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapPerspective{Button(perspectiveOpt)};

   const Component mapButtons{Container::Horizontal({
      btnMapZoomOut, btnMapZoomIn, btnMapRotL, btnMapRotR,
      btnMapCenter, btnMapTrails, btnMapPerspective,
   })};

   const Component mapBody{Container::Vertical({mapCanvasArea, mapButtons})};
   const Component mapTab{Renderer(mapBody, [&]() -> Element {
      return vbox({
         mapCanvasArea->Render() | flex,
         text("[setas/arraste] mover  [clique] selecionar entidade") | dim,
         mapButtons->Render(),
      });
   })};
   // O tratamento de tecla/mouse do mapa NAO fica num CatchEvent local aqui
   // -- ver o comentario grande sobre 'withKeys' mais abaixo: Container::
   // Vertical so encaminha TECLADO para o filho FOCADO (ContainerBase::
   // OnEvent checa Focused() antes de descer), e 'root' tem dois filhos
   // (toolbar/contentTab) competindo pelo foco. Um CatchEvent aninhado
   // dentro de 'contentTab' ficaria refem de qual dos dois esta focado NO
   // MOMENTO. A solucao robusta e tratar tudo no CatchEvent MAIS EXTERNO
   // (roda incondicionalmente antes de qualquer roteamento por foco), igual
   // +/-/espaco/etc ja fazem -- e gatear por 'mapCanvasBox.Contain(x,y)'
   // pros eventos de MOUSE, que e o que faltava antes (ver a armadilha 7
   // desta secao no CLAUDE.md: sem o gate, QUALQUER clique na tela --
   // inclusive nos botoes [F1]/[F3] -- era engolido como "comecar a
   // arrastar o mapa" e nunca chegava ao botao, travando a troca de aba).

   // ---- aba "Memoria": contadores de instancia AO VIVO, ver
   // app/MetaObjectSnapshot.hpp para o criterio de "CRESCENDO" ----
   MenuOption classMenuOpt;
   classMenuOpt.entries = &classLabels;
   classMenuOpt.selected = &selectedClassIndex;
   classMenuOpt.entries_option.transform = [&](const EntryState& es) -> Element {
      if (es.index >= 0 && es.index < static_cast<int>(displayedClasses.size()))
         return renderClassRow(displayedClasses[static_cast<std::size_t>(es.index)], es.active);
      return text(es.label);
   };
   const Component classMenu{Menu(classMenuOpt)};
   const Component memoryTab{Renderer(classMenu, [&]() -> Element {
      return vbox({
         text("classes observadas: " + std::to_string(displayedClasses.size())) | dim,
         classMenu->Render() | vscroll_indicator | frame | flex,
      });
   })};

   const Component contentTab{Container::Tab({fleetTab, mapTab, memoryTab}, &activeTab)};

   // ---- barra de abas e barra de acoes, TODAS clicaveis (Button de
   // verdade), com a dica de atalho ja no rotulo ----

   const Component btnFleet{makeButton("[F1] Players", [&] { gotoTab(0); })};
   const Component btnMap{makeButton("[F2] Mapa", [&] { gotoTab(1); })};
   const Component btnMemory{makeButton("[F3] Memoria", [&] { gotoTab(2); })};
   const Component btnAccel{makeButton("[+] Acelerar", doAccelerate)};
   const Component btnDecel{makeButton("[-] Frear", doDecelerate)};
   const Component btnPause{makeButton("[espaco] Pausar", doTogglePause)};
   const Component btnReal{makeButton("[1] Tempo real", doRealTime)};
   const Component btnLoad{makeButton("[l] Carregar", doLoad)};
   const Component btnRestart{makeButton("[r] Reiniciar", doRestart)};
   const Component btnStop{makeButton("[s] Parar", doStop)};
   const Component btnQuit{makeButton("[q] Sair", doQuit)};

   const Component toolbar{Container::Horizontal({
      btnFleet, btnMap, btnMemory,
      btnAccel, btnDecel, btnPause, btnReal, btnViewOnMap,
      btnLoad, btnRestart, btnStop, btnQuit,
   })};

   const Component breakpointBar{Container::Horizontal({
      btnRunToBreakpoint, btnRunToBreakpointMax, btnCancelBreakpoint,
   })};

   const Component root{Container::Vertical({toolbar, breakpointBar, contentTab})};

   const Component withRenderer{Renderer(root, [&]() -> Element {
      DashboardState snap;
      {
         const std::lock_guard<std::mutex> lock(stateMutex);
         snap = latest;
      }

      displayedEntities = snap.entities;
      displayedClasses = snap.classStats;

      // So acrescenta ao rastro quando a amostra e REALMENTE nova -- este
      // Renderer roda a cada redesenho, nao so a cada captura nova (uma
      // tecla ou um resize tambem disparam render), e sem essa guarda o
      // rastro ganharia pontos duplicados sobrepostos.
      if (snap.simSec != lastTrailSimSec) {
         updateTrails(mapView, displayedEntities);
         lastTrailSimSec = snap.simSec;
      }

      entityLabels.clear();
      for (const auto& e : displayedEntities) entityLabels.push_back(entityRowText(e));
      classLabels.clear();
      for (const auto& c : displayedClasses) classLabels.push_back(classRowText(c));

      if (!displayedEntities.empty()) {
         selectedEntityIndex = std::clamp(selectedEntityIndex, 0,
            static_cast<int>(displayedEntities.size()) - 1);
      }
      if (!displayedClasses.empty()) {
         selectedClassIndex = std::clamp(selectedClassIndex, 0,
            static_cast<int>(displayedClasses.size()) - 1);
      }
      if (!treeLines.empty()) {
         selectedBtLineIndex = std::clamp(selectedBtLineIndex, 0,
            static_cast<int>(treeLines.size()) - 1);
      }

      // Recalculado a cada redesenho -- o terminal pode ser redimensionado
      // em qualquer frame. "+6" cobre a borda do canvas (2) + separador (1)
      // + borda do proprio card (2) + uma folga de 1.
      detailPanelWidth = std::clamp(Terminal::Size().dimx - (kMapCanvasWidthCells + 6),
                                    kDetailPanelMinWidth, kDetailPanelMaxWidth);

      const auto tabBadge = [&](const Component& btn, const int index) -> Element {
         Element e{btn->Render()};
         if (activeTab == index) return e | bgcolor(Color::Blue) | bold;
         return e | dim;
      };

      // "[m] Ver no mapa" -- pedido explicito: posicao mais visivel, igual
      // aos demais botoes principais, so aparece quando ha alguma entidade
      // selecionada (senao nao ha "onde ir").
      Elements primaryButtons{btnAccel->Render(), btnDecel->Render(), btnPause->Render(),
                              btnReal->Render()};
      if (!displayedEntities.empty()) primaryButtons.push_back(btnViewOnMap->Render());

      return vbox({
         renderHeader(snap),
         hbox({tabBadge(btnFleet, 0), tabBadge(btnMap, 1), tabBadge(btnMemory, 2)}),
         separator(),
         contentTab->Render() | flex,
         separator(),
         hbox({
            hbox(std::move(primaryButtons)),
            filler(),
            btnLoad->Render(), btnRestart->Render(), btnStop->Render(), btnQuit->Render(),
         }),
      });
   })};

   const Component withKeys{CatchEvent(withRenderer, [&](Event event) -> bool {
      if (event == Event::Character('+') || event == Event::Character('=')) { doAccelerate(); return true; }
      if (event == Event::Character('-') || event == Event::Character('_')) { doDecelerate(); return true; }
      if (event == Event::Character(' ') || event == Event::Character('p') ||
          event == Event::Character('P')) { doTogglePause(); return true; }
      if (event == Event::Character('1')) { doRealTime(); return true; }
      if (event == Event::F1) { gotoTab(0); return true; }
      if (event == Event::F2) { gotoTab(1); return true; }
      if (event == Event::F3) { gotoTab(2); return true; }

      // Navegacao por seta das listas (Frota/Memoria) -- tratada AQUI, no
      // CatchEvent mais externo, e nao deixada para o ftxui::Menu receber
      // via o roteamento normal por foco: 'root' e um Container::Vertical
      // de dois filhos (toolbar/contentTab) e so encaminha teclado para o
      // filho FOCADO (ContainerBase::OnEvent checa Focused() antes de
      // descer) -- por padrao esse filho e 'toolbar', entao a seta nunca
      // alcancaria o Menu sem o usuario navegar o foco ate la primeiro.
      // Mexer direto em 'selectedEntityIndex'/'selectedClassIndex' tem o
      // MESMO efeito da navegacao interna do Menu (o campo 'selected' dele
      // e um ponteiro pra esta mesma variavel), sem depender da cadeia de
      // foco.
      if (activeTab == 0 && !displayedEntities.empty()) {
         const int last{static_cast<int>(displayedEntities.size()) - 1};
         if (event == Event::ArrowDown) { selectedEntityIndex = std::min(selectedEntityIndex + 1, last); return true; }
         if (event == Event::ArrowUp)   { selectedEntityIndex = std::max(selectedEntityIndex - 1, 0); return true; }
         if (event == Event::Character('m') || event == Event::Character('M')) { doViewOnMap(); return true; }
      }
      if (activeTab == 2 && !displayedClasses.empty()) {
         const int last{static_cast<int>(displayedClasses.size()) - 1};
         if (event == Event::ArrowDown) { selectedClassIndex = std::min(selectedClassIndex + 1, last); return true; }
         if (event == Event::ArrowUp)   { selectedClassIndex = std::max(selectedClassIndex - 1, 0); return true; }
      }

      // Breakpoint de BT -- GLOBAL (nao depende de aba), mesmo raciocinio
      // do status em 'buildBreakpointStatus()'.
      if (event == Event::Character('g')) { doArmBreakpoint(false); return true; }
      if (event == Event::Character('G')) { doArmBreakpoint(true); return true; }
      if (event == Event::Character('x') || event == Event::Character('X')) { doCancelBreakpoint(); return true; }

      // As quatro pedem CONFIRMACAO agora (ver 'confirmDialog' mais abaixo)
      // -- 'Escape' saiu daqui de proposito: dentro do dialogo ele significa
      // "cancelar", e mante-lo tambem como atalho de 'q' aqui geraria o
      // efeito estranho de Escape armar a confirmacao de sair e o Escape
      // SEGUINTE cancelar ela na hora.
      if (event == Event::Character('l') || event == Event::Character('L')) { doLoad(); return true; }
      if (event == Event::Character('r') || event == Event::Character('R')) { doRestart(); return true; }
      if (event == Event::Character('s') || event == Event::Character('S')) { doStop(); return true; }
      if (event == Event::Character('q') || event == Event::Character('Q')) { doQuit(); return true; }

      // Interacao do MAPA -- so quando a aba esta ativa, tratada aqui (nao
      // num CatchEvent aninhado em 'mapTab') pelo motivo explicado no
      // comentario de 'mapTab' acima: o CatchEvent mais externo roda
      // incondicionalmente, sem depender de qual filho de 'root' esta
      // focado no momento.
      if (activeTab == 1) {
         if (event.is_mouse()) {
            const Mouse& m{event.mouse()};
            const bool insideCanvas{mapCanvasBox.Contain(m.x, m.y)};

            // Um arrasto EM ANDAMENTO processa ate soltar, mesmo que o
            // mouse escape do canvas por um instante (movimento rapido) --
            // so o COMECO (Pressed) e a roda exigem estar dentro do canvas.
            // Sem esse gate, QUALQUER clique na tela (inclusive nos botoes
            // de troca de aba) era engolido como "comecar a arrastar o
            // mapa" e nunca chegava ao componente por baixo -- a causa da
            // aba Mapa "prender" a navegacao (ver CLAUDE.md).
            if (mapView.dragging) {
               if (m.motion == Mouse::Released) {
                  mapView.dragging = false;
                  const int totalMove{std::abs(m.x - mapView.pressX) + std::abs(m.y - mapView.pressY)};
                  if (totalMove <= 1) {
                     // Deslocamento minimo entre Pressed e Released: e um
                     // CLIQUE, nao um arrasto -- seleciona a entidade sob o
                     // cursor (se houver) pro painel lateral e pra aba
                     // Frota (mesma 'selectedEntityIndex' das duas abas).
                     const int cellX{m.x - mapCanvasBox.x_min};
                     const int cellY{m.y - mapCanvasBox.y_min};
                     const int hitId{hitTestEntity(displayedEntities, mapView, cellX, cellY)};
                     if (hitId >= 0) {
                        for (std::size_t i = 0; i < displayedEntities.size(); i++) {
                           if (displayedEntities[i].id == hitId) {
                              selectedEntityIndex = static_cast<int>(i);
                              break;
                           }
                        }
                     }
                  }
                  return true;
               }
               if (m.motion == Mouse::Moved) {
                  const int dx{m.x - mapView.dragLastX};
                  const int dy{m.y - mapView.dragLastY};
                  mapView.dragLastX = m.x;
                  mapView.dragLastY = m.y;
                  // Arrastar pra direita/cima deve fazer o CONTEUDO seguir o
                  // cursor -- ver o "porque" do sinal em panMap()
                  // (MapPanel.cpp): a tela e 2 px de canvas por celula na
                  // horizontal, 4 na vertical, e o eixo Y do terminal cresce
                  // pra baixo.
                  panMap(mapView, -dx * mapView.metersPerCell * 2.0, dy * mapView.metersPerCell * 4.0);
                  return true;
               }
               return true;
            }

            if (!insideCanvas) return false;
            if (m.button == Mouse::WheelUp) { doMapZoomIn(); return true; }
            if (m.button == Mouse::WheelDown) { doMapZoomOut(); return true; }
            if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
               mapView.dragging = true;
               mapView.pressX = m.x;
               mapView.pressY = m.y;
               mapView.dragLastX = m.x;
               mapView.dragLastY = m.y;
               return true;
            }
            return false;
         }

         if (event == Event::ArrowLeft)  { panMap(mapView, -mapView.metersPerCell * 4.0, 0.0); return true; }
         if (event == Event::ArrowRight) { panMap(mapView,  mapView.metersPerCell * 4.0, 0.0); return true; }
         if (event == Event::ArrowUp)    { panMap(mapView, 0.0,  mapView.metersPerCell * 4.0); return true; }
         if (event == Event::ArrowDown)  { panMap(mapView, 0.0, -mapView.metersPerCell * 4.0); return true; }
         if (event == Event::Character('[')) { doMapZoomOut(); return true; }
         if (event == Event::Character(']')) { doMapZoomIn(); return true; }
         if (event == Event::Character(',')) { doMapRotateLeft(); return true; }
         if (event == Event::Character('.')) { doMapRotateRight(); return true; }
         if (event == Event::Character('t') || event == Event::Character('T')) { doMapToggleTrails(); return true; }
         if (event == Event::Character('v') || event == Event::Character('V')) { doMapTogglePerspective(); return true; }
         if (event == Event::Character('c') || event == Event::Character('C')) { doMapCenterOnSelected(); return true; }
      }

      return false;
   })};

   // ---- dialogo de confirmacao (carregar/reiniciar/parar/sair) --
   // MESMO padrao do exemplo oficial modal_dialog_custom.cpp do FTXUI:
   // Container::Tab so pra ROTEAR evento (so o filho ATIVO recebe -- ver
   // TabContainer::OnEvent, container.cpp) e a composicao visual (dbox +
   // clear_under, por cima do resto congelado) feita a mao no Renderer mais
   // externo. Enquanto 'uiDepth==1', 'withKeys' nao recebe evento NENHUM --
   // e o que bloqueia interacao com o resto da UI ate confirmar/cancelar.
   const Component btnConfirmYes{makeButton("[Enter] Confirmar", runPendingAction)};
   const Component btnConfirmNo{makeButton("[Esc] Cancelar", cancelPendingAction)};
   const Component confirmButtons{Container::Horizontal({btnConfirmYes, btnConfirmNo})};
   const Component confirmDialogBody{Renderer(confirmButtons, [&]() -> Element {
      return vbox({
                text(" Confirmar acao ") | bold | bgcolor(Color::Yellow) | color(Color::Black),
                separator(),
                text("Deseja realmente " + pendingActionLabel(pendingAction) + "?") | center,
                text(""),
                hbox({filler(), btnConfirmYes->Render(), text("   "), btnConfirmNo->Render(), filler()}),
             })
             | border | bgcolor(Color::Black) | size(WIDTH, EQUAL, 44);
   })};
   const Component confirmDialog{CatchEvent(confirmDialogBody, [&](Event event) -> bool {
      if (event == Event::Return) { runPendingAction(); return true; }
      if (event == Event::Escape) { cancelPendingAction(); return true; }
      return false;   // deixa os botoes do dialogo tratar clique normalmente
   })};

   const Component appLayers{Container::Tab({withKeys, confirmDialog}, &uiDepth)};
   const Component appRoot{Renderer(appLayers, [&]() -> Element {
      Element doc{withKeys->Render()};
      if (uiDepth == 1) {
         doc = dbox({doc, confirmDialog->Render() | clear_under | center});
      }
      return doc;
   })};

   // Ctrl+C: o FTXUI ja instala o proprio handler e sai do Loop() sozinho
   // (App::ForceHandleCtrlC(true) e o default) -- 'action' fica em Quit, que
   // e exatamente o que se quer.
   screen.Loop(appRoot);

   running = false;
   simThread.join();

   return action;
}

} // namespace app
