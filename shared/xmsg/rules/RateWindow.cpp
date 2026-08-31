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

bool RateWindow::ready() const
{
   if (count_ < 2) return false;

   const std::size_t novo{(head_ + CAPACITY - 1) % CAPACITY};

   // Percorre do mais novo para tras ate sair da janela; basta existir um par
   // com separacao positiva.
   for (std::size_t k = 1; k < count_; ++k) {
      const std::size_t idx{(novo + CAPACITY - k) % CAPACITY};
      if (buf_[novo].t - buf_[idx].t > 0.0) return true;
   }
   return false;
}

//------------------------------------------------------------------------------
// rate() -- inclinacao da corda entre a amostra mais VELHA ainda dentro da
// janela e a mais nova.
//------------------------------------------------------------------------------
double RateWindow::rate() const
{
   if (count_ < 2) return 0.0;

   const std::size_t novo{(head_ + CAPACITY - 1) % CAPACITY};
   const double tNovo{buf_[novo].t};

   std::size_t escolhido{novo};
   for (std::size_t k = 1; k < count_; ++k) {
      const std::size_t idx{(novo + CAPACITY - k) % CAPACITY};
      if (tNovo - buf_[idx].t > window_) break;   // saiu da janela
      escolhido = idx;
   }

   const double dt{tNovo - buf_[escolhido].t};
   if (dt <= 0.0) return 0.0;

   return (buf_[novo].v - buf_[escolhido].v) / dt;
}

} // namespace rules
} // namespace xmsg
} // namespace mixr
