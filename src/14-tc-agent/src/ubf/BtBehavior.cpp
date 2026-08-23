#include "ubf/BtBehavior.hpp"

#include "bt/bt_factory.hpp"
#include "ubf/FlightAction.hpp"
#include "xnative/runtime_utils.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/Times.hpp"

#include <exception>
#include <mutex>

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(BtBehavior, "BtBehavior")

BEGIN_SLOTTABLE(BtBehavior)
   "treeFile",         //  1
   "patrolHeading",    //  2
   "legTime",          //  3
   "legTurn",          //  4
   "patrolAltitude",   //  5
   "patrolSpeed",      //  6
   "rtbAltitude",      //  7
   "rtbSpeed",         //  8
   "arrivalRadius",    //  9
   "fuelReserve",      // 10
   "breakTurn",        // 11
   "evadeClimb",       // 12
   "evadeSpeed",       // 13
   "supportSpeed",     // 14
END_SLOTTABLE(BtBehavior)

BEGIN_SLOT_MAP(BtBehavior)
   ON_SLOT( 1, setSlotTreeFile,       base::String)
   ON_SLOT( 2, setSlotPatrolHeading,  base::Angle)
   ON_SLOT( 3, setSlotLegTime,        base::Time)
   ON_SLOT( 4, setSlotLegTurn,        base::Angle)
   ON_SLOT( 5, setSlotPatrolAltitude, base::Distance)
   ON_SLOT( 6, setSlotPatrolSpeed,    base::Number)
   ON_SLOT( 7, setSlotRtbAltitude,    base::Distance)
   ON_SLOT( 8, setSlotRtbSpeed,       base::Number)
   ON_SLOT( 9, setSlotArrivalRadius,  base::Distance)
   ON_SLOT(10, setSlotFuelReserve,    base::Number)
   ON_SLOT(11, setSlotBreakTurn,      base::Angle)
   ON_SLOT(12, setSlotEvadeClimb,     base::Distance)
   ON_SLOT(13, setSlotEvadeSpeed,     base::Number)
   ON_SLOT(14, setSlotSupportSpeed,   base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(BtBehavior)

namespace {
// createTreeFromFile() nao e reentrante e varios avioes chegam ao primeiro
// genAction() ao mesmo tempo, em threads T/C diferentes.
std::mutex g_treeBuildMutex;
}

BtBehavior::BtBehavior()
{
   STANDARD_CONSTRUCTOR()
}

void BtBehavior::copyData(const BtBehavior& org, const bool)
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
   fuelReserve = org.fuelReserve;
   breakTurnDeg = org.breakTurnDeg;
   evadeClimbM = org.evadeClimbM;
   evadeSpeedKts = org.evadeSpeedKts;
   supportSpeedKts = org.supportSpeedKts;

   // A arvore NAO e copiada (BT::Tree e move-only e cada aeronave precisa
   // da sua): a copia reconstroi no primeiro genAction().
   treeBuilt = false;
   treeValid = false;
   plansReady = false;
   snap = FlightState::Snapshot{};
   currentDecision.reset();
}

void BtBehavior::reset()
{
   BaseClass::reset();

   configurePlans();
   patrol.reset();
   plansReady = true;

   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeBuilt = false;
   treeValid = false;
   currentDecision.reset();
}

bool BtBehavior::shutdownNotification()
{
   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeValid = false;
   treeBuilt = true;
   return BaseClass::shutdownNotification();
}

void BtBehavior::configurePlans()
{
   patrol.configure(patrolHeadingDeg, legTimeSec, legTurnDeg, patrolAltitudeM, patrolSpeedKts);
   rtb.configure(0.0, 0.0, arrivalRadiusM, rtbAltitudeM, rtbSpeedKts);

   domain::EvasionLimits limits;
   limits.breakTurnDeg = breakTurnDeg;
   limits.climbM = evadeClimbM;
   limits.dashSpeedKts = evadeSpeedKts;
   threat.setLimits(limits);
}

void BtBehavior::buildTree()
{
   treeBuilt = true;   // uma tentativa so

   if (treeFile.empty()) {
      logLine("[BtBehavior] slot 'treeFile' vazio -- arvore nao construida");
      return;
   }

   bt_nodes::NodeContext context;
   context.behavior = this;

   std::lock_guard<std::mutex> lock(g_treeBuildMutex);
   bt_nodes::registerNodes(btFactory, context);

   try {
      tree = btFactory.createTreeFromFile(treeFile, BT::Blackboard::create());
      treeValid = true;
   } catch (const std::exception& ex) {
      logLine(std::string("[BtBehavior] falha ao carregar a arvore: ") + ex.what());
      treeValid = false;
   }
}

//------------------------------------------------------------------------------
// genAction() -- chamado pelo Agent (fase 3), depois de updateState().
//------------------------------------------------------------------------------
base::ubf::AbstractAction* BtBehavior::genAction(const base::ubf::AbstractState* const state,
                                                 const double dt)
{
   const auto flightState = dynamic_cast<const FlightState*>(state);
   if (flightState == nullptr) return nullptr;

   // Ver a armadilha no cabecalho: reset() pode nunca chegar a um
   // comportamento aninhado no Arbiter, entao a configuracao vinda dos
   // slots e aplicada aqui, na primeira decisao.
   if (!plansReady) {
      configurePlans();
      patrol.reset();
      plansReady = true;
   }

   snap = flightState->snapshot();
   if (!snap.valid) return nullptr;

   frameDt = dt;

   if (!treeBuilt) buildTree();
   if (!treeValid) return nullptr;

   currentDecision.reset();
   tree.tickRoot();
   if (!currentDecision.taken) return nullptr;

   // Acao PRE-REF'd (o Agent chama unref() depois de executar) -- contrato
   // do UBF: "returns a pre-ref'd Action".
   const auto action = new FlightAction();
   action->setCommand(currentDecision.command);
   action->setLabel(currentDecision.label);
   if (currentDecision.broadcastAlert) {
      action->setAlertBroadcast(currentDecision.alertContactName,
                                currentDecision.alertNorthM, currentDecision.alertEastM,
                                currentDecision.alertAltitudeM, currentDecision.alertRangeM);
   }

   // O voto do comportamento vai junto: e por ele que o UbfArbiter escolhe
   // entre esta acao e a de outro comportamento no mesmo frame.
   action->setVote(getVote());
   return action;
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool BtBehavior::setSlotTreeFile(const base::String* const msg)
{
   if (msg == nullptr || msg->getString() == nullptr) return false;
   treeFile = msg->getString();
   return !treeFile.empty();
}

bool BtBehavior::setSlotPatrolHeading(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   patrolHeadingDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtBehavior::setSlotLegTime(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   legTimeSec = base::Seconds::convertStatic(*msg);
   return (legTimeSec > 0.0);
}

bool BtBehavior::setSlotLegTurn(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   legTurnDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtBehavior::setSlotPatrolAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   patrolAltitudeM = base::Meters::convertStatic(*msg);
   return (patrolAltitudeM > 0.0);
}

bool BtBehavior::setSlotPatrolSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   patrolSpeedKts = msg->getDouble();
   return (patrolSpeedKts > 0.0);
}

bool BtBehavior::setSlotRtbAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   rtbAltitudeM = base::Meters::convertStatic(*msg);
   return (rtbAltitudeM > 0.0);
}

bool BtBehavior::setSlotRtbSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   rtbSpeedKts = msg->getDouble();
   return (rtbSpeedKts > 0.0);
}

bool BtBehavior::setSlotArrivalRadius(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   arrivalRadiusM = base::Meters::convertStatic(*msg);
   return (arrivalRadiusM > 0.0);
}

bool BtBehavior::setSlotFuelReserve(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   fuelReserve = msg->getDouble();
   return (fuelReserve >= 0.0 && fuelReserve <= 1.0);
}

bool BtBehavior::setSlotBreakTurn(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   breakTurnDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtBehavior::setSlotEvadeClimb(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   evadeClimbM = base::Meters::convertStatic(*msg);
   return (evadeClimbM >= 0.0);
}

bool BtBehavior::setSlotEvadeSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   evadeSpeedKts = msg->getDouble();
   return (evadeSpeedKts > 0.0);
}

bool BtBehavior::setSlotSupportSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   supportSpeedKts = msg->getDouble();
   return (supportSpeedKts > 0.0);
}

} // namespace xnative
} // namespace mixr
