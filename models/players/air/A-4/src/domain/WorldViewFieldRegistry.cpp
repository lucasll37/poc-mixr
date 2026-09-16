#include "domain/WorldViewFieldRegistry.hpp"

#include "xrlbridge/ObservationFields.hpp"

namespace mixr {
namespace models {
namespace xA_4 {
namespace domain {

const xrlbridge::FieldRegistry<WorldView>& worldViewFieldRegistry()
{
   static const xrlbridge::FieldRegistry<WorldView> registro = [] {
      xrlbridge::FieldRegistry<WorldView> r;
#define XRLBRIDGE_F(nome) r.add({#nome, xrlbridge::FieldKind::kFloat, \
      [](const WorldView& s) { return static_cast<double>(s.nome); }});
#define XRLBRIDGE_B(nome) r.add({#nome, xrlbridge::FieldKind::kBool, \
      [](const WorldView& s) { return s.nome ? 1.0 : 0.0; }});
      XRLBRIDGE_OBSERVATION_FIELDS
#undef XRLBRIDGE_F
#undef XRLBRIDGE_B
      return r;
   }();
   return registro;
}

} // namespace domain
} // namespace xA_4
} // namespace models
} // namespace mixr
