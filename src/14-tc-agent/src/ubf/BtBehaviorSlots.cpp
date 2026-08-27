//
// A FRONTEIRA COM O EDL do BtBehavior -- e so isso.
//
// Uma unica questao: quais nomes o cenario pode ajustar, em que unidade eles
// chegam e o que e valor valido. Todo setter faz as mesmas tres coisas --
// recusa nulo, converte para a unidade interna (metros, graus, segundos,
// nos) e valida a faixa -- escrevendo no BtTuning. Nenhuma decisao de voo
// acontece aqui; ela mora em BtBehavior.cpp.
//
// Separar isto tira ~150 linhas de tabela e setters do arquivo da decisao.
// Continua sendo a MESMA classe: a slottable e o slot map sao membros
// estaticos de BtBehavior, so definidos nesta unidade de traducao.
//
#include "ubf/BtBehavior.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/Times.hpp"

namespace mixr {
namespace xnative {

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

bool BtBehavior::setSlotTreeFile(const base::String* const msg)
{
   if (msg == nullptr || msg->getString() == nullptr) return false;
   tune.treeFile = msg->getString();
   return !tune.treeFile.empty();
}

bool BtBehavior::setSlotPatrolHeading(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   tune.patrolHeadingDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtBehavior::setSlotLegTime(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   tune.legTimeSec = base::Seconds::convertStatic(*msg);
   return (tune.legTimeSec > 0.0);
}

bool BtBehavior::setSlotLegTurn(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   tune.legTurnDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtBehavior::setSlotPatrolAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   tune.patrolAltitudeM = base::Meters::convertStatic(*msg);
   return (tune.patrolAltitudeM > 0.0);
}

bool BtBehavior::setSlotPatrolSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   tune.patrolSpeedKts = msg->getDouble();
   return (tune.patrolSpeedKts > 0.0);
}

bool BtBehavior::setSlotRtbAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   tune.rtbAltitudeM = base::Meters::convertStatic(*msg);
   return (tune.rtbAltitudeM > 0.0);
}

bool BtBehavior::setSlotRtbSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   tune.rtbSpeedKts = msg->getDouble();
   return (tune.rtbSpeedKts > 0.0);
}

bool BtBehavior::setSlotArrivalRadius(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   tune.arrivalRadiusM = base::Meters::convertStatic(*msg);
   return (tune.arrivalRadiusM > 0.0);
}

bool BtBehavior::setSlotFuelReserve(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   tune.fuelReserve = msg->getDouble();
   return (tune.fuelReserve >= 0.0 && tune.fuelReserve <= 1.0);
}

bool BtBehavior::setSlotBreakTurn(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   tune.breakTurnDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool BtBehavior::setSlotEvadeClimb(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   tune.evadeClimbM = base::Meters::convertStatic(*msg);
   return (tune.evadeClimbM >= 0.0);
}

bool BtBehavior::setSlotEvadeSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   tune.evadeSpeedKts = msg->getDouble();
   return (tune.evadeSpeedKts > 0.0);
}

bool BtBehavior::setSlotSupportSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   tune.supportSpeedKts = msg->getDouble();
   return (tune.supportSpeedKts > 0.0);
}

} // namespace xnative
} // namespace mixr
