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

// clang-format off
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
   "evadeHold",        // 15
   "terrainClearance", // 16
   "launchMinRange",   // 17
   "launchMaxRange",   // 18
   "launchCone",       // 19
   "patrolJitterHeading", // 20
   "patrolMasterSeed",    // 21
   "patrolSeedOverride",  // 22 -- opcional; a PRESENCA do slot e o que importa,
                          //       nao o valor (0 e semente valida) -- ver
                          //       setSlotPatrolSeedOverride() abaixo
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
   ON_SLOT(15, setSlotEvadeHold,      base::Time)
   ON_SLOT(16, setSlotTerrainClearance, base::Distance)
   ON_SLOT(17, setSlotLaunchMinRange, base::Distance)
   ON_SLOT(18, setSlotLaunchMaxRange, base::Distance)
   ON_SLOT(19, setSlotLaunchCone,     base::Angle)
   ON_SLOT(20, setSlotPatrolJitterHeading, base::Angle)
   ON_SLOT(21, setSlotPatrolMasterSeed,    base::Number)
   ON_SLOT(22, setSlotPatrolSeedOverride,  base::Number)
END_SLOT_MAP()

bool BtBehavior::setSlotTreeFile(const base::String* const msg)
// clang-format on
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

bool BtBehavior::setSlotEvadeHold(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   tune.evadeHoldSec = base::Seconds::convertStatic(*msg);
   return (tune.evadeHoldSec >= 0.0);
}

// Zero e valido e significa "desligar a camada de terreno" -- o piso volta a
// ser so o absoluto de domain::ThreatPolicy. E o controle negativo do
// cenario: da para provar que o campo esta vivo sem recompilar nada.
bool BtBehavior::setSlotTerrainClearance(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   tune.terrainClearanceM = base::Meters::convertStatic(*msg);
   return (tune.terrainClearanceM >= 0.0);
}

bool BtBehavior::setSlotLaunchMinRange(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   tune.launchEnv.minRangeM = base::Meters::convertStatic(*msg);
   return (tune.launchEnv.minRangeM >= 0.0);
}

bool BtBehavior::setSlotLaunchMaxRange(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   tune.launchEnv.maxRangeM = base::Meters::convertStatic(*msg);
   return (tune.launchEnv.maxRangeM > 0.0);
}

bool BtBehavior::setSlotLaunchCone(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   tune.launchEnv.coneDeg = base::Degrees::convertStatic(*msg);
   return (tune.launchEnv.coneDeg >= 0.0);
}

// Zero e o default e significa "recurso desligado" -- mesmo raciocinio do
// terrainClearance acima: o dump determinístico so muda se este slot for
// declarado com um valor positivo.
bool BtBehavior::setSlotPatrolJitterHeading(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   tune.patrolJitterHeadingDeg = base::Degrees::convertStatic(*msg);
   return (tune.patrolJitterHeadingDeg >= 0.0);
}

bool BtBehavior::setSlotPatrolMasterSeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   tune.patrolMasterSeed = static_cast<std::uint64_t>(msg->getInt64());
   return true;
}

// So chamado se 'patrolSeedOverride:' aparecer de fato no bloco deste
// player -- e essa CHAMADA (nao o valor) que liga o override. 0 e uma
// semente valida, entao nao pode servir de sentinela de ausencia (mesma
// armadilha ja evitada acima para terrainClearance/patrolJitterHeading, so
// que ali zero E o "desligado" -- aqui a ausencia do slot e que e).
bool BtBehavior::setSlotPatrolSeedOverride(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   tune.patrolSeedOverride = static_cast<std::uint64_t>(msg->getInt64());
   tune.patrolSeedOverrideSet = true;
   return true;
}

} // namespace xnative
} // namespace mixr
