#include "app/FrameCallChain.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace app {

namespace {

std::string secs(const double v)
{
   std::ostringstream os;
   os << std::fixed << std::setprecision(6) << v << " s";
   return os.str();
}

CallChainLine line(const int depth, const CallLineKind kind, const std::string& text,
                   const std::string& args = {}, const std::string& note = {},
                   const std::string& sourceRef = {}, const bool active = false)
{
   CallChainLine l;
   l.depth = depth;
   l.kind = kind;
   l.text = text;
   l.args = args;
   l.note = note;
   l.sourceRef = sourceRef;
   l.active = active;
   return l;
}

//------------------------------------------------------------------------------
// A cadeia do frame de TEMPO CRITICO. Toda linha foi lida do fonte do fork
// v1.0.5 (contexts/src/mixr/src/...) -- as referencias de arquivo:linha sao
// as de la, e estao no proprio dado para quem quiser conferir.
//------------------------------------------------------------------------------
std::vector<CallChainLine> timeCriticalChain(const EstimatedPhase phase, const FrameCallParams& p)
{
   const double dt{frameStepSeconds(p)};
   const double dtPhase{dt / 4.0};
   const double dt0{p.paused ? 0.0 : dt};

   const bool ph0 = (phase == EstimatedPhase::DynamicsPhase0);
   const bool ph12 = (phase == EstimatedPhase::SensorPhase1And2);
   const bool ph3 = (phase == EstimatedPhase::DecisionPhase3);
   const bool structural = (phase == EstimatedPhase::Structural);

   std::ostringstream rate;
   rate << std::fixed << std::setprecision(1) << p.tcRateHz;

   std::vector<CallChainLine> out;
   out.push_back(line(0, CallLineKind::Thread, "thread T/C nativa (StationTcPeriodicThread)", {},
                      "criada por Station::createTimeCriticalProcess(); acorda a tcRate = " + rate.str() + " Hz"));
   out.push_back(line(0, CallLineKind::Call, "Station::processTimeCriticalTasks", "(dt = " + secs(dt) + ")",
                      "dt = 1 / tcRate", "Station.cpp:506", structural));
   out.push_back(line(1, CallLineKind::Loop, "for (jj = 0; jj < getFastForwardRate(); jj++)", {},
                      "fastForwardRate = " + std::to_string(p.fastForwardRate)
                      + " -> " + std::to_string(p.fastForwardRate) + " frame(s) por periodo (e assim que [+] acelera)",
                      "Station.cpp:508"));
   out.push_back(line(2, CallLineKind::Call, "Station::tcFrame", "(" + secs(dt) + ")",
                      "tcFrame() so cronometra e chama updateTC()", "Component.cpp:184", structural));
   out.push_back(line(3, CallLineKind::Call, "Station::updateTC", "(" + secs(dt) + ")", {},
                      "Station.cpp:258"));
   out.push_back(line(4, CallLineKind::Call, "Simulation::tcFrame", "(" + secs(dt) + ")", {}, {}));
   out.push_back(line(5, CallLineKind::Call, "Simulation::updateTC", "(" + secs(dt) + ")", {},
                      "Simulation.cpp:457"));
   out.push_back(line(6, CallLineKind::Assign, "execTime += dt", {},
                      "ANTES do teste de freeze -- por isso pausar tem de deixar de chamar tcFrame(), "
                      "nao so marcar o flag",
                      "Simulation.cpp:462"));
   out.push_back(line(6, CallLineKind::Assign, "dt0 = isFrozen() ? 0.0 : dt", "= " + secs(dt0),
                      p.paused ? std::string{"PAUSADO: dt0 zerado, o mundo nao integra"} : std::string{},
                      "Simulation.cpp:498"));
   out.push_back(line(6, CallLineKind::Loop, "for (f = 0; f < 4; f++)  setPhase(f)", {},
                      "as QUATRO fases do frame -- e o que permite o paralelismo determinista",
                      "Simulation.cpp:548"));
   out.push_back(line(7, CallLineKind::Call, "Simulation::updateTcPlayerList",
                      "(players, dt0/4 = " + secs(dt0 / 4.0) + ", idx, n)",
                      "n = numTcThreads = " + std::to_string(p.numTcThreads)
                      + "; cada thread pega 1 player a cada n -- por isso a divisao e deterministica",
                      "Simulation.cpp:595"));
   out.push_back(line(8, CallLineKind::Call, "AbstractPlayer::tcFrame", "(" + secs(dtPhase) + ")",
                      "um QUARTO do dt do frame", "Simulation.cpp:610"));
   out.push_back(line(9, CallLineKind::Call, "Player::updateTC", "(dt0 = " + secs(dtPhase) + ")", {},
                      "Player.cpp:528"));
   out.push_back(line(10, CallLineKind::Assign, "dt4 = dt * 4", "= " + secs(dt),
                      "o dt desce dividido por 4 e volta multiplicado por 4: modulos que rodam UMA vez a "
                      "cada quatro fases veem o dt do FRAME inteiro",
                      "Player.cpp:561"));
   out.push_back(line(10, CallLineKind::Loop, "switch (getWorldModel()->phase())", {}, {},
                      "Player.cpp:562"));
   out.push_back(line(11, CallLineKind::Call, "case 0: dynamics", "(dt4 = " + secs(dt) + ")",
                      "6-DOF do player (JSBSimModel) + amostra REID_PLAYER_DATA pro Tacview",
                      "Player.cpp:567", ph0));
   out.push_back(line(11, CallLineKind::Call, "case 1: (vazio no Player)", {},
                      "quem transmite sao os SISTEMAS, logo abaixo", "Player.cpp:592", ph12));
   out.push_back(line(11, CallLineKind::Call, "case 2: (vazio no Player)", {},
                      "idem para receber", "Player.cpp:596", ph12));
   out.push_back(line(11, CallLineKind::Call, "case 3: (vazio no Player)", {},
                      "idem para a decisao", "Player.cpp:600", ph3));
   out.push_back(line(10, CallLineKind::Call, "Component::updateTC", "(dt = " + secs(dtPhase) + ")",
                      "RECURSAO: obj->tcFrame(dt) em CADA filho de 'components:' -- e assim que a arvore "
                      "desenhada aqui do lado e percorrida",
                      "Component.cpp:243"));
   out.push_back(line(11, CallLineKind::Call, "System::updateTC", "(" + secs(dtPhase) + ")",
                      "todo sensor/piloto/agente e um models::System", "System.cpp:84"));
   out.push_back(line(12, CallLineKind::Loop, "switch (sim->phase())", {},
                      "AQUI e onde a fase vira comportamento de verdade", "System.cpp:106"));
   out.push_back(line(13, CallLineKind::Call, "case 0: dynamics", "(dt4 = " + secs(dt) + ")",
                      "dinamica propria do subsistema", "System.cpp:108", ph0));
   out.push_back(line(13, CallLineKind::Call, "case 1: transmit", "(dt4 = " + secs(dt) + ")",
                      "antena/radar EMITEM", "System.cpp:112", ph12));
   out.push_back(line(13, CallLineKind::Call, "case 2: receive", "(dt4 = " + secs(dt) + ")",
                      "antena/radar RECEBEM; o TrackManager forma as pistas", "System.cpp:116", ph12));
   out.push_back(line(13, CallLineKind::Call, "case 3: process", "(dt4 = " + secs(dt) + ")",
                      "a DECISAO: Autopilot, UbfArbiter, FlightAgentTC -- e o ( FlightAgentTC ) das pocs "
                      "multi-thread decide exatamente aqui",
                      "System.cpp:120", ph3));
   return out;
}

//------------------------------------------------------------------------------
// A cadeia de FUNDO. Neste app ela nao parte de uma thread do framework: o
// proprio laco de simThread (DashboardLoop.cpp) faz o papel da
// StationBgPeriodicThread, chamando station->updateData(dt) -- ver o
// comentario grande de app::BackgroundInfo (aba F4).
//------------------------------------------------------------------------------
std::vector<CallChainLine> backgroundChain(const EstimatedPhase phase, const FrameCallParams& p)
{
   const double dt{p.bgRateHz > 0.0 ? (1.0 / p.bgRateHz) : 0.0};
   const double dt0{p.paused ? 0.0 : dt};
   const bool decision = (phase == EstimatedPhase::DecisionBackground);
   const bool background = (phase == EstimatedPhase::Background);

   std::ostringstream rate;
   rate << std::fixed << std::setprecision(1) << p.bgRateHz;

   std::vector<CallChainLine> out;
   out.push_back(line(0, CallLineKind::Thread, "laco de fundo do ./app (simThread)", {},
                      "este app NAO cria a StationBgPeriodicThread nativa -- o proprio laco a "
                      "~" + rate.str() + " Hz faz esse papel (ver a aba F4)"));
   out.push_back(line(0, CallLineKind::Call, "Station::updateData", "(dt = " + secs(dt) + ")", {},
                      "Station.cpp:323", background));
   out.push_back(line(1, CallLineKind::Call, "Station::processBackgroundTasks", "(dt)",
                      "so quando bgRate == 0 e nao ha thread de fundo propria", "Station.cpp:516"));
   out.push_back(line(2, CallLineKind::Call, "AbstractIoHandler::updateData", "(dt)",
                      "e por aqui que o joystick entra (shared/xjoystick)", "Station.cpp:523"));
   out.push_back(line(2, CallLineKind::Call, "Simulation::updateData", "(dt)", {},
                      "Station.cpp:527"));
   out.push_back(line(3, CallLineKind::Assign, "dt0 = isFrozen() ? 0.0 : dt", "= " + secs(dt0),
                      p.paused ? std::string{"PAUSADO: o caminho de fundo tambem congela"} : std::string{},
                      "Simulation.cpp:625"));
   out.push_back(line(3, CallLineKind::Call, "Simulation::updatePlayerList", "()",
                      "materializa quem nasceu no frame: o missil liberado, o fantasma que chegou por DIS",
                      "Simulation.cpp:631"));
   out.push_back(line(3, CallLineKind::Call, "Simulation::updateBgPlayerList", "(players, dt0, idx, n)",
                      "mesma divisao por thread da versao T/C", "Simulation.cpp:639"));
   out.push_back(line(4, CallLineKind::Call, "Player::updateData", "(" + secs(dt0) + ")", {},
                      "Player.cpp:619"));
   out.push_back(line(5, CallLineKind::Call, "Player::updateElevation", "()",
                      "o terreno e consultado AQUI, no fundo -- nao no frame T/C; e por isso que o AGL "
                      "pode estar ate 100 ms velho na poc multi-thread",
                      "Player.cpp:630"));
   out.push_back(line(5, CallLineKind::Call, "Component::updateData", "(dt)",
                      "RECURSAO: obj->updateData(dt) DIRETO nos filhos -- nao ha bgFrame(); as duas "
                      "recursoes (T/C e fundo) NAO sao simetricas",
                      "Component.cpp:269"));
   out.push_back(line(6, CallLineKind::Call, "ubf::Agent::updateData", "(dt)",
                      "o ( SimAgent ) da poc single-thread decide AQUI, fora do frame", "Agent.cpp:59", decision));
   out.push_back(line(7, CallLineKind::Call, "ubf::Agent::controller", "(dt)",
                      "updateState(actor) -> genAction(state, dt) -> action->execute(actor)",
                      "Agent.cpp:64", decision));
   out.push_back(line(1, CallLineKind::Call, "Station::processNetworkInputTasks", "(dt)",
                      "DIS entra por aqui", "Station.cpp:342", background));
   out.push_back(line(1, CallLineKind::Call, "Station::processNetworkOutputTasks", "(dt)", {},
                      "Station.cpp:343", background));
   out.push_back(line(1, CallLineKind::Call, "AbstractDataRecorder::processRecords", "()",
                      "drena a fila do gravador -> shared/xtacview -> Tacview. Sem esta chamada o "
                      "Tacview nao recebe nada",
                      "Station.cpp:349", background));
   return out;
}

}   // namespace

std::vector<CallChainLine> frameDescentPath(const std::vector<CallChainLine>& chain)
{
   // Varre de TRAS pra frente: ao achar uma linha ativa, passa a aceitar
   // qualquer linha de profundidade estritamente menor -- que e exatamente
   // a definicao de "ancestral" numa lista indentada em pre-ordem. Uma
   // segunda linha ativa mais acima reabre o limite (as fases 1 e 2 marcam
   // duas chamadas irmas, por exemplo).
   std::vector<CallChainLine> reversed;
   int wanted{-1};   // profundidade maxima ainda aceita; -1 = nada aceito ainda
   for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
      const bool isAncestor{wanted >= 0 && it->depth < wanted};
      if (it->active || isAncestor) {
         reversed.push_back(*it);
         wanted = it->active ? std::max(wanted, it->depth) : it->depth;
      }
   }
   return std::vector<CallChainLine>(reversed.rbegin(), reversed.rend());
}

std::string nodeCallLabel(const EstimatedPhase nodePhase, const EstimatedPhase flowPhase,
                          const FrameCallParams& params)
{
   if (nodePhase != flowPhase) return {};

   // Curto de proposito: isto e desenhado NO CANVAS, embaixo do nome do no,
   // e cada caractere custa 2 px de largura (Canvas::DrawText) num espaco
   // que ja e disputado pelos irmaos.
   std::ostringstream os;
   os << std::fixed << std::setprecision(3);

   switch (nodePhase) {
      case EstimatedPhase::Structural:
         os << "tcFrame(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::DynamicsPhase0:
         os << "dynamics(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::SensorPhase1And2:
         os << "tx+rx(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::DecisionPhase3:
         os << "process(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::DecisionBackground:
         os << "controller(" << (params.bgRateHz > 0.0 ? 1.0 / params.bgRateHz : 0.0) << "s)";
         break;
      case EstimatedPhase::Background:
         os << "updateData(" << (params.bgRateHz > 0.0 ? 1.0 / params.bgRateHz : 0.0) << "s)";
         break;
      case EstimatedPhase::Unknown:
      default:
         return {};
   }
   return os.str();
}

double frameStepSeconds(const FrameCallParams& params)
{
   return (params.tcRateHz > 0.0) ? (1.0 / params.tcRateHz) : 0.0;
}

bool isTimeCriticalPhase(const EstimatedPhase phase)
{
   switch (phase) {
      case EstimatedPhase::Structural:
      case EstimatedPhase::DynamicsPhase0:
      case EstimatedPhase::SensorPhase1And2:
      case EstimatedPhase::DecisionPhase3:
         return true;
      default:
         return false;
   }
}

std::vector<CallChainLine> buildFrameCallChain(const EstimatedPhase phase, const FrameCallParams& params)
{
   return isTimeCriticalPhase(phase) ? timeCriticalChain(phase, params) : backgroundChain(phase, params);
}

} // namespace app
