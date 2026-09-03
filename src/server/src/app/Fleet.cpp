#include "app/Fleet.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

#include "mixr/base/List.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"

namespace app {

std::vector<mixr::models::Player*> discoverPlayers(mixr::models::WorldModel* const wm)
{
   std::vector<mixr::models::Player*> out;
   if (wm == nullptr) return out;

   mixr::base::PairStream* const players{wm->getPlayers()};
   if (players == nullptr) return out;

   mixr::base::List::Item* item{players->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<mixr::base::Pair*>(item->getValue());
      if (pair != nullptr) {
         const auto player = dynamic_cast<mixr::models::Player*>(pair->object());
         if (player != nullptr) out.push_back(player);
      }
      item = item->getNext();
   }

   players->unref();
   return out;
}

} // namespace app
