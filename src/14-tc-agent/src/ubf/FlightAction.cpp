#include "ubf/FlightAction.hpp"

#include "xnative/AlertDatalink.hpp"
#include "xnative/runtime_utils.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/system/Autopilot.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/units/distance_utils.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(FlightAction, "FlightAction")
EMPTY_SLOTTABLE(FlightAction)
EMPTY_DELETEDATA(FlightAction)

FlightAction::FlightAction()
{
   STANDARD_CONSTRUCTOR()
}

void FlightAction::copyData(const FlightAction& org, const bool)
{
   BaseClass::copyData(org);

   command = org.command;
   label = org.label;
   broadcast = org.broadcast;
   alertContactName = org.alertContactName;
   alertNorthM = org.alertNorthM;
   alertEastM = org.alertEastM;
   alertAltitudeM = org.alertAltitudeM;
   alertRangeM = org.alertRangeM;
}

void FlightAction::setAlertBroadcast(const std::string& contactName,
                                     const double northM, const double eastM,
                                     const double altitudeM, const double rangeM)
{
   broadcast = true;
   alertContactName = contactName;
   alertNorthM = northM;
   alertEastM = eastM;
   alertAltitudeM = altitudeM;
   alertRangeM = rangeM;
}

//------------------------------------------------------------------------------
// execute() -- atuacao, agora inteiramente sobre subsistemas NATIVOS.
//
// Compare com a poc/12: la a acao escrevia no NOSSO FlightDirector. Aqui ela
// comanda o models::Autopilot do framework, que por sua vez fala com o
// JSBSimModel (ap/heading_hold, ap/altitude_hold, ap/airspeed_hold) --
// nenhuma lei de controle nossa no caminho.
//
// GOTCHA DE UNIDADE: Autopilot::setCommandedAltitudeFt() e em PES, enquanto
// o domain::FlightCommand (e o resto desta poc) trabalha em metros. A
// conversao acontece aqui, na fronteira.
//
// O rotulo do comportamento vai para o quadro de status (ver
// xnative/runtime_utils.hpp): o Aircraft nativo nao tem onde guarda-lo.
//------------------------------------------------------------------------------
bool FlightAction::execute(base::Component* actor)
{
   const auto player = dynamic_cast<models::Player*>(actor);
   if (player == nullptr) return false;

   base::Pair* const pilotPair{player->getPilotByType(typeid(models::Autopilot))};
   const auto autopilot = (pilotPair != nullptr)
                           ? dynamic_cast<models::Autopilot*>(pilotPair->object())
                           : nullptr;
   if (autopilot == nullptr) return false;

   autopilot->setHeadingHoldMode(true);
   autopilot->setAltitudeHoldMode(true);
   autopilot->setVelocityHoldMode(true);

   autopilot->setCommandedHeadingD(command.headingDeg);
   autopilot->setCommandedAltitudeFt(command.altitudeM * base::distance::M2FT);
   autopilot->setCommandedVelocityKts(command.speedKts);

   setBehaviorLabel(player->getID(), label);

   if (broadcast) {
      const auto datalink = dynamic_cast<AlertDatalink*>(player->getDatalink());
      if (datalink != nullptr) {
         datalink->broadcastAlert(alertContactName, alertNorthM, alertEastM,
                                  alertAltitudeM, alertRangeM);
      }
   }

   return true;
}

} // namespace xnative
} // namespace mixr
