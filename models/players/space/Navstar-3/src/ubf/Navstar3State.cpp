#include "ubf/Navstar3State.hpp"

#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace models {
namespace xNavstar_3 {

IMPLEMENT_SUBCLASS(Navstar3State, "Navstar3State")
EMPTY_SLOTTABLE(Navstar3State)
EMPTY_DELETEDATA(Navstar3State)

Navstar3State::Navstar3State()
{
   STANDARD_CONSTRUCTOR()
}

// Copiada campo a campo, nao EMPTY_COPYDATA -- a leitura percebida e estado
// de verdade, nao so cache descartavel.
void Navstar3State::copyData(const Navstar3State& org, const bool)
{
   BaseClass::copyData(org);
   valid = org.valid;
   altitudeM = org.altitudeM;
}

void Navstar3State::updateState(const base::Component* const actor)
{
   const auto player = dynamic_cast<const models::Player*>(actor);
   if (player == nullptr) { valid = false; return; }

   altitudeM = player->getAltitudeM();
   valid = true;
}

} // namespace xNavstar_3
} // namespace models
} // namespace mixr
