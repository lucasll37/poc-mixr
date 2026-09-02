#include "app/ScenarioCatalog.hpp"

namespace app {

const std::vector<ScenarioEntry>& scenarioCatalog()
{
   static const std::vector<ScenarioEntry> catalog{
      {
         "patrol", "Patrulha",
         "4 falcons patrulhando, sem intruso -- bom para ver pausar/acelerar sem ruido",
         "./app/configs/scenario_patrol.epp.in",
      },
      {
         "intercept", "Intercepto",
         "+ bandit1 local -- mostra a evasao e o apoio entre os falcons (EVADE/SUPPORT)",
         "./app/configs/scenario_intercept.epp.in",
      },
      {
         "intercept_missile", "Intercepto + Missil",
         "+ falcon1 com um missil guiado -- lancamento/detonacao, otimo pra pausar no meio",
         "./app/configs/scenario_intercept_missile.epp.in",
      },
   };
   return catalog;
}

const ScenarioEntry* findScenario(const std::string& key)
{
   for (const auto& entry : scenarioCatalog()) {
      if (entry.key == key) return &entry;
   }
   return nullptr;
}

} // namespace app
