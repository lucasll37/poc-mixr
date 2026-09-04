#include "app/ComponentTreeQuery.hpp"
#include "app/FleetPanel.hpp"   // modeLabel()/sideLabel() -- o MESMO vocabulario da aba F1,
                                // reusado em vez de um switch paralelo que envelheceria sozinho

#include "mixr/models/player/Player.hpp"
#include "mixr/simulation/AbstractDataRecorder.hpp"
#include "mixr/simulation/Simulation.hpp"
#include "mixr/simulation/Station.hpp"

// Os headers abaixo existem SO por captureLiveState() -- o estado VIVO que o
// card de detalhe da aba F6 mostra. Sao todos classes do proprio MIXR (o
// host pode incluir o que quiser do framework; a opacidade que
// tests/guard/check_host_opaco.sh cobra e em relacao ao fonte do MODELO,
// que continua alcancado so por RTTI/nome, nunca por header).
#include "mixr/models/dynamics/DynamicsModel.hpp"
#include "mixr/models/system/Autopilot.hpp"
#include "mixr/models/system/Datalink.hpp"
#include "mixr/models/system/Gimbal.hpp"
#include "mixr/models/system/RfSensor.hpp"
#include "mixr/models/system/trackmanager/TrackManager.hpp"
#include "mixr/models/Track.hpp"
#include "mixr/interop/common/NetIO.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Identifier.hpp"
#include "mixr/base/List.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/concepts/linkage/AbstractIoHandler.hpp"
#include "mixr/base/units/angle_utils.hpp"

#include <cctype>
#include <cxxabi.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <sstream>

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

//------------------------------------------------------------------------------
// ESTADO VIVO -- o oposto de estimatePhase() logo acima: aqui NAO ha palpite
// nenhum, todo campo sai de um getter PUBLICO do proprio objeto MIXR, lido
// no instante da descoberta. E o que responde "o que este componente esta
// fazendo AGORA" no card de detalhe da aba F6.
//
// Duas notas que valem por todo o bloco:
//
//  * A leitura acontece na THREAD DE DESENHO (discoverComponentTree() e
//    chamada do Renderer mais externo, ver DashboardLoop.cpp), enquanto o
//    pool de tempo critico escreve esses mesmos campos. Sao leituras
//    escalares (double/int/bool alinhados) usadas so para EXIBIR -- nenhuma
//    decisao do app depende delas, e um valor de um frame atras nao muda
//    nada visualmente a 10 Hz. A propria travessia da arvore ja tinha essa
//    caracteristica desde a primeira versao da aba.
//
//  * A ORDEM dos dynamic_cast e do mais derivado para o mais generico, e o
//    bloco generico de Component roda SEMPRE no fim -- todo componente tem
//    filhos/freeze/shutdown, mesmo os que nao caem em nenhuma classe
//    reconhecida (esses ficam so com o bloco generico, e o painel diz isso
//    com todas as letras em vez de fingir que nao ha estado).
//------------------------------------------------------------------------------

std::string fmtNum(const double v, const int decimals)
{
   std::ostringstream os;
   os << std::fixed << std::setprecision(decimals) << v;
   return os.str();
}

const char* yesNo(const bool v) { return v ? "sim" : "nao"; }
const char* onOff(const bool v) { return v ? "ON" : "OFF"; }

void addField(std::vector<ComponentStateField>& out, const std::string& label, const std::string& value)
{
   out.push_back(ComponentStateField{label, value});
}

// Conta as entradas de um PairStream pre-ref()'d, liberando-o. Usado pelos
// contadores de 'players'/'networks' da Station -- getPlayers()/getNetworks()
// devolvem ref()'d (ver o cabecalho de Station.hpp).
int countAndRelease(mixr::base::PairStream* const stream)
{
   if (stream == nullptr) return 0;
   const int n{static_cast<int>(stream->entries())};
   stream->unref();
   return n;
}

std::vector<ComponentStateField> captureLiveState(mixr::base::Object* const obj)
{
   std::vector<ComponentStateField> out;
   if (obj == nullptr) return out;

   // ---- Station: as TAXAS que governam as tres threads do framework ----
   if (auto* const station = dynamic_cast<mixr::simulation::Station*>(obj)) {
      addField(out, "taxa T/C", fmtNum(station->getTimeCriticalRate(), 1) + " Hz");
      addField(out, "taxa fundo", fmtNum(station->getBackgroundRate(), 1) + " Hz");
      addField(out, "taxa rede", fmtNum(station->getNetworkRate(), 1) + " Hz");
      // getFastForwardRate() e o multiplicador que Station::
      // processTimeCriticalTasks() usa no 'for' de tcFrame() -- e o que as
      // teclas +/- desta app movem, via xclock::ClockStation::setTimeScale().
      addField(out, "fast-forward", std::to_string(station->getFastForwardRate()) + "x");
      addField(out, "players", std::to_string(countAndRelease(station->getPlayers())));
      addField(out, "redes", std::to_string(countAndRelease(station->getNetworks())));
   }

   // ---- Simulation/WorldModel: o relogio do executivo ----
   if (auto* const sim = dynamic_cast<mixr::simulation::Simulation*>(obj)) {
      addField(out, "tempo exec", fmtNum(sim->getExecTimeSec(), 2) + " s");
      addField(out, "contador exec", std::to_string(sim->getExecCounter()));
      addField(out, "hora sim (UTC)", fmtNum(sim->getSimTimeOfDay(), 1) + " s");
      addField(out, "players", std::to_string(countAndRelease(sim->getPlayers())));
   }

   // ---- Player: o mesmo vocabulario da aba F1 (modeLabel/sideLabel) ----
   if (auto* const player = dynamic_cast<mixr::models::Player*>(obj)) {
      addField(out, "modo", modeLabel(static_cast<int>(player->getMode())));
      addField(out, "lado", sideLabel(static_cast<unsigned int>(player->getSide())));
      addField(out, "altitude", fmtNum(player->getAltitudeM(), 0) + " m MSL");
      addField(out, "AGL", fmtNum(player->getAltitudeAglM(), 0) + " m");
      addField(out, "rumo", fmtNum(player->getHeadingD(), 1) + " deg");
      addField(out, "velocidade", fmtNum(player->getTotalVelocityKts(), 1) + " kt");
      addField(out, "mach", fmtNum(player->getMach(), 3));
      addField(out, "dano", fmtNum(player->getDamage() * 100.0, 0) + " %");
      // Um fantasma que chegou por DIS e "remoto" -- ele nao tem
      // dynamicsModel/pilot locais, so dead reckoning (ver a secao
      // src/poc/bandit-dis do CLAUDE.md). Distinguir isso explica de graca
      // por que a subarvore dele e mais pobre que a de um player local.
      addField(out, "origem", player->isNetworkedPlayer()
                  ? ("remoto (rede " + std::to_string(player->getNetworkID()) + ")")
                  : std::string{"local"});
   }

   // ---- Autopilot: o comando que ele esta impondo AGORA ----
   if (auto* const ap = dynamic_cast<mixr::models::Autopilot*>(obj)) {
      addField(out, "rumo cmd", fmtNum(ap->getCommandedHeadingD(), 1) + " deg  hold "
                                + onOff(ap->isHeadingHoldOn()));
      addField(out, "altitude cmd", fmtNum(ap->getCommandedAltitudeFt(), 0) + " ft  hold "
                                    + onOff(ap->isAltitudeHoldOn()));
      addField(out, "velocidade cmd", fmtNum(ap->getCommandedVelocityKts(), 1) + " kt  hold "
                                      + onOff(ap->isVelocityHoldOn()));
      addField(out, "modo nav", onOff(ap->isNavModeOn()));
      addField(out, "modo loiter", onOff(ap->isLoiterModeOn()));
   }

   // ---- Gimbal/Antenna: para onde a antena esta apontada ----
   if (auto* const gimbal = dynamic_cast<mixr::models::Gimbal*>(obj)) {
      addField(out, "azimute", fmtNum(gimbal->getAzimuthD(), 1) + " deg");
      addField(out, "elevacao", fmtNum(gimbal->getElevationD(), 1) + " deg");
      addField(out, "az cmd", fmtNum(gimbal->getCmdAz() * mixr::base::angle::R2DCC, 1) + " deg");
      addField(out, "el cmd", fmtNum(gimbal->getCmdElev() * mixr::base::angle::R2DCC, 1) + " deg");
      addField(out, "nos limites", yesNo(gimbal->isAtLimits()));
   }

   // ---- RfSensor/Radar ----
   if (auto* const sensor = dynamic_cast<mixr::models::RfSensor*>(obj)) {
      addField(out, "transmitindo", yesNo(sensor->isTransmitting()));
      addField(out, "varrendo", yesNo(sensor->isScanOn()));
      addField(out, "alcance", fmtNum(sensor->getRange(), 1) + " NM");
      addField(out, "PRF", fmtNum(sensor->getPRF(), 0) + " Hz");
   }

   // ---- TrackManager: quantas pistas ele mantem AGORA ----
   if (auto* const tm = dynamic_cast<mixr::models::TrackManager*>(obj)) {
      // getTrackList() com destino nulo nao existe -- e preciso um vetor de
      // verdade; 'kMaxTracks' e generoso pro maior cenario deste repositorio
      // e o retorno e o numero REALMENTE copiado.
      const unsigned int kMaxTracks{64};
      const mixr::models::Track* tracks[kMaxTracks]{};
      const int n{tm->getTrackList(tracks, kMaxTracks)};
      addField(out, "pistas", std::to_string(n));
      addField(out, "idade max de pista", fmtNum(tm->getMaxTrackAge(), 1) + " s");
   }

   // ---- Datalink ----
   if (auto* const dl = dynamic_cast<mixr::models::Datalink*>(obj)) {
      addField(out, "envio local", onOff(dl->isLocalSendEnabled()));
      addField(out, "fila de rede", onOff(dl->isNetworkQueueEnabled()));
   }

   // ---- DynamicsModel (JSBSimModel e um): combustivel/peso/empuxo ----
   if (auto* const dyn = dynamic_cast<mixr::models::DynamicsModel*>(obj)) {
      const double fuel{dyn->getFuelWt()};
      const double fuelMax{dyn->getFuelWtMax()};
      addField(out, "combustivel", fmtNum(fuel, 0) + " / " + fmtNum(fuelMax, 0) + " lb");
      addField(out, "peso bruto", fmtNum(dyn->getGrossWeight(), 0) + " lb");
      const int engines{dyn->getNumberOfEngines()};
      addField(out, "motores", std::to_string(engines));
      if (engines > 0) {
         const int kMaxEngines{8};
         double thrust[kMaxEngines]{};
         const int n{dyn->getEngThrust(thrust, std::min(engines, kMaxEngines))};
         double total{};
         for (int i = 0; i < n; i++) total += thrust[i];
         if (n > 0) addField(out, "empuxo total", fmtNum(total, 0) + " lb");
      }
   }

   // ---- interop::NetIO (o DisNetIO dos cenarios com 'networks:') ----
   if (auto* const net = dynamic_cast<mixr::interop::NetIO*>(obj)) {
      addField(out, "rede id", std::to_string(net->getNetworkID()));
      addField(out, "inicializada", yesNo(net->isNetworkInitialized()));
      addField(out, "entrada", onOff(net->isInputEnabled()));
      addField(out, "saida", onOff(net->isOutputEnabled()));
      // NAO ha contagem de NIBs aqui: getInputListSize()/getOutputListSize()
      // sao PROTECTED em interop::NetIO (confirmado tentando -- erro de
      // compilacao, nao suposicao). Mesma regra ja registrada para o
      // outputHandler do DataRecorder: o MIXR e dependencia binaria, e um
      // getter sem acessor publico simplesmente nao aparece nesta aba.
   }

   // ---- Generico: vale para QUALQUER Component, inclusive um de um modelo
   //      de terceiro que este arquivo nunca viu ----
   if (auto* const comp = dynamic_cast<mixr::base::Component*>(obj)) {
      addField(out, "filhos (components:)", std::to_string(comp->getNumberOfComponents()));
      addField(out, "congelado", yesNo(comp->isFrozen()));
      addField(out, "desligado", yesNo(comp->isShutdown()));
   }

   return out;
}

// Chave ESTAVEL do no (ver ComponentTreeNode::nodeKey). O sufixo "#i" so
// entra quando um irmao ANTERIOR ja usou o mesmo nome -- assim o caso comum
// (nomes de slot, que sao unicos por construcao no EDL) gera uma chave
// legivel e imune a insercao/remocao de irmaos.
std::string makeNodeKey(const ComponentTreeNode& parent, const std::string& name)
{
   int repeats{};
   for (const auto& sibling : parent.children) {
      const std::string sibName{sibling.slotName.empty() ? sibling.className : sibling.slotName};
      if (sibName == name) repeats += 1;
   }
   // A raiz tem chave "/" -- concatenar sem separador extra nesse caso e o
   // que evita "//simulation" (e mantem toda chave comecando por "/").
   const std::string prefix{(!parent.nodeKey.empty() && parent.nodeKey.back() == '/')
                            ? parent.nodeKey : parent.nodeKey + "/"};
   std::string key{prefix + name};
   if (repeats > 0) key += "#" + std::to_string(repeats);
   return key;
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
   playersNode.nodeKey = makeNodeKey(simNode, "players");

   for (mixr::base::List::Item* item = players->getFirstItem();
        item != nullptr && nodeCount <= kMaxNodes; item = item->getNext()) {
      auto* const pair{static_cast<mixr::base::Pair*>(item->getValue())};
      if (pair == nullptr) continue;
      const std::string slotName{(pair->slot() != nullptr) ? pair->slot()->getString() : std::string{}};
      addChild(playersNode, pair->object(), slotName, depth, nodeCount);
   }
   players->unref();

   // Os dois nos SINTETICOS ('players' e 'networks') nao tem objeto MIXR
   // por tras -- captureLiveState() nao tem o que ler neles. O tamanho da
   // lista e o unico estado que existe, e e justamente o que se quer saber
   // com o galho retraido.
   addField(playersNode.state, "itens", std::to_string(playersNode.children.size()));
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
      netsNode.nodeKey = makeNodeKey(stationNode, "networks");
      for (mixr::base::List::Item* item = nets->getFirstItem();
           item != nullptr && nodeCount <= kMaxNodes; item = item->getNext()) {
         auto* const pair{static_cast<mixr::base::Pair*>(item->getValue())};
         if (pair == nullptr) continue;
         const std::string slotName{(pair->slot() != nullptr) ? pair->slot()->getString() : std::string{}};
         addChild(netsNode, pair->object(), slotName, depth, nodeCount);
      }
      nets->unref();
      addField(netsNode.state, "itens", std::to_string(netsNode.children.size()));
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
   node.nodeKey = makeNodeKey(parent, node.slotName.empty() ? node.className : node.slotName);
   node.state = captureLiveState(obj);

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
      root.nodeKey = "/";
      return root;
   }

   std::size_t nodeCount{1};   // conta a propria raiz
   root.slotName = "";
   root.className = demangledTypeName(station);
   root.phase = EstimatedPhase::Structural;
   root.nodeKey = "/";
   root.state = captureLiveState(station);

   appendComponentChildren(root, station, 1, nodeCount);
   appendStationExtras(root, station, 1, nodeCount);

   return root;
}

} // namespace app
