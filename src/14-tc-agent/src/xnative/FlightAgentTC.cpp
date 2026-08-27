#include "xnative/FlightAgentTC.hpp"

#include "xnative/ThreadTag.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

#include "mixr/base/Pair.hpp"

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(FlightAgentTC, "FlightAgentTC")
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
   lastThreadTag.store(-1);
}

//------------------------------------------------------------------------------
// initActor() -- o ator e o player que CONTEM este agente.
//
// O SimAgent nativo (poc/13) faz o contrario: mora na Station e resolve o
// ator por NOME, procurando na lista de players do WorldModel. Aqui basta
// subir a cadeia de containers -- e o bloco EDL fica identico para as
// quatro aeronaves.
//------------------------------------------------------------------------------
void FlightAgentTC::initActor()
{
   if (getActor() != nullptr) return;

   const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
   if (player != nullptr) setActor(player);
}

//------------------------------------------------------------------------------
// updateData() -- NO-OP de proposito (armadilha 3 do .hpp).
//
// ubf::Agent::updateData() chama controller(dt), e Player::updateData()
// propaga para a lista de componentes: sem esta sobrescrita, o agente
// decidiria tambem na thread de background, alem da fase 3. O filtro de
// fase de controller() nao evita isso -- ao fim do tcFrame a fase corrente
// permanece em 3, entao a chamada de background passaria pelo filtro.
//
// Nao ha nada a propagar aqui: o proprio Agent nativo NAO repassa
// updateData() ao 'state'/'behavior' (eles nao sao atualizados pelo ciclo
// de componentes), entao o no-op nao suprime nenhum trabalho existente.
//------------------------------------------------------------------------------
void FlightAgentTC::updateData(const double)
{
}

//------------------------------------------------------------------------------
// controller() -- ciclo do UBF, so na FASE 3 e com o dt do frame inteiro.
//
// Chamado por AgentTC::updateTC(), que por sua vez e chamado pelo ciclo de
// componentes do player -- ou seja, DENTRO da thread de tempo critico que
// estiver processando esta aeronave (com numTcThreads > 1, aeronaves
// diferentes decidem em threads diferentes, em paralelo).
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

   lastThreadTag.store(threadTag(), std::memory_order_relaxed);

   BaseClass::controller(dt * 4.0);

   decisions.fetch_add(1, std::memory_order_relaxed);
}

const FlightAgentTC* findFlightAgent(const models::AirVehicle* const air)
{
   if (air == nullptr) return nullptr;

   const base::Pair* const pair{air->findByType(typeid(FlightAgentTC))};
   if (pair == nullptr) return nullptr;
   return dynamic_cast<const FlightAgentTC*>(pair->object());
}

} // namespace xnative
} // namespace mixr
