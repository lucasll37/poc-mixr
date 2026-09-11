#include "ubf/AaaAction.hpp"

#include "xboard/Board.hpp"
#include "xlog/Log.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/weapon/Missile.hpp"
#include "mixr/models/system/StoresMgr.hpp"

namespace mixr {
namespace models {
namespace xaaa {

IMPLEMENT_SUBCLASS(AaaAction, "AaaAction")
EMPTY_SLOTTABLE(AaaAction)
EMPTY_DELETEDATA(AaaAction)

AaaAction::AaaAction()
{
   STANDARD_CONSTRUCTOR()
}

AaaAction::AaaAction(const std::string& label, const bool fireRequested,
                     const std::string& targetName)
   : label_(label), fireRequested_(fireRequested), targetName_(targetName)
{
   STANDARD_CONSTRUCTOR()
}

void AaaAction::copyData(const AaaAction& org, const bool)
{
   BaseClass::copyData(org);
   label_ = org.label_;
   fireRequested_ = org.fireRequested_;
   targetName_ = org.targetName_;
}

//------------------------------------------------------------------------------
// execute() -- a chamada ao xboard e' INCONDICIONAL (roda em todo tick,
// disparando ou nao -- ver CONTRATO.md secao 3 e o cabecalho deste
// arquivo). O disparo em si e' a MESMA sequencia de 3 chamadas ja provada
// em models/players/A-4/src/ubf/FlightAction.cpp:299-328:
// Player::getStoresManagement() -> StoresMgr::releaseOneMissile()
// (publico, PRE-REF'D, dynamic_cast<Missile*> por baixo -- casa QUALQUER
// subclasse de Missile) -> AbstractWeapon::setTargetPlayer(alvo,
// posTrkEnb=true) -> unref(). Nao ha nada de A-4-especifico ali: e' API
// generica de Player/StoresMgr/AbstractWeapon, e por isso a MESMA antena de
// codigo serve o lancador aereo e o terrestre.
//------------------------------------------------------------------------------
bool AaaAction::execute(base::Component* const actor)
{
   const auto site = dynamic_cast<models::Player*>(actor);
   if (site == nullptr) return false;

   xboard::setBehaviorLabel(site->getID(), label_);
   xboard::bumpDecisionCount(site->getID());

   if (!fireRequested_) return true;

   auto* const world = site->getWorldModel();
   const auto target = (world != nullptr)
      ? dynamic_cast<models::Player*>(world->findPlayerByName(targetName_.c_str()))
      : nullptr;

   auto* const storesMgr = site->getStoresManagement();

   if (target == nullptr) {
      LOG(WARNING) << "[AaaAction] disparo abortado -- alvo '" << targetName_
                   << "' nao encontrado";
   } else if (storesMgr == nullptr || storesMgr->available() == 0) {
      LOG(WARNING) << "[AaaAction] disparo abortado -- cabide vazio";
   } else {
      auto* const flyout = storesMgr->releaseOneMissile();
      if (flyout != nullptr) {
         flyout->setTargetPlayer(target, /*posTrkEnb=*/true);
         // LOG(INFO) << "[AaaAction] missil lancado contra " << targetName_;
         flyout->unref();   // releaseOneMissile() devolve pre-ref'd
      } else {
         LOG(WARNING) << "[AaaAction] disparo abortado -- releaseOneMissile() devolveu nulo";
      }
   }

   return true;
}

} // namespace xaaa
} // namespace models
} // namespace mixr
