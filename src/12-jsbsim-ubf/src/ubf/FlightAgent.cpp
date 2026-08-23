#include "ubf/FlightAgent.hpp"

#include "xair/runtime_utils.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace xair {

IMPLEMENT_SUBCLASS(FlightAgent, "FlightAgent")
EMPTY_SLOTTABLE(FlightAgent)
EMPTY_DELETEDATA(FlightAgent)

FlightAgent::FlightAgent()
{
   STANDARD_CONSTRUCTOR()
}

void FlightAgent::copyData(const FlightAgent& org, const bool)
{
   BaseClass::copyData(org);
   decisions.store(0);
   lastThreadTag.store(-1);
}

//------------------------------------------------------------------------------
// initActor() -- o ator e o player que CONTEM este agente.
//
// O SimAgent nativo amarra o ator por nome, a partir da Station; aqui o
// agente mora dentro da aeronave, entao basta subir a cadeia de containers.
//------------------------------------------------------------------------------
void FlightAgent::initActor()
{
   if (getActor() != nullptr) return;

   const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
   if (player != nullptr) setActor(player);
}

//------------------------------------------------------------------------------
// controller() -- ciclo do UBF, so na FASE 3 e com o dt do frame inteiro.
//------------------------------------------------------------------------------
void FlightAgent::controller(const double dt)
{
   if (dt <= 0.0) return;

   if (getActor() == nullptr) initActor();

   const auto player = dynamic_cast<models::Player*>(getActor());
   if (player == nullptr) return;

   const models::WorldModel* const world{player->getWorldModel()};
   if (world == nullptr) return;

   // AgentTC chama controller() uma vez por FASE (4x por frame, com dt/4).
   // A decisao pertence a fase 3, com o dt do frame inteiro.
   if (world->phase() != 3) return;

   lastThreadTag.store(threadTag(), std::memory_order_relaxed);

   BaseClass::controller(dt * 4.0);

   decisions.fetch_add(1, std::memory_order_relaxed);
}

} // namespace xair
} // namespace mixr
