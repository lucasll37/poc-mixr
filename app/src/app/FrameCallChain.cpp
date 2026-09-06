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
   const double dt0{p.paused ? 0.0 : dt};
   const double dtPhase{dt0 / 4.0};

   const bool ph0 = (phase == EstimatedPhase::DynamicsPhase0);
   const bool ph1 = (phase == EstimatedPhase::TransmitPhase1);
   const bool ph2 = (phase == EstimatedPhase::ReceivePhase2);
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
   out.push_back(line(10, CallLineKind::Assign, "dt4 = dt * 4", "= " + secs(dt0),
                      "o dt desce dividido por 4 e volta multiplicado por 4: modulos que rodam UMA vez a "
                      "cada quatro fases veem o dt do FRAME inteiro",
                      "Player.cpp:561"));
   out.push_back(line(10, CallLineKind::Loop, "switch (getWorldModel()->phase())", {}, {},
                      "Player.cpp:562"));
   out.push_back(line(11, CallLineKind::Call, "case 0: dynamics", "(dt4 = " + secs(dt0) + ")",
                      "6-DOF do player (JSBSimModel) + amostra REID_PLAYER_DATA pro Tacview",
                      "Player.cpp:567", ph0));
   out.push_back(line(11, CallLineKind::Call, "case 1: (vazio no Player)", {},
                      "quem transmite sao os SISTEMAS, logo abaixo", "Player.cpp:592", ph1));
   out.push_back(line(11, CallLineKind::Call, "case 2: (vazio no Player)", {},
                      "idem para receber", "Player.cpp:596", ph2));
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
   out.push_back(line(13, CallLineKind::Call, "case 0: dynamics", "(dt4 = " + secs(dt0) + ")",
                      "dinamica propria do subsistema", "System.cpp:108", ph0));
   out.push_back(line(13, CallLineKind::Call, "case 1: transmit", "(dt4 = " + secs(dt0) + ")",
                      "antena/radar EMITEM", "System.cpp:112", ph1));
   out.push_back(line(13, CallLineKind::Call, "case 2: receive", "(dt4 = " + secs(dt0) + ")",
                      "antena/radar RECEBEM; o TrackManager forma as pistas", "System.cpp:116", ph2));
   out.push_back(line(13, CallLineKind::Call, "case 3: process", "(dt4 = " + secs(dt0) + ")",
                      "a DECISAO: Autopilot, UbfArbiter, FlightAgentTC -- e o ( FlightAgentTC ) das pocs "
                      "multi-thread decide exatamente aqui",
                      "System.cpp:120", ph3));
   return out;
}

}   // namespace

std::vector<CallChainLine> frameDescentPath(const std::vector<CallChainLine>& chain)
{
   // Varre de TRAS pra frente: ao achar uma linha ativa, passa a aceitar
   // qualquer linha de profundidade estritamente menor -- que e exatamente
   // a definicao de "ancestral" numa lista indentada em pre-ordem. Uma
   // segunda linha ativa mais acima reabre o limite (generico o bastante
   // pra cobrir uma fase futura que marque mais de uma chamada irma, mesmo
   // que nenhuma das quatro de hoje faca isso mais -- transmit/receive
   // foram desmembradas, cada uma so marca a propria linha).
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

std::string nodeCallLabel(const EstimatedPhase flowPhase, const FrameCallParams& params)
{
   // A PARTICIPACAO (o "se") ja foi decidida pelo chamador, via
   // ownPhaseMask -- ver o comentario grande no header. Aqui so resta o
   // "o que", que depende so da fase corrente.
   //
   // Curto de proposito: isto e desenhado NO CANVAS, embaixo do nome do no,
   // e cada caractere custa 2 px de largura (Canvas::DrawText) num espaco
   // que ja e disputado pelos irmaos.
   std::ostringstream os;
   os << std::fixed << std::setprecision(3);

   switch (flowPhase) {
      case EstimatedPhase::Structural:
         os << "tcFrame(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::DynamicsPhase0:
         os << "dynamics(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::TransmitPhase1:
         os << "transmit(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::ReceivePhase2:
         os << "receive(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::DecisionPhase3:
         os << "process(" << frameStepSeconds(params) << "s)";
         break;
      case EstimatedPhase::SensorBothPhases:
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

std::vector<CallChainLine> buildFrameCallChain(const EstimatedPhase phase, const FrameCallParams& params)
{
   return timeCriticalChain(phase, params);
}

} // namespace app
