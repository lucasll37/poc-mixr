#include "xmsg/SlotUnits.hpp"

#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/Times.hpp"

namespace mixr {
namespace xmsg {

bool SlotValue::capture(const base::Number* const n)
{
   if (n == nullptr) return false;

   if (const auto d = dynamic_cast<const base::Distance*>(n)) {
      kind = Dim::Distance;
      value = base::Meters::convertStatic(*d);
   } else if (const auto a = dynamic_cast<const base::Angle*>(n)) {
      kind = Dim::Angle;
      value = base::Degrees::convertStatic(*a);
   } else if (const auto t = dynamic_cast<const base::Time*>(n)) {
      kind = Dim::Time;
      value = base::Seconds::convertStatic(*t);
   } else {
      kind = Dim::None;
      value = n->getDouble();
   }

   set = true;
   return true;
}

bool SlotValue::resolve(const Dim fieldDim, double& out) const
{
   if (!set) return false;

   // Numero cru vale para qualquer campo: quem escreveu assume a unidade que
   // esta no nome do campo.
   if (kind == Dim::None) { out = value; return true; }

   // Objeto de unidade tem de casar com a dimensao do campo. 'Speed' nao tem
   // classe de unidade propria aqui, entao so aceita numero cru.
   if (kind != fieldDim) return false;

   out = value;
   return true;
}

bool captureSeconds(const base::Number* const n, double& out)
{
   if (n == nullptr) return false;

   if (const auto t = dynamic_cast<const base::Time*>(n)) {
      out = base::Seconds::convertStatic(*t);
   } else {
      out = n->getDouble();
   }
   return out >= 0.0;
}

} // namespace xmsg
} // namespace mixr
