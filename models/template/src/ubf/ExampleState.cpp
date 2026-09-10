#include "ubf/ExampleState.hpp"

#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace models {
namespace xtemplate {

IMPLEMENT_SUBCLASS(ExampleState, "ExampleState")
EMPTY_SLOTTABLE(ExampleState)
EMPTY_DELETEDATA(ExampleState)

ExampleState::ExampleState()
{
   STANDARD_CONSTRUCTOR()
}

// Copiada campo a campo, nao EMPTY_COPYDATA -- 'value' e estado de verdade
// (a ultima leitura), nao so cache descartavel; um clone() deste objeto
// (ex.: ao clonar o Player dono dele) tem de preservar o que ja foi
// percebido. Mesmo raciocinio de models/players/A-4/src/ubf/FlightState.cpp.
void ExampleState::copyData(const ExampleState& org, const bool)
{
   BaseClass::copyData(org);
   value = org.value;
}

void ExampleState::updateState(const base::Component* const actor)
{
   const auto player = dynamic_cast<const models::Player*>(actor);
   if (player == nullptr) return;

   value = player->getAltitudeM();
}

} // namespace xtemplate
} // namespace models
} // namespace mixr
