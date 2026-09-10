#include "xnative/ParatrooperPlaceholder.hpp"

#include "mixr/base/String.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

IMPLEMENT_SUBCLASS(ParatrooperPlaceholder, "C130ParatrooperPlaceholder")
EMPTY_SLOTTABLE(ParatrooperPlaceholder)
EMPTY_COPYDATA(ParatrooperPlaceholder)
EMPTY_DELETEDATA(ParatrooperPlaceholder)

const char* ParatrooperPlaceholder::getDescription() const   { return "Paratrooper (placeholder)"; }
const char* ParatrooperPlaceholder::getNickname() const      { return "Paratrooper"; }

// Tipo default "PARATROOPER" -- bate com o default de
// ActionParatrooperRelease::storeType_ (xnative/ActionParatrooperRelease.hpp)
// mesmo se o EDL nao declarar 'type:' na estacao.
ParatrooperPlaceholder::ParatrooperPlaceholder()
{
   STANDARD_CONSTRUCTOR()

   static base::String generic("PARATROOPER");
   setType(&generic);
}

} // namespace xC_130
} // namespace models
} // namespace mixr
