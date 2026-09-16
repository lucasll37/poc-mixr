#include "xnative/ActionParatrooperRelease.hpp"

#include "xnative/ReleaseHelpers.hpp"

#include "mixr/base/String.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

IMPLEMENT_SUBCLASS(ActionParatrooperRelease, "C130ActionParatrooperRelease")
EMPTY_DELETEDATA(ActionParatrooperRelease)

// clang-format off
BEGIN_SLOTTABLE(ActionParatrooperRelease)
   "storeType",   // 1
END_SLOTTABLE(ActionParatrooperRelease)

BEGIN_SLOT_MAP(ActionParatrooperRelease)
   ON_SLOT(1, setSlotStoreType, base::String)
END_SLOT_MAP()

ActionParatrooperRelease::ActionParatrooperRelease()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void ActionParatrooperRelease::copyData(const ActionParatrooperRelease& org, const bool)
{
   BaseClass::copyData(org);
   storeType_ = org.storeType_;
}

//------------------------------------------------------------------------------
// trigger() -- acha a proxima estacao livre cujo Player::getType() bata com
// 'storeType_' e libera, UMA vez so' (sempre completo ao voltar -- ao
// contrario de ActionParatrooperStick, que fica em progresso ate liberar
// 'count' estacoes). A busca em si mora em xnative::releaseNextStoreOfType(),
// compartilhada com ActionParatrooperStick.
//------------------------------------------------------------------------------
bool ActionParatrooperRelease::trigger(OnboardComputer* const mgr)
{
   const bool ok{releaseNextStoreOfType(mgr, storeType_)};
   BaseClass::setCompleted(true);
   return ok;
}

bool ActionParatrooperRelease::setSlotStoreType(const base::String* const msg)
{
   if (msg == nullptr) return false;
   storeType_ = *msg;
   return true;
}

} // namespace xC_130
} // namespace models
} // namespace mixr
