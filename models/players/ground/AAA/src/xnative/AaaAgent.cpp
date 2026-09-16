#include "xnative/AaaAgent.hpp"

namespace mixr {
namespace models {
namespace xAAA {

IMPLEMENT_SUBCLASS(AaaAgent, "AaaAgent")
EMPTY_SLOTTABLE(AaaAgent)
EMPTY_DELETEDATA(AaaAgent)

AaaAgent::AaaAgent()
{
   STANDARD_CONSTRUCTOR()
}

void AaaAgent::copyData(const AaaAgent& org, const bool)
{
   BaseClass::copyData(org);
}

//------------------------------------------------------------------------------
// shutdownNotification() -- quebra o ciclo de referencia com o proprio
// player antes de delegar para a base (ver a armadilha do .hpp).
//
// Agent::myActor e' um safe_ptr (ref-owning) apontando de volta para o
// player que contem este agente (initActor() nativo sobe container()). O
// player possui o agente via components:, e o agente possui uma
// referencia de volta ao player via myActor -- um ciclo que nenhum
// unref() externo desfaz sozinho. setActor(nullptr) solta essa
// referencia; a chamada a BaseClass::shutdownNotification() continua
// propagando o evento para os subcomponentes do proprio agente (ex.:
// 'state', adicionado via addComponent() em Agent::setState()).
//------------------------------------------------------------------------------
bool AaaAgent::shutdownNotification()
{
   setActor(nullptr);
   return BaseClass::shutdownNotification();
}

} // namespace xAAA
} // namespace models
} // namespace mixr
