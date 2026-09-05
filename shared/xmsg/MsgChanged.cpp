#include "xmsg/MsgChanged.hpp"

#include "mixr/base/numeric/Number.hpp"

#include "xlog/Log.hpp"

namespace mixr {
namespace xmsg {

IMPLEMENT_SUBCLASS(MsgChanged, "MsgChanged")

// clang-format off
BEGIN_SLOTTABLE(MsgChanged)
   "by",
END_SLOTTABLE(MsgChanged)

BEGIN_SLOT_MAP(MsgChanged)
   ON_SLOT(1, setSlotBy, base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(MsgChanged)

MsgChanged::MsgChanged()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void MsgChanged::copyData(const MsgChanged& org, const bool)
{
   BaseClass::copyData(org);
   by_ = org.by_;
   state_.clear();
}

bool MsgChanged::setSlotBy(const base::Number* const x) { return by_.capture(x); }

bool MsgChanged::prepare(const int maxPlayers)
{
   if (!BaseClass::prepare(maxPlayers)) return false;

   double byV{};
   if (by_.set && !by_.resolve(dim(), byV)) {
      LOG(ERROR) << "[xmsg] MsgChanged em '" << fieldName()
                 << "': unidade de 'by:' incompativel com a dimensao do campo";
      return false;
   }

   rules::Deadband modelo;
   modelo.configure(byV);
   state_.assign(static_cast<std::size_t>(slots()), modelo);
   return true;
}

bool MsgChanged::evaluate(const double, const Snapshot& snap, const int slot)
{
   if (slot < 0 || slot >= static_cast<int>(state_.size())) return false;

   bool ok{};
   const double x{read(snap, ok)};
   return state_[static_cast<std::size_t>(slot)].update(x, ok);
}

void MsgChanged::resetSlot(const int slot)
{
   if (slot >= 0 && slot < static_cast<int>(state_.size())) {
      state_[static_cast<std::size_t>(slot)].reset();
   }
}

} // namespace xmsg
} // namespace mixr
