#include "ubf/ExampleBehavior.hpp"

#include "ubf/ExampleAction.hpp"
#include "ubf/ExampleState.hpp"

#include "mixr/base/units/Distances.hpp"

namespace mixr {
namespace models {
namespace xtemplate {

IMPLEMENT_SUBCLASS(ExampleBehavior, "ExampleBehavior")

// clang-format off
BEGIN_SLOTTABLE(ExampleBehavior)
   "onValue",    // 1
   "offValue",   // 2
END_SLOTTABLE(ExampleBehavior)

BEGIN_SLOT_MAP(ExampleBehavior)
   ON_SLOT(1, setSlotOnValue,  base::Distance)
   ON_SLOT(2, setSlotOffValue, base::Distance)
END_SLOT_MAP()

EMPTY_DELETEDATA(ExampleBehavior)

ExampleBehavior::ExampleBehavior()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void ExampleBehavior::copyData(const ExampleBehavior& org, const bool)
{
   BaseClass::copyData(org);
   rule = org.rule;
   engaged = org.engaged;
}

//------------------------------------------------------------------------------
// genAction() -- chamado uma vez por ciclo de decisao pelo Agent do UBF.
// Devolver nullptr e legitimo no UBF (ver
// models/players/A-4/src/ubf/AltitudeSafetyBehavior.cpp para um exemplo real de
// "as vezes nao ha o que recomendar"), mas este exemplo sempre tem uma
// acao: engajado ou nao ja cobre todo o espaco de decisao dele.
//------------------------------------------------------------------------------
base::ubf::AbstractAction* ExampleBehavior::genAction(const base::ubf::AbstractState* const state,
                                                       const double)
{
   const auto s = dynamic_cast<const ExampleState*>(state);
   if (s == nullptr) return nullptr;

   engaged = rule.next(s->getValue(), engaged);

   const auto action = new ExampleAction(engaged ? "ENGAGED" : "IDLE");   // pre-ref'd: o Agent da unref()
   action->setVote(getVote());
   return action;
}

bool ExampleBehavior::setSlotOnValue(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   rule.onValue = base::Meters::convertStatic(*msg);
   return true;
}

bool ExampleBehavior::setSlotOffValue(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   rule.offValue = base::Meters::convertStatic(*msg);
   return true;
}

} // namespace xtemplate
} // namespace models
} // namespace mixr
