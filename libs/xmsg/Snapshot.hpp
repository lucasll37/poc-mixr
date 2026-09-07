#pragma once

#include <cstddef>

namespace mixr {
namespace xmsg {

//------------------------------------------------------------------------------
// Snapshot -- a visao de UM player em UM ciclo, em numeros crus.
//
// Todo campo e um double num array contiguo, indexado pelo enum Field. E o que
// faz "ler o campo que o usuario pediu" custar aritmetica de offset, e nao uma
// busca por nome: a resolucao nome -> indice acontece UMA vez, no reset().
//
// A VALIDADE E POR GRUPO, e isso nao e detalhe. Um player recebido por DIS e
// clonado de um template sem dynamicsModel, entao TODA grandeza de motor dele
// le zero -- e zero e um valor plausivel de empuxo. Sem separar "e zero" de
// "nao existe", uma mensagem de pane com players: { } acusaria falha
// permanente no intruso. Grupo invalido vira 'null' no JSON e faz a condicao
// nao avaliar (ver rules/Schmitt.hpp).
//
// Os rotulos sao buffers fixos: o subsistema inteiro nao aloca em regime.
//------------------------------------------------------------------------------

enum class Group : int {
   Ident, Kinematics, AirData, Fuel, Engine, Track, Status,
   COUNT
};

constexpr int GROUP_COUNT{static_cast<int>(Group::COUNT)};

// A ordem aqui e a ordem do catalogo (FieldCatalog.cpp) -- as duas tem de
// andar juntas, e ha uma verificacao disso na construcao do catalogo.
enum Field : int {
   // IDENT
   F_playerId, F_sideNum, F_modeNum,

   // KINEMATICS
   F_latDeg, F_lonDeg, F_northM, F_eastM,
   F_altMslM, F_altAglM, F_terrainElevM,
   F_hdgDeg, F_pitchDeg, F_rollDeg, F_speedKts, F_climbMps,

   // AIRDATA
   F_machNum, F_gLoad, F_aoaDeg,

   // FUEL
   F_fuelLb, F_fuelMaxLb, F_fuelFrac,

   // ENGINE -- agregados
   F_engCount, F_engThrustTotalLb, F_engThrustMinLb, F_engThrustAsymFrac,
   F_engRpmMinRaw, F_engFuelFlowTotalPph, F_engOilPsiMin, F_engPlaMinPct,
   F_engWorstIndex,
   // ENGINE -- por motor (1-based no nome; ate 4)
   F_eng1ThrustLb, F_eng2ThrustLb, F_eng3ThrustLb, F_eng4ThrustLb,
   F_eng1RpmRaw,   F_eng2RpmRaw,   F_eng3RpmRaw,   F_eng4RpmRaw,

   // TRACK
   F_trackCount, F_trackRangeM, F_trackBearingDeg, F_trackDeltaAltM,

   // STATUS
   F_damage, F_crashedFlag, F_killedFlag, F_smokeFrac, F_flamesFrac,

   F_COUNT
};

constexpr int MAX_ENGINES{4};
constexpr std::size_t NAME_LEN{64};
constexpr std::size_t SHORT_LEN{16};

struct Snapshot
{
   double v[F_COUNT]{};
   bool groupValid[GROUP_COUNT]{};

   // rotulos (texto): viajam na mensagem, mas NAO sao usaveis em condicao --
   // condicao e sempre numerica, e nao ha linguagem de expressao aqui.
   char playerName[NAME_LEN]{};
   char sideName[SHORT_LEN]{};
   char modeName[SHORT_LEN]{};
   char trackName[NAME_LEN]{};

   bool valid(const Group g) const   { return groupValid[static_cast<int>(g)]; }
   void mark(const Group g)          { groupValid[static_cast<int>(g)] = true; }
};

} // namespace xmsg
} // namespace mixr
