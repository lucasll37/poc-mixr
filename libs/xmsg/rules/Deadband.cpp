#include "xmsg/rules/Deadband.hpp"

#include <cmath>

namespace mixr {
namespace xmsg {
namespace rules {

void Deadband::configure(const double by)
{
   by_ = (by > 0.0) ? by : 0.0;
   reset();
}

void Deadband::reset()
{
   has_ = false;
   lastEmitted_ = 0.0;
}

bool Deadband::update(const double value, const bool valid)
{
   // Campo indisponivel congela a referencia: quando o valor voltar, a
   // comparacao e contra a ultima coisa que se AVISOU, nao contra um buraco.
   if (!valid) return false;

   if (!has_) {
      has_ = true;
      lastEmitted_ = value;
      return true;
   }

   const double delta{std::fabs(value - lastEmitted_)};
   const bool emit{(by_ > 0.0) ? (delta >= by_) : (delta > 0.0)};

   if (emit) lastEmitted_ = value;
   return emit;
}

} // namespace rules
} // namespace xmsg
} // namespace mixr
