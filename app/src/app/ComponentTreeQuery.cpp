#include "app/ComponentTreeQuery.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/simulation/AbstractDataRecorder.hpp"
#include "mixr/simulation/Simulation.hpp"
#include "mixr/simulation/Station.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Identifier.hpp"
#include "mixr/base/List.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/concepts/linkage/AbstractIoHandler.hpp"

#include <cctype>
#include <cxxabi.h>

#include <algorithm>
#include <cstdlib>
#include <memory>

namespace app {

namespace {

// Teto de seguranca -- getComponents() so DESCE (confirmado lendo
// contexts/src/mixr/src/base/Component.cpp: nenhum caminho de volta ao
// container), entao nao ha ciclo em uso normal. Mas um .epp malformado (ou
// um modelo de terceiro estranho, ver models/plugins/) nao pode travar o
// dashboard -- os dois numeros abaixo sao generosos pro maior cenario deste
// repositorio (4 falcons x ~10 subsistemas cada, algumas dezenas de nos) e
// ainda assim finitos.
const int kMaxDepth{24};
const std::size_t kMaxNodes{4000};

// RTTI pura, sem incluir header nenhum do modelo -- mesma tecnica de
// app/DashboardState.cpp::demangledClassName(), generalizada pra qualquer
// mixr::base::Object (nao so Player): funciona pra qualquer classe de
// qualquer .so carregado, inclusive um modelo de terceiro desconhecido.
std::string demangledTypeName(const mixr::base::Object* const obj)
{
   if (obj == nullptr) return "?";
   int status{};
   const std::unique_ptr<char, void(*)(void*)> demangled{
      abi::__cxa_demangle(typeid(*obj).name(), nullptr, nullptr, &status),
      std::free};
   if (status != 0 || demangled == nullptr) return "?";

   std::string name{demangled.get()};
   const std::string prefix{"mixr::"};
   if (name.rfind(prefix, 0) == 0) name = name.substr(prefix.size());
   return name;
}

std::string toLowerAscii(const std::string& s)
{
   std::string out(s.size(), '\0');
   std::transform(s.begin(), s.end(), out.begin(),
                  [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
   return out;
}

bool containsAny(const std::string& hay, const std::initializer_list<const char*> needles)
{
   for (const char* const n : needles) {
      if (hay.find(n) != std::string::npos) return true;
   }
   return false;
}

//------------------------------------------------------------------------------
// A UNICA parte deste arquivo que e PALPITE, nao fato observado -- ver o
// comentario grande de EstimatedPhase no header. Ordem de checagem IMPORTA
// (a primeira que bater vence):
//
//   1) estrutural (Station/Simulation/Player) -- decidido pelo TIPO C++
//      exato antes de chegar aqui (ver classifyNode()), nao por string.
//   2) 'SimAgent' por CLASSE -- decide em updateData() (ver
//      ubf::Agent::updateData() no CLAUDE.md), diferente de todo resto do
//      grupo "decisao".
//   3) classes/slots de decisao dentro do frame (fase 3): Autopilot/
//      BtBehavior/FlightAgentTC/RLBridgeBehavior/UbfArbiter/
//      AltitudeSafetyBehavior por classe, ou slot 'pilot'/'behavior'/
//      'agent' por nome -- cobre tanto o agente em si quanto os nos do
//      arbitro UBF pendurados nele.
//   4) slot 'dynamicsModel' -- fase 0.
//   5) sensor/antena/gimbal/datalink, por slot OU classe -- fases 1 e 2
//      (a heuristica nao distingue as duas: um Gimbal/Antenna participa
//      de transmit() E receive() dependendo do papel).
//   6) dataRecorder/ioHandler/networks -- updateData(), thread de fundo.
//   7) sem regra -> Unknown (mostrado como tal na UI, nao escondido).
//------------------------------------------------------------------------------
EstimatedPhase estimatePhase(const std::string& slotName, const std::string& className,
                             const bool isStructural)
{
   if (isStructural) return EstimatedPhase::Structural;

   const std::string slot{toLowerAscii(slotName)};
   const std::string cls{toLowerAscii(className)};

   if (cls.find("simagent") != std::string::npos) return EstimatedPhase::DecisionBackground;

   if (containsAny(cls, {"flightagenttc", "btbehavior", "rlbridgebehavior", "autopilot",
                         "ubfarbiter", "altitudesafetybehavior", "onnxpolicy", "pydecide"})
       || containsAny(slot, {"pilot", "behavior", "agent"})) {
      return EstimatedPhase::DecisionPhase3;
   }

   if (slot.find("dynamicsmodel") != std::string::npos) return EstimatedPhase::DynamicsPhase0;

   if (containsAny(slot, {"sensor", "antenna", "gimbal", "rfsensor", "radar", "datalink"})
       || containsAny(cls, {"gimbal", "antenna", "sensormgr", "datalink", "rfsensor"})) {
      return EstimatedPhase::SensorPhase1And2;
   }

   if (containsAny(slot, {"datarecorder", "iohandler", "networks", "msgfeed", "outputhandler"})) {
      return EstimatedPhase::Background;
   }

   return EstimatedPhase::Unknown;
}

void addChild(ComponentTreeNode& parent, mixr::base::Object* const obj, const std::string& slotName,
             const int depth, std::size_t& nodeCount);

// Generico: TODO objeto MIXR que seja Component pode ter filhos via
// 'components:' -- e a travessia que reconstroi dynamicsModel/pilot/
// sensores/obc/agent (e o que estiver DENTRO deles) sem saber nada
// especifico do modelo 'flight'. getComponents() devolve ref()'d
// (Component.hpp) -- unref() ao terminar.
void appendComponentChildren(ComponentTreeNode& node, mixr::base::Component* const comp,
                             const int depth, std::size_t& nodeCount)
{
   if (comp == nullptr || depth > kMaxDepth || nodeCount > kMaxNodes) return;

   mixr::base::PairStream* const kids{comp->getComponents()};
   if (kids == nullptr) return;

   for (mixr::base::List::Item* item = kids->getFirstItem();
        item != nullptr && nodeCount <= kMaxNodes; item = item->getNext()) {
      auto* const pair{static_cast<mixr::base::Pair*>(item->getValue())};
      if (pair == nullptr) continue;
      const std::string slotName{(pair->slot() != nullptr) ? pair->slot()->getString() : std::string{}};
      addChild(node, pair->object(), slotName, depth, nodeCount);
   }
   kids->unref();
}

// 'players:' NAO passa por Component::components (ver o cabecalho do
// header) -- Simulation guarda a lista num safe_ptr proprio. Cada player
// vira filho de um no sintetico "players", e dali pra baixo a recursao
// volta a ser generica (um Player E' Component).
void appendPlayers(ComponentTreeNode& simNode, mixr::simulation::Simulation* const sim,
                   const int depth, std::size_t& nodeCount)
{
   mixr::base::PairStream* const players{sim->getPlayers()};
   if (players == nullptr) return;

   ComponentTreeNode playersNode;
   playersNode.slotName = "players";
   playersNode.className = "(lista)";
   playersNode.phase = EstimatedPhase::Structural;

   for (mixr::base::List::Item* item = players->getFirstItem();
        item != nullptr && nodeCount <= kMaxNodes; item = item->getNext()) {
      auto* const pair{static_cast<mixr::base::Pair*>(item->getValue())};
      if (pair == nullptr) continue;
      const std::string slotName{(pair->slot() != nullptr) ? pair->slot()->getString() : std::string{}};
      addChild(playersNode, pair->object(), slotName, depth, nodeCount);
   }
   players->unref();

   simNode.children.push_back(std::move(playersNode));
}

// 'simulation'/'dataRecorder'/'ioHandler'/'networks' -- os quatro membros
// da Station que TAMBEM nao passam por Component::components (confirmado
// lendo Station::setSlotSimulation()/setDataRecorder()/setSlotIoHandler():
// os tres so fazem 'container(this)' + ref(), nunca processComponents()).
// Sem isto a arvore desta aba mostraria SO o que estiver dentro de
// 'components:' da Station (nos cenarios deste app, so o PluginLoader) --
// nenhum player, nenhum gravador.
void appendStationExtras(ComponentTreeNode& stationNode, mixr::simulation::Station* const station,
                         const int depth, std::size_t& nodeCount)
{
   if (auto* const sim = station->getSimulation()) {
      addChild(stationNode, sim, "simulation", depth, nodeCount);
   }
   if (auto* const dr = station->getDataRecorder()) {
      addChild(stationNode, dr, "dataRecorder", depth, nodeCount);
   }
   if (auto* const io = station->getIoHandler()) {
      addChild(stationNode, io, "ioHandler", depth, nodeCount);
   }

   mixr::base::PairStream* const nets{station->getNetworks()};   // pre-ref()'d
   if (nets != nullptr) {
      ComponentTreeNode netsNode;
      netsNode.slotName = "networks";
      netsNode.className = "(lista)";
      netsNode.phase = EstimatedPhase::Background;
      for (mixr::base::List::Item* item = nets->getFirstItem();
           item != nullptr && nodeCount <= kMaxNodes; item = item->getNext()) {
         auto* const pair{static_cast<mixr::base::Pair*>(item->getValue())};
         if (pair == nullptr) continue;
         const std::string slotName{(pair->slot() != nullptr) ? pair->slot()->getString() : std::string{}};
         addChild(netsNode, pair->object(), slotName, depth, nodeCount);
      }
      nets->unref();
      stationNode.children.push_back(std::move(netsNode));
   }
}

void addChild(ComponentTreeNode& parent, mixr::base::Object* const obj, const std::string& slotName,
             const int depth, std::size_t& nodeCount)
{
   if (obj == nullptr || depth > kMaxDepth || nodeCount > kMaxNodes) return;
   nodeCount += 1;

   ComponentTreeNode node;
   node.slotName = slotName;
   node.className = demangledTypeName(obj);

   const auto* const player{dynamic_cast<const mixr::models::Player*>(obj)};
   node.isPlayer = (player != nullptr);
   if (player != nullptr) {
      node.playerId = static_cast<int>(player->getID());
      if (node.slotName.empty() && player->getName() != nullptr) {
         node.slotName = player->getName()->getString();
      }
   }

   const bool structural{node.isPlayer
      || dynamic_cast<const mixr::simulation::Station*>(obj) != nullptr
      || dynamic_cast<const mixr::simulation::Simulation*>(obj) != nullptr};
   node.phase = estimatePhase(node.slotName, node.className, structural);

   auto* const comp{dynamic_cast<mixr::base::Component*>(obj)};
   if (comp != nullptr) {
      appendComponentChildren(node, comp, depth + 1, nodeCount);

      if (auto* const sim = dynamic_cast<mixr::simulation::Simulation*>(obj)) {
         appendPlayers(node, sim, depth + 1, nodeCount);
      }
      if (auto* const station = dynamic_cast<mixr::simulation::Station*>(obj)) {
         appendStationExtras(node, station, depth + 1, nodeCount);
      }
      // NAO ha uma quarta checagem aqui para 'mixr::recorder::DataRecorder::
      // getOutputHandler()' -- os dois overloads sao 'protected' no header
      // instalado pelo Conan (confirmado tentando: erro de compilacao, nao
      // suposicao). Diferente de 'players'/'simulation'/'dataRecorder'/
      // 'ioHandler'/'networks' (todos getters PUBLICOS de Station/
      // Simulation), o outputHandler do gravador simplesmente NAO tem
      // acessor publico -- e o MIXR e dependencia binaria, sem remendar
      // header nenhum (mesma regra ja aplicada a shared/xlog/shared/xmsg).
      // Por isso o no 'dataRecorder' aparece na arvore SEM filhos: e honesto
      // (nao inventa uma cadeia que este app nao consegue alcancar), nao um
      // bug -- registrado tambem no relatorio desta tarefa como limite
      // conhecido da PRIMEIRA METADE da feature.
   }

   parent.children.push_back(std::move(node));
}

}   // namespace

std::string phaseLabel(const EstimatedPhase phase)
{
   switch (phase) {
      case EstimatedPhase::Structural:         return "estrutural";
      case EstimatedPhase::DynamicsPhase0:      return "fase 0 (dynamics)";
      case EstimatedPhase::SensorPhase1And2:    return "fases 1/2 (transmit/receive)";
      case EstimatedPhase::DecisionPhase3:      return "fase 3 (decisao, no frame T/C)";
      case EstimatedPhase::DecisionBackground:  return "decisao em updateData() (fundo)";
      case EstimatedPhase::Background:          return "updateData() (fundo)";
      case EstimatedPhase::Unknown:
      default:                                 return "desconhecida";
   }
}

bool isHeuristicPhase(const EstimatedPhase phase)
{
   return phase != EstimatedPhase::Structural;
}

ComponentTreeNode discoverComponentTree(mixr::simulation::Station* const station)
{
   ComponentTreeNode root;
   if (station == nullptr) {
      root.className = "?";
      return root;
   }

   std::size_t nodeCount{1};   // conta a propria raiz
   root.slotName = "";
   root.className = demangledTypeName(station);
   root.phase = EstimatedPhase::Structural;

   appendComponentChildren(root, station, 1, nodeCount);
   appendStationExtras(root, station, 1, nodeCount);

   return root;
}

} // namespace app
