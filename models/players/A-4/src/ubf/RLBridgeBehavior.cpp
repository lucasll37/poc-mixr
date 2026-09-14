#include "ubf/RLBridgeBehavior.hpp"

#include "domain/FlightCommand.hpp"
#include "domain/WorldView.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"
#include "ubf/ObservationBridge.hpp"

#include "xrlbridge/RLBridge.hpp"

namespace mixr {
namespace models {
namespace xA_4 {

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

// toObservation() mora em ubf/ObservationBridge.hpp -- extraida daqui nesta
// mesma passada (era uma copia campo a campo A MAO, sem a garantia de
// compilacao que a macro da) para ficar testavel isoladamente. Ver
// tests/native/test_observation_bridge.cpp.

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
// decide nada: publica o WorldView deste frame em libs/xrlbridge (para o
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

   // ACHADO POR AUDITORIA (nao redescobrir, ver o comentario grande em
   // xrlbridge::Command): sem o host ter publicado uma acao valida ainda
   // (frame de priming do reset(), ou o Command generico ficou obsoleto de
   // um episodio anterior), nao ha recomendacao nenhuma -- devolver nullptr
   // e deixar o UbfArbiter (AltitudeSafetyBehavior continua no candidato
   // vote:90 do MESMO arbitro, ver scenario_rl.edl) decidir sem este voto,
   // em vez de atuar heading=0/altitude=0/speed=0 como se fosse uma
   // decisao de verdade.
   const xrlbridge::Command pending{xrlbridge::getPendingCommand()};
   if (!pending.valid) return nullptr;

   const domain::FlightCommand cmd{toFlightCommand(pending)};

   // Acao PRE-REF'd (o Agent chama unref() depois de executar) -- contrato do
   // UBF: "returns a pre-ref'd Action".
   const auto action = new FlightAction();
   action->setCommand(cmd);
   action->setLabel("RL");
   action->setVote(getVote());
   return action;
}

} // namespace xA_4
} // namespace models
} // namespace mixr
