#include "app/DashboardLoop.hpp"

#include "mixr/linkage/IoHandler.hpp"

#include "app/DashboardWiring.hpp"
#include "app/Fleet.hpp"
#include "app/FleetPanel.hpp"
#include "app/LogPanel.hpp"
#include "app/MemoryPanel.hpp"
#include "app/Shutdown.hpp"

#include "xboard/Board.hpp"
#include "xclock/ClockStation.hpp"
#include "xlog/Log.hpp"
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
#include <cstdio>
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
// FASE 7 (2026-09): a construcao de CADA aba (Players/Mapa/Memoria/Tempo
// Nao-Critico/Log/Componentes/EDL) foi extraida para
// app/Dashboard{Fleet,Map,Memory,Background,Log,Components,Edl}Tab.cpp, uma
// funcao 'buildXTab(DashboardWiring&)' por arquivo -- ver app/DashboardWiring.hpp
// para o que agrega e o que DELIBERADAMENTE fica so' aqui (o CatchEvent
// unico, a barra de ferramentas, o dialogo de confirmacao, as duas threads).
// Este arquivo ficou com: montagem do DashboardWiring, 'simThread', a
// chamada aos sete 'buildXTab()' (ORDEM importa: Fleet antes de Map, ver o
// comentario em DashboardWiring.hpp), a barra de ferramentas/'contentTab'/
// 'withKeys'/'appRoot', e o Loop() final do FTXUI.
//
// POR QUE NAO libs/xclock::TimeControls/ConsoleKeyboard: os dois mexem em
// termios (modo bruto do terminal) por fora do FTXUI, que ja e dono do
// terminal assim que ScreenInteractive::Fullscreen() comeca. As acoes de
// controle de tempo aqui chamam ClockStation::setTimeScale()/
// togglePaused()/setPaused() DIRETO -- a mesma API que TimeControls::apply()
// ja usa por baixo.
//
// CADA ACAO (acelerar/frear/pausar/trocar de aba/reiniciar/sair) e uma
// UNICA lambda nomeada, chamada tanto pelo atalho de teclado
// quanto pelo Button correspondente -- e o que da "elemento clicavel com
// dica de atalho" sem duplicar logica (ver a barra de botoes no fim desta
// funcao).
//
// SETE ABAS (ftxui::Container::Tab) -- Players/Mapa/Memoria/Log sao cada uma
// um ftxui::Menu dentro de frame()/vscroll_indicator() (padrao oficial do
// FTXUI para lista rolavel -- ver o exemplo menu_in_frame.cpp da propria
// lib): e o que deixa a UI caber QUALQUER quantidade de entidades/classes
// sem crescer a tela. Tempo Nao-Critico e um painel estatico -- ver
// app/BackgroundPanel.hpp. A aba Log le o buffer em memoria de
// libs/xlog (ver app/LogPanel.hpp). A aba "EDL" (F7) e a UNICA que
// precisa de foco de TECLADO de verdade (um ftxui::Input multilinha, ver
// app/EdlEditorState.hpp) -- as outras seis nunca precisaram porque cada
// tecla e tratada a mao no CatchEvent mais externo (ver o comentario grande
// sobre 'edlInput->TakeFocus()' mais abaixo, no motivo do porque).
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

// As duas acoes disruptivas (reiniciam o cenario ou encerram o processo) --
// pedido explicito: confirmar antes de executar. Ver 'uiDepth'/'pendingAction'
// em runDashboard().
enum class PendingAction { None, Restart, Quit };

std::string pendingActionLabel(const PendingAction a)
{
   switch (a) {
      case PendingAction::Restart: return "reiniciar o cenario atual";
      case PendingAction::Quit:    return "sair do dashboard";
      default:                    return "";
   }
}

Color speedToneColor(const SpeedTone tone)
{
   switch (tone) {
      case SpeedTone::Yellow:  return Color::Yellow;
      case SpeedTone::Cyan:    return Color::Cyan;
      case SpeedTone::Red:     return Color::Red;
      case SpeedTone::Magenta: return Color::Magenta;
      case SpeedTone::Green:
      default:                return Color::Green;
   }
}

Element renderHeader(const DashboardState& st)
{
   std::ostringstream tw;
   tw << "t=" << std::fixed << std::setprecision(0) << st.wallSec << "s";
   std::ostringstream ts;
   ts << "sim=" << std::fixed << std::setprecision(1) << st.simSec << "s";

   const SpeedDisplay disp{speedDisplay(st.fastBreakpointRun, st.paused, st.timeScale, st.actualTimeScale)};
   std::string speedLabel{disp.label};
   Color speedColor{speedToneColor(disp.tone)};

   // Um BP armado TRAVA os controles manuais de velocidade nos DOIS modos
   // ('g'/'G' -- ver isBreakpointArmedNow() em runDashboard()), e isso tem
   // de ficar obvio mesmo fora do card da arvore (pedido explicito). O modo
   // rapido ja fala por si (label "MAX (~Nx real)" em Magenta); e no modo
   // de velocidade ATUAL a cor normal (verde/amarelo/ciano) nao diz nada
   // sobre a trava -- daí o override pra Azul so nesse caso (mesmo tom que
   // o card da arvore ja usa pra folha ativa, ver renderBtLine()).
   if (st.breakpointArmed) {
      speedLabel += " [BP]";
      if (!st.fastBreakpointRun) speedColor = Color::Blue;
   }

   return hbox({
             text(" app ") | bold | bgcolor(Color::Blue) | color(Color::White),
             text(" " + st.scenarioLabel + " ") | bold,
             text(" entidades=" + std::to_string(st.entities.size()) + " ") | dim,
             filler(),
             text(tw.str() + "  " + ts.str() + "  "),
             text(" " + speedLabel + " ") | bold | bgcolor(speedColor) | color(Color::Black),
             text("  numTcThreads=" + std::to_string(st.numTcThreads)
                  + " numBgThreads=" + std::to_string(st.numBgThreads) + " "),
          })
          | border;
}

}

DashboardExit runDashboard(mixr::simulation::Station* const station,
                           mixr::models::WorldModel* const worldModel,
                           mixr::xclock::ClockStation* const clockStation,
                           mixr::xtacview::TacviewOutput* const tacviewOutput,
                           mixr::linkage::IoHandler* const ioHandler,
                           const int numTcThreads, const int numBgThreads,
                           const std::string& scenarioLabel,
                           const BtNode& behaviorTree, const std::string& generatedEdlPath)
{
   // Todo o estado que antes era local a esta funcao mora agora em 'w' (ver
   // app/DashboardWiring.hpp) -- 'w' e' uma variavel local DESTA funcao,
   // entao vive exatamente pelo tempo que os Component/lambda construidos a
   // partir dela precisam (ate' o Loop() do FTXUI retornar, mais adiante).
   DashboardWiring w;
   w.station = station;
   w.worldModel = worldModel;
   w.clockStation = clockStation;
   w.tacviewOutput = tacviewOutput;
   w.ioHandler = ioHandler;
   w.numTcThreads = numTcThreads;
   w.numBgThreads = numBgThreads;
   w.scenarioLabel = scenarioLabel;
   w.generatedEdlPath = generatedEdlPath;

   // Amostrador de terreno da aba Mapa -- construido UMA vez, aqui, e nao a
   // cada redesenho. makeTerrainSampler() le worldModel->getRefLatitude()/
   // getRefLongitude() na construcao e captura os dois por VALOR; o corpo do
   // amostrador so consulta o cache proprio de tiles, sob mutex, sem tocar o
   // WorldModel.
   w.terrainSampler = makeTerrainSampler(worldModel);

   // A arvore de BT achatada NAO muda (o arquivo e lido uma vez, no
   // startup -- ver main.cpp).
   w.treeLines = flattenBehaviorTree(behaviorTree);
   for (const auto& line : w.treeLines) w.treeLineLabels.push_back(line.display);

   w.ladder.seedFromScale(clockStation != nullptr ? clockStation->getTimeScale() : 1.0);

   std::mutex stateMutex;
   DashboardState latest;
   std::atomic<bool> running{true};

   // A aba F6 esta em cena? Escrito pela thread de DESENHO, lido por
   // 'simThread', que so entao percorre o grafo vivo do MIXR para montar a
   // arvore de componentes (ver DashboardState::componentTree).
   std::atomic<bool> wantComponentTree{false};

   // A partir daqui o FTXUI e dono do terminal (alternate screen buffer,
   // modo bruto) -- uma linha de log escrita direto em std::cout suja o
   // desenho, e o FTXUI nao sabe que alguem escreveu por baixo dele para
   // redesenhar aquela regiao. Desliga SO o console: arquivo
   // (./app/data/logs/app.log) e buffer em memoria continuam, e e do
   // buffer que a aba Log le. Religado no fim desta funcao.
   mixr::xlog::setConsoleEnabled(false);

   auto screen = ScreenInteractive::Fullscreen();
   w.screen = &screen;

   // Liga a medicao de duracao do frame de tempo critico (aba F4). Uma vez
   // so, antes do laco. NUNCA setPrintTimingStats(true): esse flag faz
   // Component::printTimingStats() escrever direto em std::cout, e o FTXUI e
   // dono do terminal.
   station->setTimingStatsEnabled(true);

   std::thread simThread([&] {
      station->createTimeCriticalProcess();
      mixr::base::msleep(settleMs);

      const double dt{1.0 / static_cast<double>(bgRate)};
      double wallTimeElapsed{};
      double startTime{mixr::base::getComputerTime()};
      long frameCount{};

      // Referencia de PAREDE de verdade para "t=" no cabecalho -- NUNCA
      // resetada. Ver o comentario grande na versao anterior deste arquivo
      // (diario, CLAUDE.md): o modo rapido pula o msleep() e giraria muito
      // mais que 1/dt vezes por segundo, e "t=" nao pode acelerar so porque
      // a simulacao esta em MAX.
      const double realWallClockStart{mixr::base::getComputerTime()};
      std::vector<ClassStat> classHistory;
      bool wasFast{};

      // Velocidade FACTUAL (medida) -- tempo simulado / tempo de PAREDE de
      // verdade, numa janela deslizante de ~0.5s.
      double speedMarkRealTime{mixr::base::getComputerTime()};
      double speedMarkSimSec{0.0};
      double measuredActualTimeScale{1.0};

      // Ver o comentario grande de app::BackgroundInfo (DashboardState.hpp):
      // este laco INTEIRO e a "thread de tempo nao critico" da aba Tempo Nao-Critico.
      double bgRateMarkRealTime{mixr::base::getComputerTime()};
      long bgRateMarkCount{};
      double measuredBgHz{static_cast<double>(bgRate)};
      long radarScanPushCount{};

      while (running.load()) {
         const double iterationStart{mixr::base::getComputerTime()};

         // Identidade real de cada player (tipo/lado/major type) ANTES de
         // drenar o gravador -- e o updateData() abaixo que DECLARA cada
         // objeto no stream ACMI, e Name/Type/Color so vao na primeira
         // aparicao de cada id.
         if (tacviewOutput != nullptr) tacviewOutput->publishIdentities(worldModel);

         // Joystick (so o cenario 'bandit' declara um 'ioHandler:'): mesma
         // taxa e mesmo lugar do laco de tempo real que as pocs usavam
         // antes de o ./app virar o runner unico delas -- 10 Hz, fora do
         // frame de tempo critico.
         if (ioHandler != nullptr) ioHandler->inputDevices(dt);

         station->updateData(dt);

         if (tacviewOutput != nullptr) {
            const double simTime{worldModel->getExecTimeSec()};
            for (auto* const player : discoverPlayers(worldModel)) {
               const mixr::xboard::Readout board{mixr::xboard::get(player->getID())};
               if (board.radarValid) {
                  tacviewOutput->updateRadarScan(static_cast<std::uint32_t>(player->getID()), simTime,
                     board.radarAzDeg, board.radarElDeg, board.radarRangeM,
                     board.radarHBeamDeg, board.radarVBeamDeg);
                  radarScanPushCount += 1;
               }
            }
         }

         frameCount += 1;
         DashboardState next{captureState(worldModel, station, tacviewOutput,
                                          mixr::base::getComputerTime() - realWallClockStart,
                                          worldModel->getExecTimeSec(), clockStation,
                                          numTcThreads, numBgThreads, scenarioLabel, classHistory,
                                          wantComponentTree.load(std::memory_order_relaxed))};
         classHistory = next.classStats;

         {
            const double nowReal{mixr::base::getComputerTime()};
            const double dReal{nowReal - speedMarkRealTime};
            if (dReal >= 0.5) {
               const double dSim{next.simSec - speedMarkSimSec};
               measuredActualTimeScale = (dReal > 1e-6) ? (dSim / dReal) : 0.0;
               speedMarkRealTime = nowReal;
               speedMarkSimSec = next.simSec;
            }
         }

         bgRateMarkCount += 1;
         {
            const double nowReal{mixr::base::getComputerTime()};
            const double dReal{nowReal - bgRateMarkRealTime};
            if (dReal >= 0.5) {
               measuredBgHz = (dReal > 1e-6) ? (static_cast<double>(bgRateMarkCount) / dReal) : 0.0;
               bgRateMarkRealTime = nowReal;
               bgRateMarkCount = 0;
            }
         }

         next.background.targetHz = bgRate;
         next.background.measuredHz = measuredBgHz;
         next.background.iterationCount = frameCount;
         next.background.lastIterationMs = (mixr::base::getComputerTime() - iterationStart) * 1000.0;
         next.background.tacviewEnabled = (tacviewOutput != nullptr);
         next.background.radarScanPushCount = radarScanPushCount;

         // Checagem do breakpoint -- ver app/BreakpointController.hpp.
         // Roda toda amostra, mesmo fora do modo rapido: "a velocidade que
         // eu decidir" tambem tem de parar sozinha quando o no e atingido.
         {
            const std::lock_guard<std::mutex> lock(w.bpMutex);
            std::vector<BreakpointEntity> bpEntities;
            bpEntities.reserve(next.entities.size());
            for (const auto& e : next.entities) bpEntities.push_back({e.id, e.behaviorLabel});

            const BreakpointTickResult result{w.bp.tick(bpEntities, matchesLabel, next.simSec)};
            if (result.outcome != BreakpointOutcome::None) {
               if (clockStation != nullptr) {
                  if (result.shouldRestoreScale) clockStation->setTimeScale(w.bp.restoreTimeScale());
                  if (result.shouldPause) clockStation->setPaused(true);
               }
               w.fastRunToBreakpoint = false;
            }

            // Publica pro DashboardState -- ver o "porque" no cabecalho de
            // BreakpointController.hpp.
            next.breakpointArmed = w.bp.isArmed();
            const BreakpointStatus globalBpStatus{w.bp.status(false, false, "")};
            next.breakpointHit = (globalBpStatus.branch == BreakpointStatusBranch::Hit);
            next.breakpointHitMessage = globalBpStatus.text;
         }

         next.actualTimeScale = measuredActualTimeScale;
         next.fastBreakpointRun = w.fastRunToBreakpoint.load();

         {
            const std::lock_guard<std::mutex> lock(stateMutex);
            latest = next;
         }
         screen.PostEvent(Event::Custom);

         // Modo rapido: pula o msleep() de pacing, deixa o laco girar o
         // mais rapido que a CPU permitir. Ao SAIR do modo (breakpoint
         // atingido, cancelado ou nunca ligado), resincroniza a referencia
         // de parede.
         const bool fastNow{w.fastRunToBreakpoint.load()};
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

   // Camada de confirmacao -- 'uiDepth' 0 = UI normal, 1 = dialogo de
   // confirmacao por cima. E o MESMO padrao do exemplo oficial
   // modal_dialog_custom.cpp do FTXUI.
   int uiDepth{};
   PendingAction pendingAction{PendingAction::None};

   int activeTab{};

   // Rastro do Mapa -- so acrescenta amostra nova (ver o Renderer mais
   // externo, mais abaixo); -1.0 garante que a PRIMEIRA amostra (simSec
   // tipicamente 0.0) sempre conte como "nova".
   double lastTrailSimSec{-1.0};

   // ---- acoes nomeadas: cada uma e usada por TECLA e por BOTAO ----
   //
   // Acelerar/frear/voltar-a-tempo-real MANUAIS ficam BLOQUEADOS enquanto
   // HOUVER um breakpoint armado -- pedido explicito, e vale nos DOIS modos
   // ('g': velocidade atual; 'G': velocidade maxima), nao so no rapido.
   const auto isBreakpointArmedNow = [&] {
      const std::lock_guard<std::mutex> lock(w.bpMutex);
      return w.bp.isArmed();
   };
   // Nenhuma destas acoes chama LOG(...): o app e LEITOR do log, nao
   // produtor.
   const auto doAccelerate = [&] {
      if (clockStation == nullptr || isBreakpointArmedNow()) return;
      if (w.ladder.accelerate()) clockStation->setTimeScale(w.ladder.scale());
   };
   const auto doDecelerate = [&] {
      if (clockStation == nullptr || isBreakpointArmedNow()) return;
      if (w.ladder.decelerate()) clockStation->setTimeScale(w.ladder.scale());
   };
   const auto doTogglePause = [&] { if (clockStation != nullptr) clockStation->togglePaused(); };
   const auto doRealTime = [&] {
      if (clockStation == nullptr || isBreakpointArmedNow()) return;
      w.ladder.toRealTime();
      clockStation->setPaused(false);
      clockStation->setTimeScale(w.ladder.scale());
   };
   // As duas versoes de VERDADE (o que 'r'/'q' faziam direto antes) -- agora
   // so rodam depois de confirmadas (ver 'confirmDialog' mais abaixo).
   const auto doRestartConfirmed = [&] { w.action = DashboardExit::Restart; screen.Exit(); };
   const auto doQuitConfirmed = [&] { w.action = DashboardExit::Quit; screen.Exit(); };

   const auto runPendingAction = [&] {
      switch (pendingAction) {
         case PendingAction::Restart: doRestartConfirmed(); break;
         case PendingAction::Quit:    doQuitConfirmed(); break;
         default: break;
      }
   };
   const auto cancelPendingAction = [&] { pendingAction = PendingAction::None; uiDepth = 0; };

   // 'dragging' so era desarmado dentro do bloco 'activeTab==1/5', quando um
   // 'Mouse::Released' chegava com a aba ainda ativa. Trocar de aba ou armar
   // o dialogo de confirmacao nunca passa por ali, entao um arrasto em
   // andamento ficava 'dragging=true' permanentemente.
   const auto cancelAnyDrag = [&] { w.mapView.dragging = false; w.componentsView.dragging = false; };

   // As duas que TECLA/BOTAO chamam de verdade -- so ARMAM o dialogo,
   // pedido explicito de confirmacao pras duas acoes disruptivas.
   const auto doRestart = [&] { cancelAnyDrag(); pendingAction = PendingAction::Restart; uiDepth = 1; };
   const auto doQuit = [&] { cancelAnyDrag(); pendingAction = PendingAction::Quit; uiDepth = 1; };
   const auto gotoTab = [&](const int index) { cancelAnyDrag(); activeTab = index; };

   // "Ver no mapa" -- so faz sentido no card da Frota (no Mapa voce ja esta
   // la). Como Frota e Mapa ja COMPARTILHAM 'w.selectedEntityIndex', trocar
   // de aba e o suficiente.
   const auto doViewOnMap = [&] { gotoTab(1); };
   const Component btnViewOnMap{makeButton("[m] Ver no mapa", doViewOnMap)};

   // ---- monta as sete abas -- ORDEM IMPORTA: Fleet antes de Map (ver o
   // comentario em app/DashboardWiring.hpp) ----
   const Component fleetTab{buildFleetTab(w)};
   const Component mapTab{buildMapTab(w)};
   const Component memoryTab{buildMemoryTab(w)};
   const Component backgroundTab{buildBackgroundTab(w)};
   const Component logTab{buildLogTab(w)};
   const Component componentsTab{buildComponentsTab(w)};
   const Component edlTab{buildEdlTab(w)};

   const Component contentTab{Container::Tab(
      {fleetTab, mapTab, memoryTab, backgroundTab, logTab, componentsTab, edlTab}, &activeTab)};

   // ---- barra de abas e barra de acoes, TODAS clicaveis (Button de
   // verdade), com a dica de atalho ja no rotulo ----

   const Component btnFleet{makeButton("[F1] Players", [&] { gotoTab(0); })};
   const Component btnMap{makeButton("[F2] Mapa", [&] { gotoTab(1); })};
   const Component btnMemory{makeButton("[F3] Memoria", [&] { gotoTab(2); })};
   const Component btnBackground{makeButton("[F4] Tempo Nao-Critico", [&] { gotoTab(3); })};
   const Component btnLog{makeButton("[F5] Log", [&] { gotoTab(4); })};
   const Component btnComponents{makeButton("[F6] Componentes", [&] { gotoTab(5); })};
   const Component btnEdl{makeButton("[F7] EDL", [&] { gotoTab(6); })};

   // Acelerar/Frear/Tempo-real ficam visualmente apagados enquanto
   // bloqueados -- QUALQUER breakpoint armado, nao so o modo rapido.
   ButtonOption accelOpt;
   accelOpt.label = "[+] Acelerar";
   accelOpt.on_click = doAccelerate;
   accelOpt.transform = [&](const EntryState&) {
      return text(" [+] Acelerar ") | (w.displayedBreakpointArmed ? dim : nothing);
   };
   const Component btnAccel{Button(accelOpt)};

   ButtonOption decelOpt;
   decelOpt.label = "[-] Frear";
   decelOpt.on_click = doDecelerate;
   decelOpt.transform = [&](const EntryState&) {
      return text(" [-] Frear ") | (w.displayedBreakpointArmed ? dim : nothing);
   };
   const Component btnDecel{Button(decelOpt)};

   const Component btnPause{makeButton("[espaco] Pausar", doTogglePause)};

   ButtonOption realOpt;
   realOpt.label = "[1] Tempo real";
   realOpt.on_click = doRealTime;
   realOpt.transform = [&](const EntryState&) {
      return text(" [1] Tempo real ") | (w.displayedBreakpointArmed ? dim : nothing);
   };
   const Component btnReal{Button(realOpt)};
   const Component btnRestart{makeButton("[r] Reiniciar", doRestart)};
   const Component btnQuit{makeButton("[q] Sair", doQuit)};

   const Component toolbar{Container::Horizontal({
      btnFleet, btnMap, btnMemory, btnBackground, btnLog, btnComponents, btnEdl,
      btnAccel, btnDecel, btnPause, btnReal, btnViewOnMap,
      btnRestart, btnQuit,
   })};

   const Component breakpointBar{Container::Horizontal({
      w.btnRunToBreakpoint, w.btnRunToBreakpointMax, w.btnCancelBreakpoint,
   })};

   const Component root{Container::Vertical({toolbar, breakpointBar, contentTab})};

   const Component withRenderer{Renderer(root, [&]() -> Element {
      DashboardState snap;
      {
         const std::lock_guard<std::mutex> lock(stateMutex);
         snap = latest;
      }

      w.displayedEntities = snap.entities;
      w.displayedClasses = snap.classStats;
      w.displayedBackground = snap.background;
      w.displayedBreakpointArmed = snap.breakpointArmed;
      w.displayedBreakpointHit = snap.breakpointHit;
      w.displayedBreakpointHitMessage = snap.breakpointHitMessage;

      // So acrescenta ao rastro quando a amostra e REALMENTE nova -- este
      // Renderer roda a cada redesenho, nao so a cada captura nova (uma
      // tecla ou um resize tambem disparam render).
      if (snap.simSec != lastTrailSimSec) {
         updateTrails(w.mapView, w.displayedEntities);
         lastTrailSimSec = snap.simSec;
      }

      w.entityLabels.clear();
      for (const auto& e : w.displayedEntities) w.entityLabels.push_back(entityRowText(e));
      w.classLabels.clear();
      for (const auto& c : w.displayedClasses) w.classLabels.push_back(classRowText(c));

      if (!w.displayedEntities.empty()) {
         w.selectedEntityIndex = std::clamp(w.selectedEntityIndex, 0,
            static_cast<int>(w.displayedEntities.size()) - 1);
      }
      if (!w.displayedClasses.empty()) {
         w.selectedClassIndex = std::clamp(w.selectedClassIndex, 0,
            static_cast<int>(w.displayedClasses.size()) - 1);
      }

      // Aba Log -- so recopia do buffer quando ha linha nova (lastSeq
      // mudou) ou quando o filtro mudou.
      {
         const std::uint64_t seqNow{mixr::xlog::lastSeq()};
         if (seqNow != w.lastLogSeq || w.logMinLevel != w.lastLogFilter) {
            w.lastLogSeq = seqNow;
            w.lastLogFilter = w.logMinLevel;
            w.displayedLogs.clear();
            w.logLabels.clear();
            for (const auto& e : mixr::xlog::snapshot()) {
               if (!passesLevelFilter(e.level, w.logMinLevel)) continue;
               w.displayedLogs.push_back(e);
               w.logLabels.push_back(logRowText(e));
            }
            if (w.logFollowTail && !w.displayedLogs.empty()) {
               w.selectedLogIndex = static_cast<int>(w.displayedLogs.size()) - 1;
            }
         }
      }
      w.selectedLogIndex = w.displayedLogs.empty()
         ? 0
         : std::clamp(w.selectedLogIndex, 0, static_cast<int>(w.displayedLogs.size()) - 1);
      if (!w.treeLines.empty()) {
         w.selectedBtLineIndex = std::clamp(w.selectedBtLineIndex, 0,
            static_cast<int>(w.treeLines.size()) - 1);
      }

      // Aba Componentes -- a arvore e recapturada a cada amostra (10 Hz).
      // Gateado por 'activeTab==5'.
      wantComponentTree.store(activeTab == 5, std::memory_order_relaxed);
      if (activeTab == 5) {
         w.componentsRoot = snap.componentTree;

         // A arvore nasce EXPANDIDA so ate kTreeInitialExpandDepth. Uma vez
         // so -- depois disso quem manda e o usuario, e reabrir tudo e [o].
         if (!w.componentsAutoFitted && !w.componentsRoot.children.empty()) {
            collapseDeeperThan(w.componentsRoot, kTreeInitialExpandDepth, w.componentsCollapsed);
         }

         w.componentsLayout = layoutComponentTree(w.componentsRoot, w.componentsCollapsed);
      }

      // Relogio da animacao de fluxo (SEGUNDA METADE) -- avanca aqui, no
      // MESMO Renderer mais externo que ja roda a cada redesenho. A
      // animacao e ESCRAVA da simulacao.
      setComponentFlowPlaying(w.componentsFlow, !snap.paused);
      tickComponentFlowAnimation(w.componentsFlow);

      // De 'snap', nao de 'station->': a thread de desenho nao le mais nada
      // do grafo vivo do MIXR.
      w.frameCallParams.tcRateHz = snap.background.stationTcRateHz;
      w.frameCallParams.fastForwardRate = snap.background.fastForwardRate;
      w.frameCallParams.numTcThreads = snap.numTcThreads;
      w.frameCallParams.paused = snap.paused;

      // Recalculado a cada redesenho -- o terminal pode ser redimensionado
      // em qualquer frame. "+6" cobre a borda do canvas (2) + separador (1)
      // + borda do proprio card (2) + uma folga de 1.
      w.detailPanelWidth = std::clamp(Terminal::Size().dimx - (kMapCanvasWidthCells + 6),
                                    kDetailPanelMinWidth, kDetailPanelMaxWidth);

      const auto tabBadge = [&](const Component& btn, const int index) -> Element {
         Element e{btn->Render()};
         if (activeTab == index) return e | bgcolor(Color::Blue) | bold;
         return e | dim;
      };

      // "[m] Ver no mapa" -- so aparece na aba Players (F1) COM uma
      // entidade selecionada.
      Elements primaryButtons{btnAccel->Render(), btnDecel->Render(), btnPause->Render(),
                              btnReal->Render()};
      if (activeTab == 0 && !w.displayedEntities.empty()) primaryButtons.push_back(btnViewOnMap->Render());

      // "Informe" de BP atingido -- fica visivel ate o usuario armar um
      // NOVO breakpoint.
      Elements rows{renderHeader(snap)};
      if (w.displayedBreakpointHit) {
         rows.push_back(text(" BP ATINGIDO -- " + w.displayedBreakpointHitMessage + " ")
                         | bold | bgcolor(Color::Blue) | color(Color::White) | center);
      }
      rows.push_back(hbox({tabBadge(btnFleet, 0), tabBadge(btnMap, 1), tabBadge(btnMemory, 2),
               tabBadge(btnBackground, 3), tabBadge(btnLog, 4), tabBadge(btnComponents, 5),
               tabBadge(btnEdl, 6)}));
      rows.push_back(separator());
      rows.push_back(contentTab->Render() | flex);
      rows.push_back(separator());
      rows.push_back(hbox({
         hbox(std::move(primaryButtons)),
         filler(),
         btnRestart->Render(), btnQuit->Render(),
      }));

      return vbox(std::move(rows));
   })};

   const Component withKeys{CatchEvent(withRenderer, [&](Event event) -> bool {
      // Troca de aba (F1..F7) vem PRIMEIRO de tudo, incondicional.
      if (event == Event::F1) { gotoTab(0); return true; }
      if (event == Event::F2) { gotoTab(1); return true; }
      if (event == Event::F3) { gotoTab(2); return true; }
      if (event == Event::F4) { gotoTab(3); return true; }
      if (event == Event::F5) { gotoTab(4); return true; }
      if (event == Event::F6) { gotoTab(5); return true; }
      if (event == Event::F7) { gotoTab(6); return true; }

      // A aba "EDL" precisa de TODA tecla que nao seja troca de aba -- o
      // ftxui::Input multilinha (w.edlInput) e quem trata cursor/insercao/
      // backspace/Enter.
      if (activeTab == 6) {
         if (event == Event::F8) { w.doEdlValidate(); return true; }
         if (event == Event::F9) { w.doEdlRun(); return true; }
         if (event == Event::F10) { w.doEdlRevert(); return true; }

         // Rolagem de mouse rola o TEXTO -- o ftxui::Input nao trata roda
         // nenhuma sozinho.
         if (event.is_mouse() && event.mouse().button == Mouse::WheelDown) {
            for (int i = 0; i < 3; i++) w.edlInput->OnEvent(Event::ArrowDown);
            return true;
         }
         if (event.is_mouse() && event.mouse().button == Mouse::WheelUp) {
            for (int i = 0; i < 3; i++) w.edlInput->OnEvent(Event::ArrowUp);
            return true;
         }
         return false;
      }

      // Espaco/[n] da aba "Componentes" (F6) tem de ser tratado ANTES do
      // espaco GLOBAL (pausa a simulacao, logo abaixo).
      if (activeTab == 5) {
         if (event == Event::Character(' ')) { w.doCompTogglePlay(); return true; }
         if (event == Event::Character('n') || event == Event::Character('N')) { w.doCompStep(); return true; }
         if (event == Event::Character('v') || event == Event::Character('V')) { w.doCompCycleSpeed(); return true; }

         // Retrair/expandir.
         if (event == Event::Return) { w.doCompToggleCollapse(); return true; }
         if (event == Event::Character('o') || event == Event::Character('O')) { w.doCompExpandAll(); return true; }
         if (event == Event::Character('f') || event == Event::Character('F')) { w.doCompCollapseAll(); return true; }

         // Setas NAVEGAM entre os nos (antes moviam o pan).
         if (event == Event::ArrowUp)    { w.doCompNavigate(TreeNavigation::Parent); return true; }
         if (event == Event::ArrowDown)  { w.doCompNavigate(TreeNavigation::FirstChild); return true; }
         if (event == Event::ArrowLeft)  { w.doCompNavigate(TreeNavigation::PrevSibling); return true; }
         if (event == Event::ArrowRight) { w.doCompNavigate(TreeNavigation::NextSibling); return true; }
      }
      if (event == Event::Character('+') || event == Event::Character('=')) { doAccelerate(); return true; }
      if (event == Event::Character('-') || event == Event::Character('_')) { doDecelerate(); return true; }
      if (event == Event::Character(' ') || event == Event::Character('p') ||
          event == Event::Character('P')) { doTogglePause(); return true; }
      if (event == Event::Character('1')) { doRealTime(); return true; }

      // Navegacao por seta das listas (Players/Memoria) -- tratada AQUI, no
      // CatchEvent mais externo.
      if (activeTab == 0 && !w.displayedEntities.empty()) {
         const int last{static_cast<int>(w.displayedEntities.size()) - 1};
         if (event == Event::ArrowDown) { w.selectedEntityIndex = std::min(w.selectedEntityIndex + 1, last); return true; }
         if (event == Event::ArrowUp)   { w.selectedEntityIndex = std::max(w.selectedEntityIndex - 1, 0); return true; }
         if (event == Event::Character('m') || event == Event::Character('M')) { doViewOnMap(); return true; }
      }
      if (activeTab == 2 && !w.displayedClasses.empty()) {
         const int last{static_cast<int>(w.displayedClasses.size()) - 1};
         if (event == Event::ArrowDown) { w.selectedClassIndex = std::min(w.selectedClassIndex + 1, last); return true; }
         if (event == Event::ArrowUp)   { w.selectedClassIndex = std::max(w.selectedClassIndex - 1, 0); return true; }
      }
      if (activeTab == 4) {
         // Rolar pra CIMA desliga o "acompanhar"; chegar de volta no fim
         // religa.
         if (event == Event::ArrowUp) {
            w.logFollowTail = false;
            w.selectedLogIndex = std::max(w.selectedLogIndex - 1, 0);
            return true;
         }
         if (event == Event::ArrowDown) {
            const int last{std::max(0, static_cast<int>(w.displayedLogs.size()) - 1)};
            w.selectedLogIndex = std::min(w.selectedLogIndex + 1, last);
            if (w.selectedLogIndex == last) w.logFollowTail = true;
            return true;
         }
         if (event == Event::Character('f') || event == Event::Character('F')) {
            w.doCycleLogFilter();
            return true;
         }
         if (event == Event::Character('a') || event == Event::Character('A')) {
            w.doToggleLogFollow();
            return true;
         }
      }

      // Breakpoint de BT -- GLOBAL (nao depende de aba).
      if (event == Event::Character('g')) { w.doArmBreakpoint(false); return true; }
      if (event == Event::Character('G')) { w.doArmBreakpoint(true); return true; }
      if (event == Event::Character('x') || event == Event::Character('X')) { w.doCancelBreakpoint(); return true; }

      // As duas pedem CONFIRMACAO agora (ver 'confirmDialog' mais abaixo) --
      // 'Escape' saiu daqui de proposito: dentro do dialogo ele significa
      // "cancelar", e mante-lo tambem como atalho de 'q' aqui geraria o
      // efeito estranho de Escape armar a confirmacao de sair e o Escape
      // SEGUINTE cancelar ela na hora.
      if (event == Event::Character('r') || event == Event::Character('R')) { doRestart(); return true; }
      if (event == Event::Character('q') || event == Event::Character('Q')) { doQuit(); return true; }

      // Interacao do MAPA -- so quando a aba esta ativa, tratada aqui (nao
      // num CatchEvent aninhado em 'mapTab').
      if (activeTab == 1) {
         if (event.is_mouse()) {
            const Mouse& m{event.mouse()};
            const bool insideCanvas{w.mapCanvasBox.Contain(m.x, m.y)};

            // Um arrasto EM ANDAMENTO processa ate soltar, mesmo que o
            // mouse escape do canvas por um instante (movimento rapido) --
            // so o COMECO (Pressed) e a roda exigem estar dentro do canvas.
            if (w.mapView.dragging) {
               if (m.motion == Mouse::Released) {
                  w.mapView.dragging = false;
                  const int totalMove{std::abs(m.x - w.mapView.pressX) + std::abs(m.y - w.mapView.pressY)};
                  if (totalMove <= 1) {
                     // Deslocamento minimo entre Pressed e Released: e um
                     // CLIQUE, nao um arrasto.
                     const int cellX{m.x - w.mapCanvasBox.x_min};
                     const int cellY{m.y - w.mapCanvasBox.y_min};
                     const int hitId{hitTestEntity(w.displayedEntities, w.mapView, cellX, cellY)};
                     if (hitId >= 0) {
                        for (std::size_t i = 0; i < w.displayedEntities.size(); i++) {
                           if (w.displayedEntities[i].id == hitId) {
                              w.selectedEntityIndex = static_cast<int>(i);
                              break;
                           }
                        }
                     }
                  }
                  return true;
               }
               if (m.motion == Mouse::Moved) {
                  const int dx{m.x - w.mapView.dragLastX};
                  const int dy{m.y - w.mapView.dragLastY};
                  w.mapView.dragLastX = m.x;
                  w.mapView.dragLastY = m.y;
                  // Arrastar pra direita/cima deve fazer o CONTEUDO seguir o
                  // cursor.
                  panMap(w.mapView, -dx * w.mapView.metersPerCell * 2.0, dy * w.mapView.metersPerCell * 4.0);
                  return true;
               }
               return true;
            }

            if (!insideCanvas) return false;
            if (m.button == Mouse::WheelUp) { w.doMapZoomIn(); return true; }
            if (m.button == Mouse::WheelDown) { w.doMapZoomOut(); return true; }
            if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
               w.mapView.dragging = true;
               w.mapView.pressX = m.x;
               w.mapView.pressY = m.y;
               w.mapView.dragLastX = m.x;
               w.mapView.dragLastY = m.y;
               return true;
            }
            return false;
         }

         if (event == Event::ArrowLeft)  { panMap(w.mapView, -w.mapView.metersPerCell * 4.0, 0.0); return true; }
         if (event == Event::ArrowRight) { panMap(w.mapView,  w.mapView.metersPerCell * 4.0, 0.0); return true; }
         if (event == Event::ArrowUp)    { panMap(w.mapView, 0.0,  w.mapView.metersPerCell * 4.0); return true; }
         if (event == Event::ArrowDown)  { panMap(w.mapView, 0.0, -w.mapView.metersPerCell * 4.0); return true; }
         if (event == Event::Character('[')) { w.doMapZoomOut(); return true; }
         if (event == Event::Character(']')) { w.doMapZoomIn(); return true; }
         if (event == Event::Character(',')) { w.doMapRotateLeft(); return true; }
         if (event == Event::Character('.')) { w.doMapRotateRight(); return true; }
         if (event == Event::Character('t') || event == Event::Character('T')) { w.doMapToggleTrails(); return true; }
         if (event == Event::Character('e') || event == Event::Character('E')) { w.doMapToggleTerrain(); return true; }
         if (event == Event::Character('v') || event == Event::Character('V')) { w.doMapTogglePerspective(); return true; }
         if (event == Event::Character('c') || event == Event::Character('C')) { w.doMapCenterOnSelected(); return true; }
         // 'f' de "follow" -- mesmo padrao de 't' (trail) e 'e' (elevation).
         if (event == Event::Character('f') || event == Event::Character('F')) { w.doMapToggleFollow(); return true; }
      }

      // Interacao da aba "Componentes" -- MESMO raciocinio/estrutura do
      // bloco do Mapa logo acima.
      if (activeTab == 5) {
         if (event.is_mouse()) {
            const Mouse& m{event.mouse()};
            const bool insideCanvas{w.componentsCanvasBox.Contain(m.x, m.y)};

            if (w.componentsView.dragging) {
               if (m.motion == Mouse::Released) {
                  w.componentsView.dragging = false;
                  const int totalMove{std::abs(m.x - w.componentsView.pressX)
                                      + std::abs(m.y - w.componentsView.pressY)};
                  if (totalMove <= 1) {
                     const int cellX{m.x - w.componentsCanvasBox.x_min};
                     const int cellY{m.y - w.componentsCanvasBox.y_min};
                     const int hitIndex{hitTestComponentTreeNode(w.componentsLayout, w.componentsView,
                                                                 cellX, cellY)};
                     if (hitIndex >= 0) {
                        w.componentsView.selectedKey =
                           w.componentsLayout.nodes[static_cast<std::size_t>(hitIndex)].nodeKey;
                     }
                  }
                  return true;
               }
               if (m.motion == Mouse::Moved) {
                  const int dx{m.x - w.componentsView.dragLastX};
                  const int dy{m.y - w.componentsView.dragLastY};
                  w.componentsView.dragLastX = m.x;
                  w.componentsView.dragLastY = m.y;
                  panComponentTree(w.componentsView, dx * 2.0, dy * 4.0);
                  return true;
               }
               return true;
            }

            if (!insideCanvas) return false;
            if (m.button == Mouse::WheelUp) { w.doCompZoomIn(); return true; }
            if (m.button == Mouse::WheelDown) { w.doCompZoomOut(); return true; }
            if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
               w.componentsView.dragging = true;
               w.componentsView.pressX = m.x;
               w.componentsView.pressY = m.y;
               w.componentsView.dragLastX = m.x;
               w.componentsView.dragLastY = m.y;
               return true;
            }
            return false;
         }

         // As setas nao chegam aqui -- sao tratadas no bloco de
         // 'activeTab == 5' la em cima, onde passaram a NAVEGAR entre os
         // nos em vez de mover o pan.
         if (event == Event::Character('[')) { w.doCompZoomOut(); return true; }
         if (event == Event::Character(']')) { w.doCompZoomIn(); return true; }
         if (event == Event::Character('c') || event == Event::Character('C')) {
            w.doCompCenterOnSelected(); return true;
         }
      }

      return false;
   })};

   // ---- dialogo de confirmacao (reiniciar/sair) --
   // MESMO padrao do exemplo oficial modal_dialog_custom.cpp do FTXUI.
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
      // ForCa o foco de TECLADO em 'w.edlInput' toda vez que a aba "EDL"
      // esta em cena, com o dialogo de confirmacao FECHADO.
      if (activeTab == 6 && uiDepth == 0) w.edlInput->TakeFocus();

      Element doc{withKeys->Render()};
      if (uiDepth == 1) {
         doc = dbox({doc, confirmDialog->Render() | clear_under | center});
      }
      return doc;
   })};

   // Ctrl+C: o FTXUI ja instala o proprio handler e sai do Loop() sozinho
   // (App::ForceHandleCtrlC(true) e o default) -- 'w.action' fica em Quit,
   // que e exatamente o que se quer.
   //
   // BARREIRA DE EXCECAO -- por que 'catch (...)' e nao 'catch (std::exception&)':
   // o MIXR sinaliza erro de contagem de referencia lancando um PONTEIRO que
   // NAO deriva de std::exception. Sem catch nenhum qualquer throw vindo do
   // laco da interface vira std::terminate -> SIGABRT, "core dumped" sem
   // uma linha de explicacao.
   //
   // NAO se tenta retomar o Loop(): o throw de ref() acontece ANTES do
   // unlock(semaphore), deixando o spin lock daquele objeto travado para
   // sempre. Depois disto o processo so pode encerrar -- mas encerrar
   // LIMPO, pelo mesmo caminho do 'q'.
   //
   // std::fputs em stderr, nao LOG(): o log toma um mutex global que pode
   // ser justamente o que ficou preso.
   try {
      screen.Loop(appRoot);
   } catch (...) {
      std::fputs("[app] excecao escapou do laco da interface -- encerrando de "
                 "forma limpa (ver a barreira em app/DashboardLoop.cpp)\n", stderr);
      w.action = DashboardExit::Quit;
   }

   // ---- ENCERRAMENTO, e a ORDEM aqui e o conserto (ver app/Shutdown.hpp) ----
   //
   // 1) Cala a PRODUTORA primeiro. A thread T/C nativa criada la em cima nao
   //    morre com o fim do Loop() -- ela sobrevive a esta funcao inteira e ao
   //    SHUTDOWN_EVENT do main.cpp.
   quiesceTimeCritical(station, clockStation);

   // 2) So agora para a CONSUMIDORA. Nesta ordem 'simThread' nao pode mais
   //    ser surpreendida por trabalho novo entrando na fila.
   running = false;
   simThread.join();

   // 3) Ultima drenagem, com a producao ja parada -- fecha a fila numa
   //    passada e deixa o DataRecorder::shutdownNotification() do
   //    SHUTDOWN_EVENT com quase nada para fazer.
   station->updateData(1.0 / static_cast<double>(bgRate));

   // Terminal de volta pro dono anterior -- ver setConsoleEnabled(false) no
   // inicio desta funcao.
   mixr::xlog::setConsoleEnabled(true);

   return w.action;
}

} // namespace app
