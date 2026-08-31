#include "xmsg/MsgRate.hpp"

#include "mixr/base/numeric/Number.hpp"

#include "xlog/Log.hpp"

namespace mixr {
namespace xmsg {

IMPLEMENT_SUBCLASS(MsgRate, "MsgRate")

BEGIN_SLOTTABLE(MsgRate)
   "above", "below", "clear", "window", "hold",
END_SLOTTABLE(MsgRate)

BEGIN_SLOT_MAP(MsgRate)
   ON_SLOT(1, setSlotAbove,  base::Number)
   ON_SLOT(2, setSlotBelow,  base::Number)
   ON_SLOT(3, setSlotClear,  base::Number)
   ON_SLOT(4, setSlotWindow, base::Number)
   ON_SLOT(5, setSlotHold,   base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(MsgRate)

MsgRate::MsgRate()
{
   STANDARD_CONSTRUCTOR()
}

void MsgRate::copyData(const MsgRate& org, const bool)
{
   BaseClass::copyData(org);
   above_ = org.above_;
   below_ = org.below_;
   clear_ = org.clear_;
   window_ = org.window_;
   hold_ = org.hold_;
   win_.clear();
   state_.clear();
}

bool MsgRate::setSlotAbove(const base::Number* const x) { return above_.capture(x); }
bool MsgRate::setSlotBelow(const base::Number* const x) { return below_.capture(x); }
bool MsgRate::setSlotClear(const base::Number* const x) { return clear_.capture(x); }
bool MsgRate::setSlotWindow(const base::Number* const x) { return captureSeconds(x, window_); }
bool MsgRate::setSlotHold(const base::Number* const x)   { return captureSeconds(x, hold_); }

bool MsgRate::prepare(const int maxPlayers)
{
   if (!BaseClass::prepare(maxPlayers)) return false;

   if (above_.set == below_.set) {
      LOG(ERROR) << "[xmsg] MsgRate em '" << fieldName()
                 << "': declare exatamente um entre 'above:' e 'below:'";
      return false;
   }

   const SlotValue& trip{above_.set ? above_ : below_};

   // Dim::None de proposito: o limiar e uma TAXA (unidade do campo por
   // segundo), e nao ha classe de unidade do MIXR que diga isso.
   double tripV{};
   if (!trip.resolve(Dim::None, tripV)) {
      LOG(ERROR) << "[xmsg] MsgRate em '" << fieldName()
                 << "': o limiar e uma TAXA (unidade do campo por segundo) --"
                    " use numero cru, nao objeto de unidade";
      return false;
   }

   double clearV{tripV};
   if (clear_.set && !clear_.resolve(Dim::None, clearV)) {
      LOG(ERROR) << "[xmsg] MsgRate em '" << fieldName()
                 << "': 'clear:' tambem e uma taxa -- use numero cru";
      return false;
   }

   rules::Schmitt modeloS;
   const auto sense{above_.set ? rules::Schmitt::Sense::Above
                               : rules::Schmitt::Sense::Below};
   if (!modeloS.configure(sense, tripV, clearV, hold_)) {
      LOG(ERROR) << "[xmsg] MsgRate em '" << fieldName()
                 << "': 'clear:' esta do lado errado do limiar";
      return false;
   }

   rules::RateWindow modeloW;
   modeloW.configure(window_);

   const auto n{static_cast<std::size_t>(slots())};
   win_.assign(n, modeloW);
   state_.assign(n, modeloS);
   return true;
}

bool MsgRate::evaluate(const double dt, const Snapshot& snap, const int slot)
{
   if (slot < 0 || slot >= static_cast<int>(state_.size())) return false;
   const auto i{static_cast<std::size_t>(slot)};

   bool ok{};
   const double x{read(snap, ok)};

   win_[i].push(dt, x, ok);

   // Enquanto nao ha dois pontos separados no tempo, nao ha derivada -- e
   // "sem derivada" e campo indisponivel, nao derivada zero. Tratar como zero
   // dispararia um 'below: -10' logo na largada, quando nada se sabe ainda.
   const bool pronto{ok && win_[i].ready()};
   return state_[i].update(dt, pronto ? win_[i].rate() : 0.0, pronto);
}

void MsgRate::resetSlot(const int slot)
{
   if (slot >= 0 && slot < static_cast<int>(state_.size())) {
      const auto i{static_cast<std::size_t>(slot)};
      win_[i].reset();
      state_[i].reset();
   }
}

} // namespace xmsg
} // namespace mixr
