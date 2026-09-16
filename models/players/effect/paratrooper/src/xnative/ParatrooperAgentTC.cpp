#include "xnative/ParatrooperAgentTC.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {

IMPLEMENT_SUBCLASS(ParatrooperAgentTC, "ParatrooperAgentTC")
EMPTY_SLOTTABLE(ParatrooperAgentTC)
EMPTY_DELETEDATA(ParatrooperAgentTC)

ParatrooperAgentTC::ParatrooperAgentTC()
{
   STANDARD_CONSTRUCTOR()
}

void ParatrooperAgentTC::copyData(const ParatrooperAgentTC& org, const bool)
{
   BaseClass::copyData(org);
   decisions.store(0);
}

//------------------------------------------------------------------------------
// initActor() -- o ator e o player que CONTEM este agente.
//------------------------------------------------------------------------------
void ParatrooperAgentTC::initActor()
{
   if (getActor() != nullptr) return;

   const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
   if (player != nullptr) setActor(player);
}

//------------------------------------------------------------------------------
// updateData() -- NO-OP de proposito (armadilha 3 do .hpp).
//------------------------------------------------------------------------------
void ParatrooperAgentTC::updateData(const double)
{
}

//------------------------------------------------------------------------------
// controller() -- ciclo do UBF, so na FASE 3 e com o dt do frame inteiro.
//------------------------------------------------------------------------------
void ParatrooperAgentTC::controller(const double dt)
{
   if (dt <= 0.0) return;

   if (getActor() == nullptr) initActor();

   const auto player = dynamic_cast<models::Player*>(getActor());
   if (player == nullptr) return;

   const models::WorldModel* const world{player->getWorldModel()};
   if (world == nullptr) return;

   // 4 passagens por frame, uma por fase, cada uma com dt/4. A decisao
   // pertence a fase 3 ("logica e controle"), com o dt do frame inteiro.
   if (world->phase() != 3) return;

   xboard::setThreadTag(player->getID(), xboard::threadTag());

   BaseClass::controller(dt * 4.0);

   decisions.fetch_add(1, std::memory_order_relaxed);
}

} // namespace xparatrooper
} // namespace models
} // namespace mixr
