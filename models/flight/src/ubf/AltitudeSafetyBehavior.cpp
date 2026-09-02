#include "ubf/AltitudeSafetyBehavior.hpp"

#include "domain/TerrainFloor.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"

#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Distances.hpp"

#include <algorithm>

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(AltitudeSafetyBehavior, "AltitudeSafetyBehavior")

BEGIN_SLOTTABLE(AltitudeSafetyBehavior)
   "minAltitude",       // 1
   "recoverAltitude",   // 2
   "recoverSpeed",      // 3
   "minClearance",      // 4
   "recoverClearance",  // 5
END_SLOTTABLE(AltitudeSafetyBehavior)

BEGIN_SLOT_MAP(AltitudeSafetyBehavior)
   ON_SLOT(1, setSlotMinAltitude,      base::Distance)
   ON_SLOT(2, setSlotRecoverAltitude,  base::Distance)
   ON_SLOT(3, setSlotRecoverSpeed,     base::Number)
   ON_SLOT(4, setSlotMinClearance,     base::Distance)
   ON_SLOT(5, setSlotRecoverClearance, base::Distance)
END_SLOT_MAP()

EMPTY_DELETEDATA(AltitudeSafetyBehavior)

AltitudeSafetyBehavior::AltitudeSafetyBehavior()
{
   STANDARD_CONSTRUCTOR()
}

void AltitudeSafetyBehavior::copyData(const AltitudeSafetyBehavior& org, const bool)
{
   BaseClass::copyData(org);

   minAltitudeM = org.minAltitudeM;
   recoverAltitudeM = org.recoverAltitudeM;
   recoverSpeedKts = org.recoverSpeedKts;
   minClearanceM = org.minClearanceM;
   recoverClearanceM = org.recoverClearanceM;
}

base::ubf::AbstractAction* AltitudeSafetyBehavior::genAction(
   const base::ubf::AbstractState* const state, const double)
{
   const auto flightState = dynamic_cast<const FlightState*>(state);
   if (flightState == nullptr) return nullptr;

   const FlightState::Snapshot& snap{flightState->snapshot()};
   if (!snap.valid) return nullptr;

   domain::GroundReference ground;
   ground.valid = snap.terrainValid;
   ground.elevationM = snap.terrainElevM;

   const bool belowAbsolute{snap.altitudeM < minAltitudeM};
   const bool belowTerrain{ground.valid && minClearanceM > 0.0
                           && snap.altitudeAglM < minClearanceM};

   if (!belowAbsolute && !belowTerrain) {
      // Nao recomendar acao e legitimo no UBF -- quem decide fica com quem
      // recomendou algo (aqui, a arvore).
      return nullptr;
   }

   domain::FlightCommand cmd;
   cmd.headingDeg = snap.headingDeg;      // mantem o rumo, so recupera altitude

   // Recupera para o mais alto entre a altitude nominal desta aeronave e o
   // terreno mais a folga -- sobre um pico, subir ate a altitude nominal
   // seria subir para dentro dele.
   cmd.altitudeM = std::max(recoverAltitudeM,
                            domain::terrainFloorM(ground, recoverClearanceM, 0.0));
   cmd.speedKts = recoverSpeedKts;

   const auto action = new FlightAction();
   action->setCommand(cmd);
   action->setLabel("SAFETY");
   action->setVote(getVote());
   return action;
}

bool AltitudeSafetyBehavior::setSlotMinAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   minAltitudeM = base::Meters::convertStatic(*msg);
   return (minAltitudeM > 0.0);
}

bool AltitudeSafetyBehavior::setSlotRecoverAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   recoverAltitudeM = base::Meters::convertStatic(*msg);
   return (recoverAltitudeM > 0.0);
}

bool AltitudeSafetyBehavior::setSlotRecoverSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   recoverSpeedKts = msg->getDouble();
   return (recoverSpeedKts > 0.0);
}

// Zero e valido nos dois: desliga a camada AGL e deixa so o piso absoluto.
bool AltitudeSafetyBehavior::setSlotMinClearance(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   minClearanceM = base::Meters::convertStatic(*msg);
   return (minClearanceM >= 0.0);
}

bool AltitudeSafetyBehavior::setSlotRecoverClearance(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   recoverClearanceM = base::Meters::convertStatic(*msg);
   return (recoverClearanceM >= 0.0);
}

} // namespace xnative
} // namespace mixr
