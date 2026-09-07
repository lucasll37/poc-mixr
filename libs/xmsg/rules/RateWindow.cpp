#include "xmsg/rules/RateWindow.hpp"

namespace mixr {
namespace xmsg {
namespace rules {

void RateWindow::configure(const double windowSeconds)
{
   window_ = (windowSeconds > 0.0) ? windowSeconds : 1.0;
   reset();
}

void RateWindow::reset()
{
   clock_ = 0.0;
   head_ = 0;
   count_ = 0;
}

void RateWindow::push(const double dt, const double value, const bool valid)
{
   clock_ += dt;
   if (!valid) return;

   buf_[head_] = Sample{clock_, value};
   head_ = (head_ + 1) % CAPACITY;
   if (count_ < CAPACITY) ++count_;
}

//------------------------------------------------------------------------------
// oldestInWindow() -- caminha do mais novo para tras ate sair de 'window_';
// devolve 'novo' (dt=0 contra si mesma) se nem a amostra imediatamente
// anterior couber na janela. Unica fonte de verdade sobre "o que esta dentro
// da janela" -- ready() e rate() tem de concordar, ou um credencia uma
// derivada que o outro nao consegue calcular (ver o "porque" no header).
//------------------------------------------------------------------------------
std::size_t RateWindow::oldestInWindow(const std::size_t novo) const
{
   const double tNovo{buf_[novo].t};

   std::size_t escolhido{novo};
   for (std::size_t k = 1; k < count_; ++k) {
      const std::size_t idx{(novo + CAPACITY - k) % CAPACITY};
      if (tNovo - buf_[idx].t > window_) break;   // saiu da janela
      escolhido = idx;
   }
   return escolhido;
}

bool RateWindow::ready() const
{
   if (count_ < 2) return false;

   const std::size_t novo{(head_ + CAPACITY - 1) % CAPACITY};
   const std::size_t escolhido{oldestInWindow(novo)};
   return buf_[novo].t - buf_[escolhido].t > 0.0;
}

//------------------------------------------------------------------------------
// rate() -- inclinacao da corda entre a amostra mais VELHA ainda dentro da
// janela e a mais nova.
//------------------------------------------------------------------------------
double RateWindow::rate() const
{
   if (count_ < 2) return 0.0;

   const std::size_t novo{(head_ + CAPACITY - 1) % CAPACITY};
   const std::size_t escolhido{oldestInWindow(novo)};

   const double dt{buf_[novo].t - buf_[escolhido].t};
   if (dt <= 0.0) return 0.0;

   return (buf_[novo].v - buf_[escolhido].v) / dt;
}

} // namespace rules
} // namespace xmsg
} // namespace mixr
