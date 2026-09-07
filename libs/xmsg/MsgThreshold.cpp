#include "xmsg/MsgThreshold.hpp"

#include "mixr/base/numeric/Number.hpp"

#include "xlog/Log.hpp"

namespace mixr {
namespace xmsg {

IMPLEMENT_SUBCLASS(MsgThreshold, "MsgThreshold")

// clang-format off
BEGIN_SLOTTABLE(MsgThreshold)
   "above", "below", "clear", "hold",
END_SLOTTABLE(MsgThreshold)

BEGIN_SLOT_MAP(MsgThreshold)
   ON_SLOT(1, setSlotAbove, base::Number)
   ON_SLOT(2, setSlotBelow, base::Number)
   ON_SLOT(3, setSlotClear, base::Number)
   ON_SLOT(4, setSlotHold,  base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(MsgThreshold)

MsgThreshold::MsgThreshold()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void MsgThreshold::copyData(const MsgThreshold& org, const bool)
{
   BaseClass::copyData(org);
   above_ = org.above_;
   below_ = org.below_;
   clear_ = org.clear_;
   hold_ = org.hold_;
   state_.clear();
}

bool MsgThreshold::setSlotAbove(const base::Number* const x) { return above_.capture(x); }
bool MsgThreshold::setSlotBelow(const base::Number* const x) { return below_.capture(x); }
bool MsgThreshold::setSlotClear(const base::Number* const x) { return clear_.capture(x); }

bool MsgThreshold::setSlotHold(const base::Number* const x)
{
   return captureSeconds(x, hold_);
}

bool MsgThreshold::prepare(const int maxPlayers)
{
   if (!BaseClass::prepare(maxPlayers)) return false;

   if (above_.set == below_.set) {
      LOG(ERROR) << "[xmsg] MsgThreshold em '" << fieldName()
                 << "': declare exatamente um entre 'above:' e 'below:'";
      return false;
   }
   if (!clear_.set) {
      LOG(ERROR) << "[xmsg] MsgThreshold em '" << fieldName()
                 << "': 'clear:' e obrigatorio -- sem histerese o evento repete"
                    " a cada ciclo em cima do limiar";
      return false;
   }

   const SlotValue& trip{above_.set ? above_ : below_};
   double tripV{}, clearV{};
   if (!trip.resolve(dim(), tripV) || !clear_.resolve(dim(), clearV)) {
      LOG(ERROR) << "[xmsg] MsgThreshold em '" << fieldName()
                 << "': unidade incompativel com a dimensao do campo";
      return false;
   }

   rules::Schmitt modelo;
   const auto sense{above_.set ? rules::Schmitt::Sense::Above
                               : rules::Schmitt::Sense::Below};
   if (!modelo.configure(sense, tripV, clearV, hold_)) {
      LOG(ERROR) << "[xmsg] MsgThreshold em '" << fieldName()
                 << "': 'clear:' esta do lado errado de "
                 << (above_.set ? "'above:'" : "'below:'")
                 << " -- com 'above' o clear tem de ser MENOR, com 'below' MAIOR";
      return false;
   }

   state_.assign(static_cast<std::size_t>(slots()), modelo);
   return true;
}

bool MsgThreshold::evaluate(const double dt, const Snapshot& snap, const int slot)
{
   if (slot < 0 || slot >= static_cast<int>(state_.size())) return false;

   bool ok{};
   const double x{read(snap, ok)};
   return state_[static_cast<std::size_t>(slot)].update(dt, x, ok);
}

void MsgThreshold::resetSlot(const int slot)
{
   if (slot >= 0 && slot < static_cast<int>(state_.size())) {
      state_[static_cast<std::size_t>(slot)].reset();
   }
}

} // namespace xmsg
} // namespace mixr
