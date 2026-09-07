#include "xmsg/FieldCatalog.hpp"

#include <array>

namespace mixr {
namespace xmsg {

namespace {

// A ordem TEM de bater com o enum Field de Snapshot.hpp -- ha uma verificacao
// em tempo de compilacao logo abaixo, e uma por indice em findField().
const std::array<FieldInfo, F_COUNT> CATALOG{{
   // IDENT
   { "playerId",             Group::Ident,      F_playerId,             Dim::None,     "" },
   { "sideNum",              Group::Ident,      F_sideNum,              Dim::None,     "" },
   { "modeNum",              Group::Ident,      F_modeNum,              Dim::None,     "" },

   // KINEMATICS
   { "latDeg",               Group::Kinematics, F_latDeg,               Dim::Angle,    "deg" },
   { "lonDeg",               Group::Kinematics, F_lonDeg,               Dim::Angle,    "deg" },
   { "northM",               Group::Kinematics, F_northM,               Dim::Distance, "m" },
   { "eastM",                Group::Kinematics, F_eastM,                Dim::Distance, "m" },
   { "altMslM",              Group::Kinematics, F_altMslM,              Dim::Distance, "m" },
   { "altAglM",              Group::Kinematics, F_altAglM,              Dim::Distance, "m" },
   { "terrainElevM",         Group::Kinematics, F_terrainElevM,         Dim::Distance, "m" },
   { "hdgDeg",               Group::Kinematics, F_hdgDeg,               Dim::Angle,    "deg" },
   { "pitchDeg",             Group::Kinematics, F_pitchDeg,             Dim::Angle,    "deg" },
   { "rollDeg",              Group::Kinematics, F_rollDeg,              Dim::Angle,    "deg" },
   { "speedKts",             Group::Kinematics, F_speedKts,             Dim::Speed,    "kt" },
   { "climbMps",             Group::Kinematics, F_climbMps,             Dim::Speed,    "m/s" },

   // AIRDATA
   { "machNum",              Group::AirData,    F_machNum,              Dim::None,     "" },
   { "gLoad",                Group::AirData,    F_gLoad,                Dim::None,     "G" },
   { "aoaDeg",               Group::AirData,    F_aoaDeg,               Dim::Angle,    "deg" },

   // FUEL
   { "fuelLb",               Group::Fuel,       F_fuelLb,               Dim::None,     "lb" },
   { "fuelMaxLb",            Group::Fuel,       F_fuelMaxLb,            Dim::None,     "lb" },
   { "fuelFrac",            Group::Fuel,       F_fuelFrac,             Dim::None,     "0-1" },

   // ENGINE -- agregados.
   //
   // O sinal PRIMARIO de saude e o empuxo, nao o RPM: getEngThrust() devolve
   // lbf para todo tipo de motor, enquanto getEngRPM() e POLIMORFICO NA
   // UNIDADE (pistao/eletrico devolvem RPM absoluto, turbina devolve %N2,
   // turboprop devolve %N1). O c310 destas pocs e pistao, maxrpm 2625: um
   // campo chamado 'engRpmPct' mentiria por um fator de ~26. Por isso o nome
   // e engRpmRaw, e por isso o gatilho recomendado e engThrustAsymFrac.
   { "engCount",             Group::Engine,     F_engCount,             Dim::None,     "" },
   { "engThrustTotalLb",     Group::Engine,     F_engThrustTotalLb,     Dim::None,     "lbf" },
   { "engThrustMinLb",       Group::Engine,     F_engThrustMinLb,       Dim::None,     "lbf" },
   { "engThrustAsymFrac",    Group::Engine,     F_engThrustAsymFrac,    Dim::None,     "0-1" },
   { "engRpmMinRaw",         Group::Engine,     F_engRpmMinRaw,         Dim::None,     "rpm|%" },
   { "engFuelFlowTotalPph",  Group::Engine,     F_engFuelFlowTotalPph,  Dim::None,     "lb/h" },
   { "engOilPsiMin",         Group::Engine,     F_engOilPsiMin,         Dim::None,     "psi" },
   { "engPlaMinPct",         Group::Engine,     F_engPlaMinPct,         Dim::None,     "%" },
   { "engWorstIndex",        Group::Engine,     F_engWorstIndex,        Dim::None,     "1-based" },

   { "eng1ThrustLb",         Group::Engine,     F_eng1ThrustLb,         Dim::None,     "lbf" },
   { "eng2ThrustLb",         Group::Engine,     F_eng2ThrustLb,         Dim::None,     "lbf" },
   { "eng3ThrustLb",         Group::Engine,     F_eng3ThrustLb,         Dim::None,     "lbf" },
   { "eng4ThrustLb",         Group::Engine,     F_eng4ThrustLb,         Dim::None,     "lbf" },
   { "eng1RpmRaw",           Group::Engine,     F_eng1RpmRaw,           Dim::None,     "rpm|%" },
   { "eng2RpmRaw",           Group::Engine,     F_eng2RpmRaw,           Dim::None,     "rpm|%" },
   { "eng3RpmRaw",           Group::Engine,     F_eng3RpmRaw,           Dim::None,     "rpm|%" },
   { "eng4RpmRaw",           Group::Engine,     F_eng4RpmRaw,           Dim::None,     "rpm|%" },

   // TRACK
   { "trackCount",           Group::Track,      F_trackCount,           Dim::None,     "" },
   { "trackRangeM",          Group::Track,      F_trackRangeM,          Dim::Distance, "m" },
   { "trackBearingDeg",      Group::Track,      F_trackBearingDeg,      Dim::Angle,    "deg" },
   { "trackDeltaAltM",       Group::Track,      F_trackDeltaAltM,       Dim::Distance, "m" },

   // STATUS
   { "damage",               Group::Status,     F_damage,               Dim::None,     "0-1" },
   { "crashedFlag",          Group::Status,     F_crashedFlag,          Dim::None,     "0|1" },
   { "killedFlag",           Group::Status,     F_killedFlag,           Dim::None,     "0|1" },
   { "smokeFrac",            Group::Status,     F_smokeFrac,            Dim::None,     "0-1" },
   { "flamesFrac",           Group::Status,     F_flamesFrac,           Dim::None,     "0-1" },
}};

} // namespace

const FieldInfo* findField(const std::string& name)
{
   for (const auto& f : CATALOG) {
      if (name == f.name) {
         // A tabela e escrita a mao; se alguem inserir uma linha no meio sem
         // mexer no enum, o indice deixa de bater com a posicao. Melhor
         // recusar do que ler o campo errado em silencio.
         if (&f - CATALOG.data() != f.index) return nullptr;
         return &f;
      }
   }
   return nullptr;
}

const FieldInfo* fieldByIndex(const int index)
{
   if (index < 0 || index >= F_COUNT) return nullptr;
   return &CATALOG[static_cast<std::size_t>(index)];
}

std::string allFieldNames()
{
   std::string out;
   for (const auto& f : CATALOG) {
      if (!out.empty()) out += ' ';
      out += f.name;
   }
   return out;
}

} // namespace xmsg
} // namespace mixr
