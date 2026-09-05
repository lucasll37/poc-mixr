#include "ubf/RLBridgeBehavior.hpp"

#include "domain/FlightCommand.hpp"
#include "domain/WorldView.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"

#include "xrlbridge/RLBridge.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(RLBridgeBehavior, "RLBridgeBehavior")
EMPTY_SLOTTABLE(RLBridgeBehavior)
EMPTY_DELETEDATA(RLBridgeBehavior)

RLBridgeBehavior::RLBridgeBehavior()
{
   STANDARD_CONSTRUCTOR()
}

void RLBridgeBehavior::copyData(const RLBridgeBehavior& org, const bool)
{
   BaseClass::copyData(org);
}

namespace {

// domain::WorldView -> xrlbridge::Observation, campo a campo. E a UNICA
// traducao entre o tipo do modelo e o tipo da ponte -- ver o "porque" no
// cabecalho de shared/xrlbridge/RLBridge.hpp.
xrlbridge::Observation toObservation(const domain::WorldView& snap)
{
   xrlbridge::Observation obs;
   obs.valid = snap.valid;
   obs.northM = snap.northM;
   obs.eastM = snap.eastM;
   obs.altitudeM = snap.altitudeM;
   obs.headingDeg = snap.headingDeg;
   obs.speedKts = snap.speedKts;
   obs.rollDeg = snap.rollDeg;
   obs.pitchDeg = snap.pitchDeg;
   obs.fuelFraction = snap.fuelFraction;
   obs.mach = snap.mach;
   obs.gLoad = snap.gLoad;
   obs.alphaDeg = snap.alphaDeg;
   obs.terrainValid = snap.terrainValid;
   obs.terrainElevM = snap.terrainElevM;
   obs.altitudeAglM = snap.altitudeAglM;
   obs.hasContact = snap.hasContact;
   obs.contactName = snap.contactName;
   obs.contactRangeM = snap.contactRangeM;
   obs.contactRelBearingDeg = snap.contactRelBearingDeg;
   obs.contactDeltaAltM = snap.contactDeltaAltM;
   obs.contactNorthM = snap.contactNorthM;
   obs.contactEastM = snap.contactEastM;
   obs.contactAltitudeM = snap.contactAltitudeM;
   obs.hasAlert = snap.hasAlert;
   obs.alertSender = snap.alertSender;
   obs.alertContactName = snap.alertContactName;
   obs.alertNorthM = snap.alertNorthM;
   obs.alertEastM = snap.alertEastM;
   obs.alertAltitudeM = snap.alertAltitudeM;
   obs.alertRangeM = snap.alertRangeM;
   obs.weaponReady = snap.weaponReady;
   return obs;
}

domain::FlightCommand toFlightCommand(const xrlbridge::Command& cmd)
{
   domain::FlightCommand out;
   out.headingDeg = cmd.headingDeg;
   out.altitudeM = cmd.altitudeM;
   out.speedKts = cmd.speedKts;
   return out;
}

} // namespace

//------------------------------------------------------------------------------
// genAction() -- chamado pelo Agent (fase 3), depois de updateState(). Nao
// decide nada: publica o WorldView deste frame em shared/xrlbridge (para o
// host ler depois que o tcFrame() atual terminar) e devolve o comando que o
// host deixou pendente na chamada anterior.
//------------------------------------------------------------------------------
base::ubf::AbstractAction* RLBridgeBehavior::genAction(
   const base::ubf::AbstractState* const state, const double)
{
   const auto flightState = dynamic_cast<const FlightState*>(state);
   if (flightState == nullptr) return nullptr;

   const FlightState::Snapshot& snap{flightState->snapshot()};
   xrlbridge::setObservation(toObservation(snap));
   if (!snap.valid) return nullptr;

   const domain::FlightCommand cmd{toFlightCommand(xrlbridge::getPendingCommand())};

   // Acao PRE-REF'd (o Agent chama unref() depois de executar) -- contrato do
   // UBF: "returns a pre-ref'd Action".
   const auto action = new FlightAction();
   action->setCommand(cmd);
   action->setLabel("RL");
   action->setVote(getVote());
   return action;
}

} // namespace xnative
} // namespace mixr
