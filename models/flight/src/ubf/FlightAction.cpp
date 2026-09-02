#include "ubf/FlightAction.hpp"

#include "xnative/AlertDatalink.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/weapon/AbstractWeapon.hpp"
#include "mixr/models/player/weapon/Missile.hpp"
#include "mixr/models/system/Autopilot.hpp"
#include "mixr/models/system/StoresMgr.hpp"

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
   launch = org.launch;
   launchTargetName = org.launchTargetName;
}

void FlightAction::setLaunchRequest(const std::string& targetName)
{
   launch = true;
   launchTargetName = targetName;
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
// execute() -- atuacao, inteiramente sobre subsistemas NATIVOS: comanda o
// models::Autopilot do framework, que por sua vez fala com o JSBSimModel
// (ap/heading_hold, ap/altitude_hold, ap/airspeed_hold) -- nenhuma lei de
// controle propria no caminho.
//
// GOTCHA DE UNIDADE: Autopilot::setCommandedAltitudeFt() e em PES, enquanto
// o domain::FlightCommand (e o resto deste subprojeto) trabalha em metros.
// A conversao acontece aqui, na fronteira.
//
// O rotulo do comportamento vai para o quadro de status (ver
// xnative/BehaviorBoard.hpp): o Aircraft nativo nao tem onde guarda-lo.
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

   // O quadro de leitura (shared/xboard) e a UNICA coisa que este modelo e o
   // host compartilham: escrevemos aqui, o dump e a linha de status leem la.
   // Ele mora numa .so de verdade justamente porque este codigo passou a rodar
   // dentro de um plugin -- ver o cabecalho de shared/xboard/Board.hpp.
   //
   // Conta DECISAO, nao candidatura: estamos depois de o UbfArbiter ter
   // escolhido o vencedor.
   xboard::setBehaviorLabel(player->getID(), label);
   xboard::bumpDecisionCount(player->getID());

   if (broadcast) {
      const auto datalink = dynamic_cast<AlertDatalink*>(player->getDatalink());
      if (datalink != nullptr) {
         datalink->broadcastAlert(alertContactName, alertNorthM, alertEastM,
                                  alertAltitudeM, alertRangeM);
      }
   }

   // --- lancamento de missil -------------------------------------------
   //
   // O UNICO ponto deste modelo que toca um objeto MIXR de arma. StoresMgr e
   // opcional (getStoresManagement() devolve nullptr sem 'stores:' no EDL) --
   // inerte em qualquer aviao de producao.
   //
   // releaseOneMissile() ja faz tudo que o framework nativo oferece: clona o
   // 'missile' do EDL num flyout e o enfileira em Simulation::addNewPlayer()
   // (materializado no proximo updatePlayerList(), no laco de background) --
   // e assim, sem nenhum codigo nosso, que um player novo entra na simulacao
   // EM EXECUCAO. Devolve pre-ref()'d (ver StoresMgr.hpp) -- por isso o
   // unref() no fim.
   if (launch) {
      models::StoresMgr* const storesMgr{player->getStoresManagement()};
      models::WorldModel* const world{player->getWorldModel()};
      if (storesMgr != nullptr && world != nullptr) {
         const auto target = dynamic_cast<models::Player*>(
            world->findPlayerByName(launchTargetName.c_str()));
         if (target != nullptr) {
            models::AbstractWeapon* const flyout{storesMgr->releaseOneMissile()};
            if (flyout != nullptr) {
               flyout->setTargetPlayer(target, true);
               flyout->unref();
            }
         }
      }
   }

   return true;
}

} // namespace xnative
} // namespace mixr
