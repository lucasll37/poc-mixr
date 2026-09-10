#include "xnative/ReleaseHelpers.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/weapon/AbstractWeapon.hpp"
#include "mixr/models/system/OnboardComputer.hpp"
#include "mixr/models/system/StoresMgr.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

bool releaseNextStoreOfType(models::OnboardComputer* const mgr, const base::String& storeType)
{
   bool ok{};

   if (mgr != nullptr) {
      const auto own = static_cast<Player*>(mgr->findContainerByType(typeid(Player)));
      if (own != nullptr) {
         StoresMgr* const sms{own->getStoresManagement()};
         if (sms != nullptr) {

            AbstractWeapon* target{};

            base::PairStream* const list{sms->getWeapons()};
            if (list != nullptr) {
               base::List::Item* item{list->getFirstItem()};
               while (item != nullptr && target == nullptr) {
                  const auto pair = static_cast<base::Pair*>(item->getValue());
                  const auto wpn = dynamic_cast<AbstractWeapon*>(pair->object());
                  if (wpn != nullptr && (wpn->isInactive() || wpn->isReleaseHold())) {
                     const base::String* const t{wpn->getType()};
                     if (t != nullptr && *t == storeType) {
                        target = wpn->getPointer();   // pre-ref'd
                     }
                  }
                  item = item->getNext();
               }
               list->unref();
            }

            if (target != nullptr) {
               AbstractWeapon* const flyout{sms->releaseWeapon(target)};
               if (flyout != nullptr) flyout->unref();
               target->unref();
               ok = true;
            }
         }
      }
   }

   return ok;
}

} // namespace xC_130
} // namespace models
} // namespace mixr
