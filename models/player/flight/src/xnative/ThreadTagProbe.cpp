#include "xnative/ThreadTagProbe.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(ThreadTagProbe, "ThreadTagProbe")
EMPTY_SLOTTABLE(ThreadTagProbe)
EMPTY_DELETEDATA(ThreadTagProbe)
EMPTY_COPYDATA(ThreadTagProbe)

ThreadTagProbe::ThreadTagProbe()
{
   STANDARD_CONSTRUCTOR()
}

//------------------------------------------------------------------------------
// updateTC() -- so na fase 3, mesmo filtro de FlightAgentTC::controller() (ver
// o cabecalho do .hpp): Player::updateTC() chama esta funcao 4x por frame,
// uma vez por fase; sem o filtro o mesmo valor seria escrito 4x a toa.
//------------------------------------------------------------------------------
void ThreadTagProbe::updateTC(const double dt)
{
   BaseClass::updateTC(dt);
   if (dt <= 0.0) return;

   const auto player = static_cast<const models::Player*>(findContainerByType(typeid(models::Player)));
   if (player == nullptr) return;

   const models::WorldModel* const world{player->getWorldModel()};
   if (world == nullptr || world->phase() != 3) return;

   xboard::setThreadTag(player->getID(), xboard::threadTag());
}

} // namespace xnative
} // namespace mixr
