#include "xmissile/MissileThreadTagProbe.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace xmissile {

IMPLEMENT_SUBCLASS(MissileThreadTagProbe, "MissileThreadTagProbe")
EMPTY_SLOTTABLE(MissileThreadTagProbe)
EMPTY_DELETEDATA(MissileThreadTagProbe)
EMPTY_COPYDATA(MissileThreadTagProbe)

MissileThreadTagProbe::MissileThreadTagProbe()
{
   STANDARD_CONSTRUCTOR()
}

//------------------------------------------------------------------------------
// updateTC() -- so na fase 3, mesmo filtro do gemeo em models/player/A4
// (ver o cabecalho do .hpp): Player::updateTC() chama esta funcao 4x por
// frame, uma vez por fase; sem o filtro o mesmo valor seria escrito 4x a
// toa. Vale igual para o missil: AbstractWeapon::updateTC() (BaseClass de
// GuidedMissile) chama Player::updateTC(), que tem o mesmo portao de
// mode==ACTIVE||PRE_RELEASE e a mesma recursao generica por fase.
//------------------------------------------------------------------------------
void MissileThreadTagProbe::updateTC(const double dt)
{
   BaseClass::updateTC(dt);
   if (dt <= 0.0) return;

   const auto player = static_cast<const models::Player*>(findContainerByType(typeid(models::Player)));
   if (player == nullptr) return;

   const models::WorldModel* const world{player->getWorldModel()};
   if (world == nullptr || world->phase() != 3) return;

   xboard::setThreadTag(player->getID(), xboard::threadTag());
}

} // namespace xmissile
} // namespace mixr
