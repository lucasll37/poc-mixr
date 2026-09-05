#include "app/DashboardLoop.hpp"

#include "mixr/linkage/IoHandler.hpp"

#include "app/BackgroundPanel.hpp"
#include "app/BehaviorTreeView.hpp"
#include "app/BreakpointController.hpp"
#include "app/ComponentTreePanel.hpp"
#include "app/DashboardState.hpp"
#include "app/Fleet.hpp"
#include "app/FleetPanel.hpp"
#include "app/LogPanel.hpp"
#include "app/MapPanel.hpp"
#include "app/MemoryPanel.hpp"
#include "app/Shutdown.hpp"
#include "app/SpeedLadder.hpp"

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
// CINCO ABAS (ftxui::Container::Tab) -- Players/Mapa/Memoria/Log sao cada uma
// um ftxui::Menu dentro de frame()/vscroll_indicator() (padrao oficial do
// FTXUI para lista rolavel -- ver o exemplo menu_in_frame.cpp da propria
// lib): e o que deixa a UI caber QUALQUER quantidade de entidades/classes
// sem crescer a tela. Tempo Nao-Critico e um painel estatico -- ver
// app/BackgroundPanel.hpp. A aba Log le o buffer em memoria de
// shared/xlog (ver app/LogPanel.hpp).
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

// Breakpoint de arvore de comportamento e escada de velocidade -- a
// DECISAO de cada um mora em app/BreakpointController.hpp e
// app/SpeedLadder.hpp (sem FTXUI/MIXR, testada em tests/app/), separada
// do wiring com mutex/ClockStation que continua aqui. Ver o "porque" nos
// dois headers.

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
                           mixr::linkage::IoHandler* const ioHandler,
                           const int numTcThreads, const std::string& scenarioLabel,
                           const BtNode& behaviorTree)
{
   std::mutex stateMutex;
   DashboardState latest;
   std::atomic<bool> running{true};

   // Estado do breakpoint (ver app/BreakpointController.hpp) --
   // 'fastRunToBreakpoint' e atomico A PARTE (lido a cada iteracao do laco
   // de 'simThread', sem tomar 'bpMutex' so pra isso) porque decide se a
   // iteracao PULA o msleep() de pacing -- caminho quente, sem alocacao.
   std::mutex bpMutex;
   BreakpointController bp;
   std::atomic<bool> fastRunToBreakpoint{false};

   // Pedidos de PASSO de simulacao pendentes -- o "[n] Passo" da aba F6.
   // Cada um vale UM Station::tcFrame(dt) de verdade, executado la em
   // 'simThread' (nunca aqui, na thread de desenho): a thread T/C nativa
   // continua viva, e so nao esta chamando tcFrame() porque
   // ClockStation::processTimeCriticalTasks() retorna cedo quando pausado
   // (ver o comentario grande la). Dar o passo com a simulacao RODANDO seria
   // duas threads dentro do mesmo frame -- por isso o passo pausa antes.
   std::atomic<int> stepFrameRequests{0};

   // A partir daqui o FTXUI e dono do terminal (alternate screen buffer,
   // modo bruto) -- uma linha de log escrita direto em std::cout suja o
   // desenho, e o FTXUI nao sabe que alguem escreveu por baixo dele pra
   // redesenhar aquela regiao. Desliga SO o console: arquivo
   // (./app/data/logs/app.log) e buffer em memoria continuam, e e do
   // buffer que a aba Log le. Religado no fim desta funcao, antes de
   // devolver o controle ao main.cpp (que pode reexecutar o processo ou
   // imprimir no terminal ja restaurado).
   mixr::xlog::setConsoleEnabled(false);

   auto screen = ScreenInteractive::Fullscreen();

   // Liga a medicao de duracao do frame de tempo critico (aba F4). Uma vez
   // so, antes do laco. NUNCA setPrintTimingStats(true): esse flag faz
   // Component::printTimingStats() escrever direto em std::cout, e o FTXUI e
   // dono do terminal -- exatamente o motivo de runDashboard() ja ter
   // desligado o console do xlog logo acima.
   station->setTimingStatsEnabled(true);

   std::thread simThread([&] {
      station->createTimeCriticalProcess();
      mixr::base::msleep(settleMs);

      const double dt{1.0 / static_cast<double>(bgRate)};
      double wallTimeElapsed{};
      double startTime{mixr::base::getComputerTime()};
      long frameCount{};

      // Referencia de PAREDE de verdade para "t=" no cabecalho -- NUNCA
      // resetada (ao contrario de 'startTime', que reancora a cada troca
      // fast<->normal so para o pacing). 'frameCount * dt' parecia
      // equivalente enquanto o pacing mantinha uma iteracao por 'dt' de
      // parede, mas no modo rapido ('fastRunToBreakpoint') o laco pula o
      // msleep() e gira muito mais que 1/dt vezes por segundo -- contando
      // iteracoes * dt, "t=" (rotulado "tempo real" no cabecalho) acelerava
      // junto com a simulacao, o que nao faz sentido: o relogio de PAREDE
      // nao pode correr mais rapido so porque a simulacao esta em MAX.
      // Medindo o tempo de parede de verdade, "t=" sempre anda a 1x, rodando
      // ou em MAX -- e continua sendo a base contra a qual "sim=" se compara.
      const double realWallClockStart{mixr::base::getComputerTime()};
      std::vector<ClassStat> classHistory;
      bool wasFast{};

      // Velocidade FACTUAL (medida) -- tempo simulado / tempo de PAREDE de
      // verdade, numa janela deslizante de ~0.5s. Ao contrario de
      // 'timeScale' (o valor NOMINAL da escada de xclock), isto continua
      // significando alguma coisa durante um breakpoint em velocidade
      // maxima, onde o laco ignora o pacing por completo -- ver o pedido
      // "o valor da aceleracao no cabecalho deve refletir o factual da
      // simulacao".
      double speedMarkRealTime{mixr::base::getComputerTime()};
      double speedMarkSimSec{0.0};
      double measuredActualTimeScale{1.0};

      // Ver o comentario grande de app::BackgroundInfo (DashboardState.hpp):
      // este laco INTEIRO e a "thread de tempo nao critico" da aba Tempo Nao-Critico --
      // 'bgRateMark*' mede a taxa REAL de iteracao (janela de ~0.5s, mesmo
      // desenho de 'speedMark*' acima), separada da taxa SIMULADA que
      // 'measuredActualTimeScale' ja cobre.
      double bgRateMarkRealTime{mixr::base::getComputerTime()};
      long bgRateMarkCount{};
      double measuredBgHz{static_cast<double>(bgRate)};
      long radarScanPushCount{};

      while (running.load()) {
         const double iterationStart{mixr::base::getComputerTime()};

         // Identidade real de cada player (tipo/lado/major type) ANTES de
         // drenar o gravador -- e o updateData() abaixo que DECLARA cada
         // objeto no stream ACMI, e Name/Type/Color so vao na primeira
         // aparicao de cada id. Sem isto, o que nasce em runtime (o missil
         // liberado, cujo nome automatico "W10001" nao esta em mapa
         // nenhum) ia pro Tacview como "Misc"/"Grey". Ver o cabecalho de
         // TacviewOutput::publishIdentities().
         if (tacviewOutput != nullptr) tacviewOutput->publishIdentities(worldModel);

         // Passo manual pedido pela aba F6 -- ANTES do updateData() para que
         // a passada de fundo que vem a seguir ja veja o estado novo (e
         // drene o gravador, alimentando o Tacview com o frame recem-dado).
         if (const int steps{stepFrameRequests.exchange(0)}; steps > 0) {
            const double tcRate{station->getTimeCriticalRate()};
            if (tcRate > 0.0 && clockStation != nullptr && clockStation->isPaused()) {
               const double tcDt{1.0 / tcRate};
               for (int i = 0; i < steps; i++) station->tcFrame(tcDt);
            }
         }

         // Joystick (so o cenario 'bandit' declara um 'ioHandler:'): mesma
         // taxa e mesmo lugar do laco de tempo real que as pocs usavam
         // antes de o ./app virar o runner unico delas -- 10 Hz, fora do
         // frame de tempo critico. Sem hardware conectado o
         // JoystickIoHandler nao toca em nada e o Autopilot segue no
         // controle (ver a armadilha 7 de shared/xjoystick no CLAUDE.md).
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
                                          numTcThreads, scenarioLabel, classHistory)};
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
            const std::lock_guard<std::mutex> lock(bpMutex);
            std::vector<BreakpointEntity> bpEntities;
            bpEntities.reserve(next.entities.size());
            for (const auto& e : next.entities) bpEntities.push_back({e.id, e.behaviorLabel});

            const BreakpointTickResult result{bp.tick(bpEntities, matchesLabel, next.simSec)};
            if (result.outcome != BreakpointOutcome::None) {
               if (clockStation != nullptr) {
                  if (result.shouldRestoreScale) clockStation->setTimeScale(bp.restoreTimeScale());
                  if (result.shouldPause) clockStation->setPaused(true);
               }
               fastRunToBreakpoint = false;
            }

            // Publica pro DashboardState -- ver o "porque" no cabecalho de
            // BreakpointController.hpp e o pedido explicito de deixar o
            // travamento de velocidade (armado, QUALQUER modo) e o "informe"
            // de hit obvios na UI, nao so dentro do card da arvore. status()
            // com (false,false,"") le so armed_/hit_ -- os
            // parametros de selecao de arvore nao entram nesses ramos.
            next.breakpointArmed = bp.isArmed();
            const BreakpointStatus globalBpStatus{bp.status(false, false, "")};
            next.breakpointHit = (globalBpStatus.branch == BreakpointStatusBranch::Hit);
            next.breakpointHitMessage = globalBpStatus.text;
         }

         next.actualTimeScale = measuredActualTimeScale;
         next.fastBreakpointRun = fastRunToBreakpoint.load();

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
   SpeedLadder ladder;
   ladder.seedFromScale(clockStation != nullptr ? clockStation->getTimeScale() : 1.0);

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

   // ---- aba "Log" ----
   // 'logMinLevel' e o filtro por nivel MINIMO (tecla/botao [f]);
   // 'logFollowTail' faz a selecao grudar na linha mais recente enquanto o
   // usuario nao rolar pra cima -- e o comportamento que se espera de um
   // painel de log ao vivo (tipo 'tail -f'), e qualquer ArrowUp desliga.
   // 'lastLogSeq' evita copiar as (ate 500) linhas do buffer a cada
   // redesenho: so recopia quando xlog::lastSeq() muda, ou quando o filtro
   // muda (ai o conteudo exibido muda sem linha nova nenhuma).
   int selectedLogIndex{};
   bool logFollowTail{true};
   mixr::xlog::Level logMinLevel{mixr::xlog::Level::DEBUG};
   mixr::xlog::Level lastLogFilter{mixr::xlog::Level::DEBUG};
   std::uint64_t lastLogSeq{};
   MapViewState mapView;
   // Caixa de tela do canvas do mapa apos o ultimo desenho (ftxui::reflect,
   // dentro de renderMap()) -- usada pelo CatchEvent mais externo pra saber
   // se um clique caiu DENTRO do mapa antes de tratar como arrasto/selecao
   // (ver o comentario grande na secao do mapa, mais abaixo).
   Box mapCanvasBox{};
   double lastTrailSimSec{-1.0};

   // ---- aba "Componentes" (F6) -- ver app/ComponentTreePanel.hpp/
   // app/ComponentTreeQuery.hpp. 'componentsLayout' e recalculado a cada
   // redesenho dentro do Renderer mais externo (mesmo lugar que ja
   // atualiza 'displayedEntities'/'displayedClasses'), barato o bastante
   // (dezenas de nos) pra nao precisar de cache -- um missil liberado ou um
   // fantasma DIS aparecem/somem sozinhos, sem invalidacao manual.
   ComponentTreeViewState componentsView;
   // A arvore DESCOBERTA (nao a posicionada) fica guardada porque
   // "retrair/expandir tudo" varre a estrutura inteira -- inclusive os
   // galhos que estao retraidos e portanto nem entram no layout.
   ComponentTreeNode componentsRoot;
   ComponentTreeLayout componentsLayout;
   // Chaves dos galhos retraidos -- ver app/ComponentTreePanel.hpp. Guardado
   // por CHAVE (e nao por indice) porque a arvore e redescoberta a cada
   // redesenho e o indice de um no muda quando algo nasce/some nela.
   CollapsedNodes componentsCollapsed;
   Box componentsCanvasBox{};
   bool componentsAutoFitted{};
   // Pedido de reenquadramento: "expandir/retrair tudo" muda a extensao da
   // arvore em ordem de grandeza, e manter o pan/zoom de antes deixaria a
   // vista num canto vazio. Um clique/tecla apenas ARMA o pedido; quem
   // reenquadra e o Renderer da aba, que e onde o canvas ja tem tamanho.
   bool componentsRefit{};

   // SEGUNDA METADE da feature (ver app/ComponentFlowState.hpp): o "pulso"
   // que percorre as fases do ciclo conceitual. Avancado por
   // tickComponentFlowAnimation() a cada redesenho (ver o Renderer mais
   // externo, mais abaixo) -- MODELO CONCEITUAL, nao medicao ao vivo.
   ComponentFlowState componentsFlow;

   // Os numeros VIVOS que entram nos argumentos da cadeia de chamadas (ver
   // app/FrameCallChain.hpp) -- recalculados a cada redesenho no Renderer
   // mais externo, junto com o resto. Nao sao constantes: 'fastForwardRate'
   // muda com [+]/[-], e 'paused' e o que faz a cadeia mostrar dt0 = 0.
   FrameCallParams frameCallParams;

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
   std::vector<mixr::xlog::Entry> displayedLogs;
   std::vector<std::string> logLabels;
   BackgroundInfo displayedBackground;

   // Copia "pra desenho" de DashboardState::breakpoint* -- ver o cabecalho
   // desses campos em DashboardState.hpp. Usada so pelos 'transform' dos
   // botoes (podem atrasar um quadro sem problema -- e so estetica); a
   // trava DE VERDADE dos comandos manuais le 'bp.isArmed()' direto, sob
   // 'bpMutex' (ver isBreakpointArmedNow(), acima).
   bool displayedBreakpointArmed{};
   bool displayedBreakpointHit{};
   std::string displayedBreakpointHitMessage;

   // A arvore de BT achatada NAO muda (o arquivo e lido uma vez, no
   // startup -- ver main.cpp) -- calculada aqui, fora de qualquer Renderer.
   // 'selectedBtLineIndex' e a escolha do usuario (clique na "caixa da
   // arvore" -- pedido explicito), compartilhada pelas DUAS instancias de
   // Menu (Frota e Mapa, ver mais abaixo) e pela logica de breakpoint.
   const std::vector<BtTreeLine> treeLines{flattenBehaviorTree(behaviorTree)};
   std::vector<std::string> treeLineLabels;
   for (const auto& line : treeLines) treeLineLabels.push_back(line.display);
   int selectedBtLineIndex{};

   // ---- acoes nomeadas: cada una e usada por TECLA e por BOTAO ----
   //
   // Acelerar/frear/voltar-a-tempo-real MANUAIS ficam BLOQUEADOS enquanto
   // HOUVER um breakpoint armado -- pedido explicito, e vale nos DOIS modos
   // ('g': velocidade atual; 'G': velocidade maxima), nao so no rapido: a
   // ideia e "a velocidade fica travada no que estava/na maxima ate o BP
   // ser atingido", nao so "nao adianta acelerar durante o modo rapido".
   // 'doRealTime' entra na mesma trava por mudar a escada pra 1x (e
   // despausar) -- deixa-lo passar destrancaria a velocidade so apertando
   // '1'.
   //
   // Le 'bp.isArmed()' sob 'bpMutex' -- chamado so por tecla/clique do
   // usuario (nao no laco quente de 'simThread'), entao tomar o lock aqui
   // e barato e mantem a checagem sempre CORRETA (ao contrario de uma
   // copia "so pra exibir" que pode atrasar um quadro -- ver
   // 'displayedBreakpointArmed', usado so pelo desenho dos botoes).
   const auto isBreakpointArmedNow = [&] {
      const std::lock_guard<std::mutex> lock(bpMutex);
      return bp.isArmed();
   };
   // Nenhuma destas acoes chama LOG(...): o app e LEITOR do log, nao
   // produtor. Quem escreve e o MODELO (models/A4) -- ver o cabecalho
   // de app/LogPanel.hpp.
   const auto doAccelerate = [&] {
      if (clockStation == nullptr || isBreakpointArmedNow()) return;
      if (ladder.accelerate()) clockStation->setTimeScale(ladder.scale());
   };
   const auto doDecelerate = [&] {
      if (clockStation == nullptr || isBreakpointArmedNow()) return;
      if (ladder.decelerate()) clockStation->setTimeScale(ladder.scale());
   };
   const auto doTogglePause = [&] { if (clockStation != nullptr) clockStation->togglePaused(); };
   const auto doRealTime = [&] {
      if (clockStation == nullptr || isBreakpointArmedNow()) return;
      ladder.toRealTime();
      clockStation->setPaused(false);
      clockStation->setTimeScale(ladder.scale());
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

   // Reancora o nivel do terreno perto do limite inferior da janela
   // (ver MapPanel.hpp::snapPanToGroundLevel) -- so tem efeito na
   // perspectiva Lateral com terreno ligado; um sampler novo aqui e
   // barato (mesmo raciocinio do que 'mapCanvasArea' ja reconstroi a cada
   // redesenho, ver mais abaixo).
   const auto doMapSnapGroundIfApplicable = [&] {
      if (!mapView.showTerrain) return;
      snapPanToGroundLevel(mapView, makeTerrainSampler(worldModel));
   };
   const auto doMapToggleTerrain = [&] {
      mapView.showTerrain = !mapView.showTerrain;
      doMapSnapGroundIfApplicable();
   };
   const auto doMapTogglePerspective = [&] {
      mapView.perspective = (mapView.perspective == Perspective::TopDown)
         ? Perspective::Lateral : Perspective::TopDown;
      doMapSnapGroundIfApplicable();
   };
   const auto doMapCenterOnSelected = [&] {
      if (displayedEntities.empty()) return;
      const std::size_t idx{static_cast<std::size_t>(
         std::clamp(selectedEntityIndex, 0, static_cast<int>(displayedEntities.size()) - 1))};
      centerMapOn(mapView, displayedEntities[idx]);
      doMapSnapGroundIfApplicable();
   };

   // ---- acoes da aba "Componentes" (F6) -- mesma regra de sempre: uma
   // lambda, usada por tecla E por botao ----
   const auto doCompZoomIn = [&] { zoomComponentTree(componentsView, true); };
   const auto doCompZoomOut = [&] { zoomComponentTree(componentsView, false); };
   const auto doCompCenterOnSelected = [&] {
      const int idx{findComponentNodeIndex(componentsLayout, componentsView.selectedKey)};
      if (idx < 0) return;
      centerComponentTreeOn(componentsView, componentsLayout.nodes[static_cast<std::size_t>(idx)]);
   };

   // ---- retrair/expandir (o galho selecionado, ou a arvore toda) ----
   const auto doCompToggleCollapse = [&] {
      const int idx{findComponentNodeIndex(componentsLayout, componentsView.selectedKey)};
      if (idx < 0) return;
      toggleComponentNodeCollapsed(componentsCollapsed,
                                   componentsLayout.nodes[static_cast<std::size_t>(idx)]);
   };
   const auto doCompExpandAll = [&] {
      componentsCollapsed.clear();
      componentsRefit = true;
   };
   const auto doCompCollapseAll = [&] {
      componentsCollapsed.clear();
      collapseAllComponentNodes(componentsRoot, componentsCollapsed);
      componentsRefit = true;
   };

   // Navegacao por teclado entre os nos -- e o que torna retrair/expandir
   // usavel sem mouse (antes a selecao so existia por clique). Reenquadra
   // SO quando o no escolhido saiu do canvas, pra vista nao "pular" a cada
   // seta (ver ensureComponentNodeVisible()).
   const auto doCompNavigate = [&](const TreeNavigation dir) {
      if (!navigateComponentTree(componentsLayout, componentsView, dir, componentsCollapsed)) return;
      const int idx{findComponentNodeIndex(componentsLayout, componentsView.selectedKey)};
      if (idx >= 0) ensureComponentNodeVisible(componentsView,
                                               componentsLayout.nodes[static_cast<std::size_t>(idx)]);
   };

   // ---- animacao de fluxo da aba "Componentes" (SEGUNDA METADE, ver
   // app/ComponentFlowState.hpp) -- mesma regra de sempre: uma lambda, usada
   // por tecla E por botao ----
   // [Espaco] na aba F6 pausa a SIMULACAO de verdade -- o mesmo caminho do
   // botao/tecla global, nao um relogio paralelo. A animacao do fluxo virou
   // ESCRAVA desse estado (ver 'setComponentFlowPlaying' no Renderer mais
   // externo): pausou a simulacao, o pulso para junto, e o cabecalho
   // (t=/sim=/thr=) passa a refletir o que foi comandado aqui -- que era
   // exatamente o que nao acontecia antes.
   const auto doCompTogglePlay = doTogglePause;

   // [n] = UM Station::tcFrame(dt) de verdade. Pausa antes, se estiver
   // rodando (o mesmo gesto de qualquer depurador: dar um passo implica
   // parar), porque dar o passo com a thread T/C nativa ativa poria duas
   // threads dentro do mesmo frame. O ponteiro de fase avanca junto, pra
   // toques seguidos percorrerem a explicacao da cadeia de chamadas.
   const auto doCompStep = [&] {
      if (clockStation != nullptr && !clockStation->isPaused()) clockStation->setPaused(true);
      stepFrameRequests.fetch_add(1);
      advanceComponentFlowStep(componentsFlow);
   };
   const auto doCompCycleSpeed = [&] { cycleComponentFlowSpeed(componentsFlow); };


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

      const std::size_t idx{static_cast<std::size_t>(
         std::clamp(selectedEntityIndex, 0, static_cast<int>(displayedEntities.size()) - 1))};
      const EntityState& target{displayedEntities[idx]};

      {
         const std::lock_guard<std::mutex> lock(bpMutex);
         bp.arm(target.id, target.name, line.tag, fast,
               clockStation != nullptr ? clockStation->getTimeScale() : 1.0);
         if (fast && clockStation != nullptr) {
            // Crava o pool de tempo critico no topo da escada -- ver o
            // comentario grande sobre 'restoreTimeScale()' em
            // app/BreakpointController.hpp sobre por que pular o msleep()
            // do dashboard nao bastava sozinho.
            clockStation->setTimeScale(ladder.maxScale());
         }
      }
      fastRunToBreakpoint = fast;
      if (clockStation != nullptr) clockStation->setPaused(false);
   };
   const auto doCancelBreakpoint = [&] {
      const std::lock_guard<std::mutex> lock(bpMutex);
      const bool shouldRestoreScale{bp.cancel()};
      if (shouldRestoreScale && clockStation != nullptr) clockStation->setTimeScale(bp.restoreTimeScale());
      fastRunToBreakpoint = false;
   };
   // Rotulo dos dois botoes ja e a LEGENDA do efeito (pedido explicito de
   // deixar g/G intuitivos sem precisar abrir outro lugar pra entender):
   // "trava" e a palavra que tambem aparece no cabecalho ([BP]) e no status
   // do card da arvore (ver BreakpointController::status()) -- vocabulario
   // consistente nos tres lugares.
   const Component btnRunToBreakpoint{
      makeButton("[g] Rodar (trava veloc. atual)", [&] { doArmBreakpoint(false); })};
   const Component btnRunToBreakpointMax{
      makeButton("[G] Rodar (trava em veloc. MAXIMA)", [&] { doArmBreakpoint(true); })};
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
         isBreakpoint = bp.isArmedOn(line.tag);
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
      const bool hasSelection{selectedBtLineIndex >= 0
                              && selectedBtLineIndex < static_cast<int>(treeLines.size())};
      const bool selectionIsLeaf{hasSelection
         && treeLines[static_cast<std::size_t>(selectedBtLineIndex)].leaf};
      const std::string selectedLeafTag{hasSelection
         ? treeLines[static_cast<std::size_t>(selectedBtLineIndex)].tag : std::string{}};

      BreakpointStatus snap;
      {
         const std::lock_guard<std::mutex> lock(bpMutex);
         snap = bp.status(hasSelection, selectionIsLeaf, selectedLeafTag);
      }

      // 'paragraphAlignLeft' quebra linha sozinho na largura disponivel --
      // pedido explicito: texto extenso vazava o quadro da arvore. Botao(es)
      // vao numa linha PROPRIA, embaixo do texto, nunca no mesmo hbox (um
      // hbox nao quebra linha, so estoura pra fora do card).
      switch (snap.branch) {
         case BreakpointStatusBranch::Armed:
            return vbox({paragraphAlignLeft(snap.text) | color(Color::Yellow) | bold,
                         btnCancelBreakpoint->Render()});
         case BreakpointStatusBranch::Hit:
            return paragraphAlignLeft(snap.text) | color(Color::Green) | bold;
         case BreakpointStatusBranch::LeafSelected:
            return vbox({paragraphAlignLeft(snap.text),
                         hbox({btnRunToBreakpoint->Render(), text(" "), btnRunToBreakpointMax->Render()})});
         case BreakpointStatusBranch::NonLeafSelected:
         case BreakpointStatusBranch::NoTreeSelection:
         default:
            return paragraphAlignLeft(snap.text) | dim;
      }
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
         // Subquadro PROPRIO, com barra de titulo -- pedido explicito:
         // "tal como no subquadro acima do player" (o card de detalhe
         // usa a mesma receita: uma linha de titulo, um separador, o
         // conteudo, tudo dentro do MESMO 'border').
         const Element treeBox{vbox({
            text(" Arvore de Comportamento ") | bold | bgcolor(Color::Blue) | color(Color::White),
            separator(),
            treeMenu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 14),
            separator(),
            buildBreakpointStatus(),
         }) | border};
         parts.push_back(separator());
         parts.push_back(treeBox);
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
                // O LIMITE tem de caber a soma de TODAS as colunas
                // (badge+nome+tipo+bt+thread+altitude+vel+combust, ver
                // app/FleetPanel.hpp) -- com "60" (herdado de antes da
                // coluna de thread) a soma passava do limite e o FTXUI
                // apertava as ultimas colunas em silencio (o cabecalho
                // "vel(kt)"/"combust." e quem expos isso: sem gap nenhum
                // antes da coluna seguinte, mesmo com kCol* de sobra).
                vbox({
                   renderEntityListHeader(),
                   separator(),
                   entityMenu->Render() | vscroll_indicator | frame | flex,
                }) | size(WIDTH, LESS_THAN, kColBadge + kColName + kColType + kColBehavior
                                            + kColThread + kColAlt + kColSpd + kColFuel + 4) | flex,
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
      // Reconstruido a cada redesenho (barato: dois getters + um ponteiro
      // capturado, ver app/TerrainQuery.hpp) -- so e CHAMADO de verdade por
      // renderMap() quando 'mapView.showTerrain' esta ligado.
      const TerrainSampler terrainSampler{makeTerrainSampler(worldModel)};

      // O canvas acompanha a area que o layout DE FATO reservou pro mapa,
      // em vez de um tamanho fixo que sobrava (terminal grande: mapa
      // desenhado so num pedaco do quadro) ou faltava (terminal pequeno:
      // desenho cortado). 'mapCanvasBox' e do quadro ANTERIOR -- e a unica
      // hora em que a caixa existe, ver fitMapCanvasToBox() em
      // app/MapPanel.hpp.
      fitMapCanvasToBox(mapView, mapCanvasBox);
      return hbox({
                renderMap(displayedEntities, mapView, focusedId, mapCanvasBox, terrainSampler) | flex,
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
      return text(std::string(" [t] Rastro: ") + (mapView.showTrails ? "ON" : "OFF") + " ")
         | (mapView.showTrails ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapTrails{Button(trailsOpt)};

   ButtonOption terrainOpt;
   terrainOpt.label = "[e] Terreno";
   terrainOpt.on_click = doMapToggleTerrain;
   terrainOpt.transform = [&](const EntryState&) {
      return text(std::string(" [e] Terreno: ") + (mapView.showTerrain ? "ON" : "OFF") + " ")
         | (mapView.showTerrain ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapTerrain{Button(terrainOpt)};

   ButtonOption perspectiveOpt;
   perspectiveOpt.label = "[v] Vista";
   perspectiveOpt.on_click = doMapTogglePerspective;
   perspectiveOpt.transform = [&](const EntryState&) {
      const bool lateral{mapView.perspective == Perspective::Lateral};
      return text(std::string(" [v] Vista: ") + (lateral ? "Lado" : "Cima") + " ")
         | (lateral ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnMapPerspective{Button(perspectiveOpt)};

   const Component mapButtons{Container::Horizontal({
      btnMapZoomOut, btnMapZoomIn, btnMapRotL, btnMapRotR,
      btnMapCenter, btnMapTrails, btnMapTerrain, btnMapPerspective,
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

   // ---- aba "Componentes" (F6): arvore de componentes REAL da Station,
   // navegavel -- mesma receita de pan/zoom/clique/canvas-responsivo da aba
   // Mapa (ver app/ComponentTreePanel.hpp/.cpp), PRIMEIRA METADE da feature:
   // so a estrutura estatica (sem animacao de fluxo entre fases nem
   // play/pause/step -- fica pra proxima iteracao). ----
   const Component componentsCanvasArea{Renderer([&]() -> Element {
      // MESMO tamanho do card de detalhe das abas F1/F2 (pedido explicito):
      // 'detailPanelWidth' e recalculado uma vez por redesenho no Renderer
      // mais externo, e 'kDetailPanelHeight' e a constante que as outras
      // duas abas ja usam -- nao ha mais largura propria escrita aqui.
      Element detail{text("(clique num no da arvore, ou navegue com as setas)")
                     | dim | center
                     | size(WIDTH, EQUAL, detailPanelWidth) | size(HEIGHT, EQUAL, kDetailPanelHeight)};
      const int selected{findComponentNodeIndex(componentsLayout, componentsView.selectedKey)};
      if (selected >= 0) {
         detail = renderComponentDetail(componentsLayout.nodes[static_cast<std::size_t>(selected)],
                                        componentsFlow, frameCallParams)
                  | size(WIDTH, EQUAL, detailPanelWidth) | size(HEIGHT, EQUAL, kDetailPanelHeight);
      }
      // Mesma tecnica de fitMapCanvasToBox() (ver o comentario grande em
      // app/MapPanel.hpp e a "decima sexta passada" do CLAUDE.md) --
      // 'componentsCanvasBox' e a caixa do quadro ANTERIOR.
      fitComponentTreeCanvasToBox(componentsView, componentsCanvasBox);

      // Uma vez so, assim que ha arvore E canvas de tamanho de verdade --
      // ver o comentario grande de app::fitComponentTreeToContent(): sem
      // isto, o pan/zoom DEFAULT deixa quase toda a arvore fora do canvas
      // (a raiz nasce na linha MEDIA de toda a arvore). Depois desta
      // primeira vez, pan/zoom manual do usuario nao e mais sobrescrito.
      if ((!componentsAutoFitted || componentsRefit) && !componentsLayout.nodes.empty()) {
         fitComponentTreeToContent(componentsView, componentsLayout);
         componentsAutoFitted = true;
         componentsRefit = false;
      }
      return hbox({
                renderComponentTree(componentsLayout, componentsView, componentsCanvasBox,
                                    componentsFlow, frameCallParams) | flex,
                separator(),
                detail,
             })
             | flex;
   })};

   const Component btnCompZoomOut{makeButton("[[] Zoom-", doCompZoomOut)};
   const Component btnCompZoomIn{makeButton("[]] Zoom+", doCompZoomIn)};
   const Component btnCompCenter{makeButton("[c] Centralizar", doCompCenterOnSelected)};

   // Play/pause com rotulo dinamico -- mesmo padrao de 'btnLogFollow'/
   // 'logFilterOpt' acima (transform le estado vivo, nao so o clique). O
   // rotulo diz SIMULACAO de proposito: e a simulacao que para, nao um
   // relogio de animacao a parte.
   ButtonOption compPlayOpt;
   compPlayOpt.on_click = doCompTogglePlay;
   compPlayOpt.transform = [&](const EntryState&) {
      const bool running{!frameCallParams.paused};
      return text(std::string(" [Espaco] ") + (running ? "Pausar" : "Rodar") + " ")
         | (running ? (bgcolor(Color::Green) | color(Color::Black) | bold)
                     : (bgcolor(Color::Yellow) | color(Color::Black) | bold));
   };
   const Component btnCompPlay{Button(compPlayOpt)};

   // O rotulo carrega o dt de verdade -- e a resposta curta pra "quanto vale
   // um passo", sem ter de ler o painel inteiro.
   ButtonOption compStepOpt;
   compStepOpt.on_click = doCompStep;
   compStepOpt.transform = [&](const EntryState&) {
      std::ostringstream os;
      os << " [n] Passo " << std::fixed << std::setprecision(4)
         << frameStepSeconds(frameCallParams) << "s ";
      return text(os.str()) | bgcolor(Color::Blue) | color(Color::White) | bold;
   };
   const Component btnCompStep{Button(compStepOpt)};

   // Retrair/expandir -- tecla E botao, a regra de sempre. O rotulo do
   // primeiro muda com o estado do no selecionado (transform roda a cada
   // redesenho), pra dizer o que a tecla vai FAZER e nao so que existe.
   ButtonOption compToggleOpt;
   compToggleOpt.on_click = doCompToggleCollapse;
   compToggleOpt.transform = [&](const EntryState&) {
      const int idx{findComponentNodeIndex(componentsLayout, componentsView.selectedKey)};
      const bool canToggle{idx >= 0
         && componentsLayout.nodes[static_cast<std::size_t>(idx)].childCount > 0};
      const bool isCollapsed{canToggle
         && componentsLayout.nodes[static_cast<std::size_t>(idx)].collapsed};
      if (!canToggle) return text(" [Enter] Retrair ") | dim;
      return text(std::string(" [Enter] ") + (isCollapsed ? "Expandir" : "Retrair") + " ")
         | bgcolor(Color::Blue) | bold;
   };
   const Component btnCompToggle{Button(compToggleOpt)};

   const Component btnCompExpandAll{makeButton("[o] Abrir tudo", doCompExpandAll)};
   const Component btnCompCollapseAll{makeButton("[f] Fechar tudo", doCompCollapseAll)};

   ButtonOption compSpeedOpt;
   compSpeedOpt.on_click = doCompCycleSpeed;
   compSpeedOpt.transform = [&](const EntryState&) {
      return text(" [v] " + std::to_string(componentsFlow.stepsPerSecond) + "x/s ")
         | bgcolor(Color::Blue) | bold;
   };
   const Component btnCompSpeed{Button(compSpeedOpt)};

   const Component componentsButtons{Container::Horizontal(
      {btnCompPlay, btnCompStep, btnCompSpeed,
       btnCompToggle, btnCompExpandAll, btnCompCollapseAll,
       btnCompZoomOut, btnCompZoomIn, btnCompCenter})};

   const Component componentsBody{Container::Vertical({componentsCanvasArea, componentsButtons})};
   const Component componentsTab{Renderer(componentsBody, [&]() -> Element {
      // MESMA altura da versao anterior a esta feature: uma linha em cima
      // (era um texto dim, hoje e a faixa de fases -- o mesmo espaco), o
      // canvas com todo o resto, e o rodape de sempre. A explicacao do passo
      // NAO mora mais num painel de texto que roubava ~19 linhas do desenho:
      // ela foi PARA DENTRO do desenho (rotulo de chamada em cada no, o
      // caminho da recursao aceso, a onda descendo) e para o card de
      // detalhe, que ja tinha espaco sobrando.
      return vbox({
         renderFramePhaseStrip(componentsFlow, frameCallParams),
         componentsCanvasArea->Render() | flex,
         separator(),
         renderComponentFlowStatus(componentsFlow, frameCallParams),
         renderComponentFlowLegend(),
         text("[setas] navegar entre nos  [Enter] retrair/expandir  [o]/[f] expandir/retrair tudo  "
              "[clique] selecionar  [arraste] mover  [ [ ]/roda ] zoom  [c] centralizar  "
              "[Espaco] pausar simulacao  [n] passo de 1 frame  [v] velocidade") | dim,
         componentsButtons->Render(),
      });
   })};
   // O tratamento de tecla/mouse desta aba, pelo MESMO motivo ja documentado
   // no comentario grande sobre 'mapTab'/'withKeys' logo acima, NAO fica num
   // CatchEvent local aqui -- fica no CatchEvent mais externo (ver
   // 'activeTab == 5' la embaixo).

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

   // ---- aba "Tempo Nao-Critico": o que roda na thread de tempo NAO critico -- painel
   // estatico, sem lista (ver app/BackgroundPanel.hpp). 'displayedBackground'
   // e alimentado pelo MESMO Renderer externo que ja alimenta
   // 'displayedEntities'/'displayedClasses' (ver 'withRenderer' abaixo) --
   // nao ha necessidade de tomar 'stateMutex' de novo aqui. ----
   const Component backgroundTab{Renderer([&]() -> Element {
      return renderBackgroundPanel(displayedBackground) | frame | flex;
   })};

   // ---- aba "Log": as ultimas linhas de shared/xlog, do host E do plugin
   // do modelo (uma copia so de libxlog.so no processo -- ver o cabecalho
   // de app/LogPanel.hpp). Mesmo padrao de lista rolavel das abas
   // Players/Memoria: ftxui::Menu dentro de frame()/vscroll_indicator(). ----
   MenuOption logMenuOpt;
   logMenuOpt.entries = &logLabels;
   logMenuOpt.selected = &selectedLogIndex;
   logMenuOpt.entries_option.transform = [&](const EntryState& es) -> Element {
      if (es.index >= 0 && es.index < static_cast<int>(displayedLogs.size()))
         return renderLogRow(displayedLogs[static_cast<std::size_t>(es.index)], es.active);
      return text(es.label);
   };
   const Component logMenu{Menu(logMenuOpt)};

   const auto doCycleLogFilter = [&] {
      logMinLevel = nextLevelFilter(logMinLevel);
      logFollowTail = true;
   };
   const auto doToggleLogFollow = [&] { logFollowTail = !logFollowTail; };

   ButtonOption logFilterOpt;
   logFilterOpt.on_click = doCycleLogFilter;
   logFilterOpt.transform = [&](const EntryState&) {
      return text(std::string(" [f] Nivel min: ") + mixr::xlog::levelName(logMinLevel) + " ")
         | bgcolor(Color::Blue) | bold;
   };
   const Component btnLogFilter{Button(logFilterOpt)};

   ButtonOption logFollowOpt;
   logFollowOpt.on_click = doToggleLogFollow;
   logFollowOpt.transform = [&](const EntryState&) {
      return text(std::string(" [a] Acompanhar: ") + (logFollowTail ? "ON" : "OFF") + " ")
         | (logFollowTail ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnLogFollow{Button(logFollowOpt)};

   const Component logButtons{Container::Horizontal({btnLogFilter, btnLogFollow})};
   const Component logBody{Container::Vertical({logMenu, logButtons})};
   const Component logTab{Renderer(logBody, [&]() -> Element {
      return vbox({
         hbox({
            text("linhas: " + std::to_string(displayedLogs.size())) | dim,
            text("  (buffer de " + std::to_string(mixr::xlog::kMemoryCapacity) + ", o mais antigo sai)") | dim,
            filler(),
            text("total emitido: " + std::to_string(lastLogSeq)) | dim,
         }),
         separator(),
         renderLogListHeader(),
         separator(),
         logMenu->Render() | vscroll_indicator | frame | flex,
         separator(),
         logButtons->Render(),
      });
   })};

   const Component contentTab{Container::Tab(
      {fleetTab, mapTab, memoryTab, backgroundTab, logTab, componentsTab}, &activeTab)};

   // ---- barra de abas e barra de acoes, TODAS clicaveis (Button de
   // verdade), com a dica de atalho ja no rotulo ----

   const Component btnFleet{makeButton("[F1] Players", [&] { gotoTab(0); })};
   const Component btnMap{makeButton("[F2] Mapa", [&] { gotoTab(1); })};
   const Component btnMemory{makeButton("[F3] Memoria", [&] { gotoTab(2); })};
   const Component btnBackground{makeButton("[F4] Tempo Nao-Critico", [&] { gotoTab(3); })};
   const Component btnLog{makeButton("[F5] Log", [&] { gotoTab(4); })};
   const Component btnComponents{makeButton("[F6] Componentes", [&] { gotoTab(5); })};

   // Acelerar/Frear/Tempo-real ficam visualmente apagados enquanto
   // bloqueados -- QUALQUER breakpoint armado ('g' OU 'G', ver
   // 'doAccelerate'/'doDecelerate'/'doRealTime' acima), nao so o modo
   // rapido. 'transform' roda a cada redesenho, entao basta ler
   // 'displayedBreakpointArmed' direto nele (a copia "pra desenho",
   // atualizada por 'withRenderer' -- ver o comentario onde ela e
   // declarada).
   ButtonOption accelOpt;
   accelOpt.label = "[+] Acelerar";
   accelOpt.on_click = doAccelerate;
   accelOpt.transform = [&](const EntryState&) {
      return text(" [+] Acelerar ") | (displayedBreakpointArmed ? dim : nothing);
   };
   const Component btnAccel{Button(accelOpt)};

   ButtonOption decelOpt;
   decelOpt.label = "[-] Frear";
   decelOpt.on_click = doDecelerate;
   decelOpt.transform = [&](const EntryState&) {
      return text(" [-] Frear ") | (displayedBreakpointArmed ? dim : nothing);
   };
   const Component btnDecel{Button(decelOpt)};

   const Component btnPause{makeButton("[espaco] Pausar", doTogglePause)};

   ButtonOption realOpt;
   realOpt.label = "[1] Tempo real";
   realOpt.on_click = doRealTime;
   realOpt.transform = [&](const EntryState&) {
      return text(" [1] Tempo real ") | (displayedBreakpointArmed ? dim : nothing);
   };
   const Component btnReal{Button(realOpt)};
   const Component btnLoad{makeButton("[l] Carregar", doLoad)};
   const Component btnRestart{makeButton("[r] Reiniciar", doRestart)};
   const Component btnStop{makeButton("[s] Parar", doStop)};
   const Component btnQuit{makeButton("[q] Sair", doQuit)};

   const Component toolbar{Container::Horizontal({
      btnFleet, btnMap, btnMemory, btnBackground, btnLog, btnComponents,
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
      displayedBackground = snap.background;
      displayedBreakpointArmed = snap.breakpointArmed;
      displayedBreakpointHit = snap.breakpointHit;
      displayedBreakpointHitMessage = snap.breakpointHitMessage;

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

      // Aba Log -- so recopia do buffer quando ha linha nova (lastSeq
      // mudou) ou quando o filtro mudou; 'snapshot()' copia ate 500
      // entradas e este Renderer roda a cada redesenho, nao so a cada
      // amostra nova (tecla, resize e mouse tambem redesenham).
      {
         const std::uint64_t seqNow{mixr::xlog::lastSeq()};
         if (seqNow != lastLogSeq || logMinLevel != lastLogFilter) {
            lastLogSeq = seqNow;
            lastLogFilter = logMinLevel;
            displayedLogs.clear();
            logLabels.clear();
            for (const auto& e : mixr::xlog::snapshot()) {
               if (!passesLevelFilter(e.level, logMinLevel)) continue;
               displayedLogs.push_back(e);
               logLabels.push_back(logRowText(e));
            }
            if (logFollowTail && !displayedLogs.empty()) {
               selectedLogIndex = static_cast<int>(displayedLogs.size()) - 1;
            }
         }
      }
      selectedLogIndex = displayedLogs.empty()
         ? 0
         : std::clamp(selectedLogIndex, 0, static_cast<int>(displayedLogs.size()) - 1);
      if (!treeLines.empty()) {
         selectedBtLineIndex = std::clamp(selectedBtLineIndex, 0,
            static_cast<int>(treeLines.size()) - 1);
      }

      // Aba Componentes -- recalculada a cada redesenho, direto de
      // 'station' (mesmo raciocinio de 'makeTerrainSampler(worldModel)' na
      // aba Mapa: barato, e um cache manual so arriscaria mostrar uma
      // arvore velha depois de um missil ser liberado ou um fantasma DIS
      // chegar pela rede). Feito aqui (Renderer mais externo, sempre roda)
      // e nao dentro de 'componentsCanvasArea' porque o CatchEvent
      // (hit-test/pan) precisa de 'componentsLayout' fresco independente de
      // qual aba esta ativa no momento do clique.
      componentsRoot = discoverComponentTree(station);

      // A arvore nasce EXPANDIDA so ate kTreeInitialExpandDepth: na vertical
      // cada FOLHA custa a largura do proprio rotulo (no layout horizontal
      // antigo custava so uma LINHA), entao uma arvore de producao inteira
      // aberta seria dezenas de vezes mais larga que o terminal. Uma vez so
      // -- depois disso quem manda e o usuario, e reabrir tudo e [o].
      if (!componentsAutoFitted && !componentsRoot.children.empty()) {
         collapseDeeperThan(componentsRoot, kTreeInitialExpandDepth, componentsCollapsed);
      }

      componentsLayout = layoutComponentTree(componentsRoot, componentsCollapsed);

      // Relogio da animacao de fluxo (SEGUNDA METADE) -- avanca aqui, no
      // MESMO Renderer mais externo que ja roda a cada redesenho
      // (screen.PostEvent(Event::Custom), ~10 Hz, disparado por
      // 'simThread' -- ver o comentario grande no topo deste arquivo),
      // reaproveitando esse pulso como relogio em vez de medir tempo de
      // parede. Roda mesmo com outra aba ativa, pelo mesmo motivo de
      // 'componentsLayout' ser recalculado incondicionalmente aqui.
      // A animacao e ESCRAVA da simulacao (pedido explicito: o controle da
      // aba F6 tem de mexer na simulacao de verdade). Pausou -> o pulso
      // para; voltou a rodar -> volta a andar. Feito aqui, e nao na acao de
      // tecla, pra tambem valer quando a pausa vem de outro lugar (o botao
      // [espaco] da barra principal, a tecla 'p', um breakpoint de BT
      // atingido).
      setComponentFlowPlaying(componentsFlow, !snap.paused);
      tickComponentFlowAnimation(componentsFlow);

      frameCallParams.tcRateHz = station->getTimeCriticalRate();
      frameCallParams.bgRateHz = static_cast<double>(bgRate);
      frameCallParams.fastForwardRate = station->getFastForwardRate();
      frameCallParams.numTcThreads = snap.numTcThreads;
      frameCallParams.paused = snap.paused;

      // A selecao NAO precisa de clamp: e uma CHAVE, nao um indice. Se o no
      // sumiu da arvore (retraido junto com o pai, missil que detonou,
      // fantasma DIS que saiu da rede), findComponentNodeIndex() devolve -1
      // e o card mostra o texto de "nenhum selecionado" -- e se ele voltar,
      // a selecao volta com ele.

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
      // aos demais botoes principais, so aparece na aba Players (F1) COM
      // uma entidade selecionada -- nas outras abas (Mapa, Memoria, Tempo Nao-Critico)
      // nao faz sentido ("ver no mapa" a partir de onde voce ja esta, ou
      // de uma lista que nao e de players).
      Elements primaryButtons{btnAccel->Render(), btnDecel->Render(), btnPause->Render(),
                              btnReal->Render()};
      if (activeTab == 0 && !displayedEntities.empty()) primaryButtons.push_back(btnViewOnMap->Render());

      // "Informe" de BP atingido, pedido explicito -- fica visivel ate o
      // usuario armar um NOVO breakpoint (ver o comentario de
      // DashboardState::breakpointHit), nao so no instante do hit; e uma
      // LINHA PROPRIA, logo abaixo do cabecalho, pra nao depender de
      // ninguem estar olhando o card da arvore (que ja mostra a mesma
      // mensagem, ver buildBreakpointStatus()) pra perceber.
      Elements rows{renderHeader(snap)};
      if (displayedBreakpointHit) {
         rows.push_back(text(" BP ATINGIDO -- " + displayedBreakpointHitMessage + " ")
                         | bold | bgcolor(Color::Blue) | color(Color::White) | center);
      }
      rows.push_back(hbox({tabBadge(btnFleet, 0), tabBadge(btnMap, 1), tabBadge(btnMemory, 2),
               tabBadge(btnBackground, 3), tabBadge(btnLog, 4), tabBadge(btnComponents, 5)}));
      rows.push_back(separator());
      rows.push_back(contentTab->Render() | flex);
      rows.push_back(separator());
      rows.push_back(hbox({
         hbox(std::move(primaryButtons)),
         filler(),
         btnLoad->Render(), btnRestart->Render(), btnStop->Render(), btnQuit->Render(),
      }));

      return vbox(std::move(rows));
   })};

   const Component withKeys{CatchEvent(withRenderer, [&](Event event) -> bool {
      // Espaco/[n] da aba "Componentes" (F6) tem de ser tratado ANTES do
      // espaco GLOBAL (pausa a simulacao, logo abaixo) -- dentro desta aba,
      // Espaco controla o PLAY/PAUSE da animacao de fluxo (relogio proprio,
      // ver app/ComponentFlowState.hpp), nao o relogio da simulacao. O
      // botao [espaco] Pausar da barra principal continua acessivel por
      // clique em qualquer aba; so a TECLA espaco muda de sentido aqui.
      if (activeTab == 5) {
         if (event == Event::Character(' ')) { doCompTogglePlay(); return true; }
         if (event == Event::Character('n') || event == Event::Character('N')) { doCompStep(); return true; }
         if (event == Event::Character('v') || event == Event::Character('V')) { doCompCycleSpeed(); return true; }

         // Retrair/expandir. Ficam AQUI, no bloco que roda ANTES das teclas
         // globais, pelo mesmo motivo do Espaco logo acima: 'f' ja significa
         // outra coisa na aba Log e 'o' e livre hoje, mas depender disso
         // seria uma armadilha esperando a proxima tecla global nascer.
         if (event == Event::Return) { doCompToggleCollapse(); return true; }
         if (event == Event::Character('o') || event == Event::Character('O')) { doCompExpandAll(); return true; }
         if (event == Event::Character('f') || event == Event::Character('F')) { doCompCollapseAll(); return true; }

         // Setas NAVEGAM entre os nos (antes moviam o pan). E o que permite
         // escolher um galho sem mouse -- e sem escolher nao ha o que
         // retrair. O pan continua acessivel por arrasto e por [c].
         if (event == Event::ArrowUp)    { doCompNavigate(TreeNavigation::Parent); return true; }
         if (event == Event::ArrowDown)  { doCompNavigate(TreeNavigation::FirstChild); return true; }
         if (event == Event::ArrowLeft)  { doCompNavigate(TreeNavigation::PrevSibling); return true; }
         if (event == Event::ArrowRight) { doCompNavigate(TreeNavigation::NextSibling); return true; }
      }
      if (event == Event::Character('+') || event == Event::Character('=')) { doAccelerate(); return true; }
      if (event == Event::Character('-') || event == Event::Character('_')) { doDecelerate(); return true; }
      if (event == Event::Character(' ') || event == Event::Character('p') ||
          event == Event::Character('P')) { doTogglePause(); return true; }
      if (event == Event::Character('1')) { doRealTime(); return true; }
      if (event == Event::F1) { gotoTab(0); return true; }
      if (event == Event::F2) { gotoTab(1); return true; }
      if (event == Event::F3) { gotoTab(2); return true; }
      if (event == Event::F4) { gotoTab(3); return true; }
      if (event == Event::F5) { gotoTab(4); return true; }
      if (event == Event::F6) { gotoTab(5); return true; }

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
      if (activeTab == 4) {
         // Rolar pra CIMA desliga o "acompanhar" (senao a proxima linha
         // nova arrastaria a selecao de volta pro fim e seria impossivel
         // ler o historico com a simulacao rodando); chegar de volta no fim
         // religa, que e o gesto natural de "voltar a acompanhar".
         if (event == Event::ArrowUp) {
            logFollowTail = false;
            selectedLogIndex = std::max(selectedLogIndex - 1, 0);
            return true;
         }
         if (event == Event::ArrowDown) {
            const int last{std::max(0, static_cast<int>(displayedLogs.size()) - 1)};
            selectedLogIndex = std::min(selectedLogIndex + 1, last);
            if (selectedLogIndex == last) logFollowTail = true;
            return true;
         }
         if (event == Event::Character('f') || event == Event::Character('F')) {
            doCycleLogFilter();
            return true;
         }
         if (event == Event::Character('a') || event == Event::Character('A')) {
            doToggleLogFollow();
            return true;
         }
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
         if (event == Event::Character('e') || event == Event::Character('E')) { doMapToggleTerrain(); return true; }
         if (event == Event::Character('v') || event == Event::Character('V')) { doMapTogglePerspective(); return true; }
         if (event == Event::Character('c') || event == Event::Character('C')) { doMapCenterOnSelected(); return true; }
      }

      // Interacao da aba "Componentes" -- MESMO raciocinio/estrutura do
      // bloco do Mapa logo acima (CatchEvent mais externo, gate por
      // 'componentsCanvasBox.Contain()' pro mouse, clique-vs-arrasto pelo
      // deslocamento total entre Pressed e Released).
      if (activeTab == 5) {
         if (event.is_mouse()) {
            const Mouse& m{event.mouse()};
            const bool insideCanvas{componentsCanvasBox.Contain(m.x, m.y)};

            if (componentsView.dragging) {
               if (m.motion == Mouse::Released) {
                  componentsView.dragging = false;
                  const int totalMove{std::abs(m.x - componentsView.pressX)
                                      + std::abs(m.y - componentsView.pressY)};
                  if (totalMove <= 1) {
                     const int cellX{m.x - componentsCanvasBox.x_min};
                     const int cellY{m.y - componentsCanvasBox.y_min};
                     const int hitIndex{hitTestComponentTreeNode(componentsLayout, componentsView,
                                                                 cellX, cellY)};
                     if (hitIndex >= 0) {
                        componentsView.selectedKey =
                           componentsLayout.nodes[static_cast<std::size_t>(hitIndex)].nodeKey;
                     }
                  }
                  return true;
               }
               if (m.motion == Mouse::Moved) {
                  const int dx{m.x - componentsView.dragLastX};
                  const int dy{m.y - componentsView.dragLastY};
                  componentsView.dragLastX = m.x;
                  componentsView.dragLastY = m.y;
                  panComponentTree(componentsView, dx * 2.0, dy * 4.0);
                  return true;
               }
               return true;
            }

            if (!insideCanvas) return false;
            if (m.button == Mouse::WheelUp) { doCompZoomIn(); return true; }
            if (m.button == Mouse::WheelDown) { doCompZoomOut(); return true; }
            if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
               componentsView.dragging = true;
               componentsView.pressX = m.x;
               componentsView.pressY = m.y;
               componentsView.dragLastX = m.x;
               componentsView.dragLastY = m.y;
               return true;
            }
            return false;
         }

         // As setas nao chegam aqui -- sao tratadas no bloco de 'activeTab == 5'
         // la em cima, onde passaram a NAVEGAR entre os nos em vez de mover
         // o pan (o pan continua no arrasto e em [c] Centralizar).
         if (event == Event::Character('[')) { doCompZoomOut(); return true; }
         if (event == Event::Character(']')) { doCompZoomIn(); return true; }
         if (event == Event::Character('c') || event == Event::Character('C')) {
            doCompCenterOnSelected(); return true;
         }
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

   // ---- ENCERRAMENTO, e a ORDEM aqui e o conserto (ver app/Shutdown.hpp) ----
   //
   // 1) Cala a PRODUTORA primeiro. A thread T/C nativa criada la em cima nao
   //    morre com o fim do Loop() -- ela sobrevive a esta funcao inteira e ao
   //    SHUTDOWN_EVENT do main.cpp. Enquanto ela roda, segue enfileirando
   //    registros na fila SEM TETO do gravador; e e a corrida dela contra o
   //    teardown que produz o auto-deadlock documentado em
   //    xclock/ClockStation.hpp.
   quiesceTimeCritical(station, clockStation);

   // 2) So agora para a CONSUMIDORA. Nesta ordem 'simThread' nao pode mais ser
   //    surpreendida por trabalho novo entrando na fila.
   running = false;
   simThread.join();

   // 3) Ultima drenagem, com a producao ja parada -- fecha a fila numa passada
   //    e deixa o DataRecorder::shutdownNotification() do SHUTDOWN_EVENT (que
   //    tambem drena, mas na thread main) com quase nada para fazer.
   station->updateData(1.0 / static_cast<double>(bgRate));

   // Terminal de volta pro dono anterior -- ver setConsoleEnabled(false)
   // no inicio desta funcao.
   mixr::xlog::setConsoleEnabled(true);

   return action;
}

} // namespace app
