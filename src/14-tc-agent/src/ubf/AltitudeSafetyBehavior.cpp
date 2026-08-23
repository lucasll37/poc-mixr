#include "ubf/AltitudeSafetyBehavior.hpp"

#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"

#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Distances.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(AltitudeSafetyBehavior, "AltitudeSafetyBehavior")

BEGIN_SLOTTABLE(AltitudeSafetyBehavior)
   "minAltitude",       // 1
   "recoverAltitude",   // 2
   "recoverSpeed",      // 3
END_SLOTTABLE(AltitudeSafetyBehavior)

BEGIN_SLOT_MAP(AltitudeSafetyBehavior)
   ON_SLOT(1, setSlotMinAltitude,     base::Distance)
   ON_SLOT(2, setSlotRecoverAltitude, base::Distance)
   ON_SLOT(3, setSlotRecoverSpeed,    base::Number)
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
}

base::ubf::AbstractAction* AltitudeSafetyBehavior::genAction(
   const base::ubf::AbstractState* const state, const double)
{
   const auto flightState = dynamic_cast<const FlightState*>(state);
   if (flightState == nullptr) return nullptr;

   const FlightState::Snapshot& snap{flightState->snapshot()};
   if (!snap.valid || snap.altitudeM >= minAltitudeM) {
      // Nao recomendar acao e legitimo no UBF -- quem decide fica com quem
      // recomendou algo (aqui, a arvore).
      return nullptr;
   }

   domain::FlightCommand cmd;
   cmd.headingDeg = snap.headingDeg;      // mantem o rumo, so recupera altitude
   cmd.altitudeM = recoverAltitudeM;
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

} // namespace xnative
} // namespace mixr
