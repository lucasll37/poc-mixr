#include "app/ScenarioCatalog.hpp"

namespace app {

namespace {

// Os falcon1..4 sao IGUAIS nos tres cenarios (mesmo modelo/tipo/cor no
// Tacview) -- so 'intercept'/'intercept_missile' acrescentam bandit1.
const char* const kFalconsModelMap{
   "falcon1: \"F-16C\"  falcon2: \"F-16C\"  falcon3: \"F-16C\"  falcon4: \"F-16C\""};
const char* const kFalconsTypeMap{
   "falcon1: \"Air+FixedWing\"  falcon2: \"Air+FixedWing\"\n"
   "                           falcon3: \"Air+FixedWing\"  falcon4: \"Air+FixedWing\""};
const char* const kFalconsColorMap{
   "falcon1: \"Blue\"  falcon2: \"Blue\"  falcon3: \"Blue\"  falcon4: \"Blue\""};

std::string withBandit1(const std::string& falcons, const std::string& bandit1Suffix)
{
   return falcons + bandit1Suffix;
}

} // namespace

const std::vector<ScenarioEntry>& scenarioCatalog()
{
   static const std::vector<ScenarioEntry> catalog{
      {
         "patrol", "Patrulha",
         "4 falcons patrulhando, sem intruso -- bom para ver pausar/acelerar sem ruido",
         "./app/configs/scenario_patrol.epp.in",
         "patrol", kFalconsModelMap, kFalconsTypeMap, kFalconsColorMap,
      },
      {
         "intercept", "Intercepto",
         "+ bandit1 local -- mostra a evasao e o apoio entre os falcons (EVADE/SUPPORT)",
         "./app/configs/scenario_intercept.epp.in",
         "intercept",
         withBandit1(kFalconsModelMap, "\n                           bandit1: \"F-16C\""),
         withBandit1(kFalconsTypeMap, "  bandit1: \"Air+FixedWing\""),
         withBandit1(kFalconsColorMap, "  bandit1: \"Red\""),
      },
      {
         "intercept_missile", "Intercepto + Missil",
         "+ falcon1 com um missil guiado -- lancamento/detonacao, otimo pra pausar no meio",
         "./app/configs/scenario_intercept_missile.epp.in",
         "intercept-missile",   // note o hifen -- ja era assim no eventName/fileName de producao
         withBandit1(kFalconsModelMap, "\n                           bandit1: \"F-16C\""),
         withBandit1(kFalconsTypeMap, "  bandit1: \"Air+FixedWing\""),
         withBandit1(kFalconsColorMap, "  bandit1: \"Red\""),
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
