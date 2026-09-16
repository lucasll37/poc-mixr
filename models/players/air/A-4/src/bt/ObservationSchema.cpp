#include "bt/ObservationSchema.hpp"

#include "domain/WorldViewFieldRegistry.hpp"

#include "xrlbridge/RLBridge.hpp"

#include <sstream>

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

xrlbridge::Schema resolveObservationSchema(const std::string& value)
{
   if (value.empty() || value == "classic28") return xrlbridge::classicSchema28();

   if (value == "all") {
      xrlbridge::Schema s;
      s.name = "all";
      for (const auto& decl : domain::worldViewFieldRegistry().all()) s.fieldNames.push_back(decl.name);
      return s;
   }

   xrlbridge::Schema s;
   s.name = "ad-hoc: " + value;
   std::istringstream iss(value);
   std::string nome;
   while (iss >> nome) s.fieldNames.push_back(nome);
   return s;
}

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
