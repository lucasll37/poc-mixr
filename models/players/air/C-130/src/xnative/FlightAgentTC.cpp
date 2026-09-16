#include "xnative/FlightAgentTC.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

IMPLEMENT_SUBCLASS(FlightAgentTC, "C130FlightAgentTC")
EMPTY_SLOTTABLE(FlightAgentTC)
EMPTY_DELETEDATA(FlightAgentTC)

FlightAgentTC::FlightAgentTC()
{
   STANDARD_CONSTRUCTOR()
}

void FlightAgentTC::copyData(const FlightAgentTC& org, const bool)
{
   BaseClass::copyData(org);
   decisions.store(0);
}

//------------------------------------------------------------------------------
// initActor() -- o ator e o player que CONTEM este agente.
//------------------------------------------------------------------------------
void FlightAgentTC::initActor()
{
   if (getActor() != nullptr) return;

   const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
   if (player != nullptr) setActor(player);
}

//------------------------------------------------------------------------------
// updateData() -- NO-OP de proposito (armadilha 3 do .hpp).
//------------------------------------------------------------------------------
void FlightAgentTC::updateData(const double)
{
}

//------------------------------------------------------------------------------
// controller() -- ciclo do UBF, so na FASE 3 e com o dt do frame inteiro.
//------------------------------------------------------------------------------
void FlightAgentTC::controller(const double dt)
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

} // namespace xC_130
} // namespace models
} // namespace mixr
