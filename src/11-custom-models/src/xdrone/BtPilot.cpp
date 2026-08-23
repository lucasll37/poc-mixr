#include "xdrone/BtPilot.hpp"

#include "xdrone/Drone.hpp"
#include "xdrone/DroneDynamics.hpp"
#include "xdrone/FuelSystem.hpp"
#include "xdrone/ProximitySensor.hpp"
#include "xdrone/runtime_utils.hpp"

#include "bt/bt_factory.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/Times.hpp"

#include <exception>

namespace mixr {
namespace xdrone {

IMPLEMENT_SUBCLASS(BtPilot, "BtPilot")

BEGIN_SLOTTABLE(BtPilot)
   "treeFile",         //  1: XML da arvore
   "patrolHeading",    //  2
   "legTime",          //  3
   "legTurn",          //  4
   "patrolAltitude",   //  5
   "patrolSpeed",      //  6
   "rtbAltitude",      //  7
   "rtbSpeed",         //  8
   "arrivalRadius",    //  9
   "breakTurn",        // 10
   "evadeClimb",       // 11
   "evadeSpeed",       // 12
END_SLOTTABLE(BtPilot)

BEGIN_SLOT_MAP(BtPilot)
   ON_SLOT( 1, setSlotTreeFile,       base::String)
   ON_SLOT( 2, setSlotPatrolHeading,  base::Angle)
   ON_SLOT( 3, setSlotLegTime,        base::Time)
   ON_SLOT( 4, setSlotLegTurn,        base::Angle)
   ON_SLOT( 5, setSlotPatrolAltitude, base::Distance)
   ON_SLOT( 6, setSlotPatrolSpeed,    base::Number)
   ON_SLOT( 7, setSlotRtbAltitude,    base::Distance)
   ON_SLOT( 8, setSlotRtbSpeed,       base::Number)
   ON_SLOT( 9, setSlotArrivalRadius,  base::Distance)
   ON_SLOT(10, setSlotBreakTurn,      base::Angle)
   ON_SLOT(11, setSlotEvadeClimb,     base::Distance)
   ON_SLOT(12, setSlotEvadeSpeed,     base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(BtPilot)

namespace {
// Serializa a construcao das arvores: varios BtPilot podem chegar ao
// primeiro process() ao mesmo tempo, em threads T/C diferentes.
std::mutex g_treeBuildMutex;
}

BtPilot::BtPilot()
{
   STANDARD_CONSTRUCTOR()
}

void BtPilot::copyData(const BtPilot& org, const bool)
{
   BaseClass::copyData(org);

   treeFile = org.treeFile;
   patrolHeadingDeg = org.patrolHeadingDeg;
   legTimeSec = org.legTimeSec;
   legTurnDeg = org.legTurnDeg;
   patrolAltitudeM = org.patrolAltitudeM;
   patrolSpeedKts = org.patrolSpeedKts;
   rtbAltitudeM = org.rtbAltitudeM;
   rtbSpeedKts = org.rtbSpeedKts;
   arrivalRadiusM = org.arrivalRadiusM;
   breakTurnDeg = org.breakTurnDeg;
   evadeClimbM = org.evadeClimbM;
   evadeSpeedKts = org.evadeSpeedKts;

   // A arvore NAO e copiada (BT::Tree e move-only e cada player precisa da
   // sua propria, com o proprio blackboard): a copia reconstroi no
   // primeiro process().
   treeBuilt = false;
   treeValid = false;
   tickCount.store(0);
   lastThreadTag.store(-1);
   lastCpu.store(-1);
   state = "INIT";
}

void BtPilot::reset()
{
   BaseClass::reset();

   configurePlans();
   patrol.reset();

   // Forca a reconstrucao da arvore: um RESET_EVENT pode ter mudado os
   // subsistemas irmaos que injetamos no blackboard.
   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeBuilt = false;
   treeValid = false;

   tickCount.store(0);
   setState("INIT");
}

void BtPilot::configurePlans()
{
   patrol.configure(patrolHeadingDeg, legTimeSec, legTurnDeg,
                     patrolAltitudeM, patrolSpeedKts);

   // A "base" e a origem da area de jogo (o ponto de referencia
   // latitude/longitude do WorldModel), que e onde os drones decolam.
   rtb.configure(0.0, 0.0, arrivalRadiusM, rtbAltitudeM, rtbSpeedKts);

   domain::EvasionLimits limits;
   limits.breakTurnDeg = breakTurnDeg;
   limits.climbM = evadeClimbM;
   limits.dashSpeedKts = evadeSpeedKts;
   threat.setLimits(limits);
}

void BtPilot::setState(const std::string& newState)
{
   std::string previous;
   {
      std::lock_guard<std::mutex> lock(stateMutex);
      if (state == newState) return;
      previous = state;
      state = newState;
   }

   // Log so na TRANSICAO (e sob mutex proprio): imprimir a cada frame, de
   // varias threads, embaralharia a saida e mascararia o comportamento.
   const models::Player* const own{getOwnship()};
   const std::string name{(own != nullptr && own->getName() != nullptr)
                          ? own->getName()->getString() : "?"};
   logLine("  [bt] " + name + ": " + previous + " -> " + newState +
           " (thread " + std::to_string(threadTag()) + ")");
}

std::string BtPilot::getState() const
{
   std::lock_guard<std::mutex> lock(stateMutex);
   return state;
}

//------------------------------------------------------------------------------
// FASE 3 -- tick da arvore de comportamento, dentro da thread T/C.
//------------------------------------------------------------------------------
void BtPilot::process(const double dt)
{
   if (dt <= 0.0) return;

   lastThreadTag.store(threadTag(), std::memory_order_relaxed);
   lastCpu.store(currentCpu(), std::memory_order_relaxed);

   if (!treeBuilt) buildTree();
   if (!treeValid) return;

   frameDt = dt;
   tree.tickRoot();
   tickCount.fetch_add(1, std::memory_order_relaxed);
}

void BtPilot::buildTree()
{
   // Uma tentativa so: se falhar, nao fica repetindo a cada frame (mesmo
   // padrao do RecorderNetOutput nativo e do nosso TacviewOutput).
   treeBuilt = true;

   const auto drone = dynamic_cast<Drone*>(getOwnship());
   if (drone == nullptr) {
      logLine("[BtPilot] ownship nao e um Drone -- arvore nao construida");
      return;
   }
   if (treeFile.empty()) {
      logLine("[BtPilot] slot 'treeFile' vazio -- arvore nao construida");
      return;
   }

   // Dependencias fixas do no vao pelo CONSTRUTOR (registerBuilder), nao
   // pelo blackboard -- ver bt/NodeContext.hpp. Cada player tem o seu
   // contexto, entao nada e compartilhado entre aeronaves: e isso que
   // torna o tick paralelo seguro sem lock nosso no caminho quente.
   bt_nodes::NodeContext context;
   context.pilot = this;
   context.drone = drone;
   context.fuel = drone->getFuelSystem();
   context.sensor = drone->getProximitySensor();
   context.dynamics = drone->getDynamicsModel();

   if (!context.complete()) {
      logLine("[BtPilot] faltam subsistemas no player (fuel/sensor/dynamicsModel)"
              " -- arvore nao construida");
      return;
   }

   std::lock_guard<std::mutex> lock(g_treeBuildMutex);

   bt_nodes::registerNodes(btFactory, context);

   try {
      // Blackboard proprio (vazio): a arvore nao precisa dele para as
      // dependencias, mas o BT.CPP exige um por arvore e ele continua
      // disponivel para dados que fluam ENTRE nos.
      tree = btFactory.createTreeFromFile(treeFile, BT::Blackboard::create());
      treeValid = true;
   } catch (const std::exception& ex) {
      logLine(std::string("[BtPilot] falha ao carregar a arvore: ") + ex.what());
      treeValid = false;
   }
}

//------------------------------------------------------------------------------
// shutdownNotification() -- convencao do framework (ver models/System.hpp,
// nota 3): no SHUTDOWN_EVENT, cada System solta o que criou. A arvore nao
// e um objeto MIXR (nao tem ref-count), mas e recurso nosso: paramos o
// tick e a liberamos aqui, em vez de depender da ordem de destruicao.
//------------------------------------------------------------------------------
bool BtPilot::shutdownNotification()
{
   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeValid = false;
   treeBuilt = true;   // nao reconstruir depois do shutdown

   return BaseClass::shutdownNotification();
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool BtPilot::setSlotTreeFile(const base::String* const msg)
{
   if (msg == nullptr) return false;
   treeFile = msg->getString();
   return !treeFile.empty();
}

bool BtPilot::setSlotPatrolHeading(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   patrolHeadingDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtPilot::setSlotLegTime(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   legTimeSec = base::Seconds::convertStatic(*msg);
   return (legTimeSec > 0.0);
}

bool BtPilot::setSlotLegTurn(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   legTurnDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtPilot::setSlotPatrolAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   patrolAltitudeM = base::Meters::convertStatic(*msg);
   return (patrolAltitudeM > 0.0);
}

bool BtPilot::setSlotPatrolSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   patrolSpeedKts = msg->getDouble();
   return (patrolSpeedKts > 0.0);
}

bool BtPilot::setSlotRtbAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   rtbAltitudeM = base::Meters::convertStatic(*msg);
   return (rtbAltitudeM > 0.0);
}

bool BtPilot::setSlotRtbSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   rtbSpeedKts = msg->getDouble();
   return (rtbSpeedKts > 0.0);
}

bool BtPilot::setSlotArrivalRadius(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   arrivalRadiusM = base::Meters::convertStatic(*msg);
   return (arrivalRadiusM > 0.0);
}

bool BtPilot::setSlotBreakTurn(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   breakTurnDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtPilot::setSlotEvadeClimb(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   evadeClimbM = base::Meters::convertStatic(*msg);
   return (evadeClimbM >= 0.0);
}

bool BtPilot::setSlotEvadeSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   evadeSpeedKts = msg->getDouble();
   return (evadeSpeedKts > 0.0);
}

} // namespace xdrone
} // namespace mixr
