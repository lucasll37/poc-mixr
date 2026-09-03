#pragma once

#include <nlohmann/json.hpp>

#include <vector>

namespace mixr {
namespace models { class Player; }
}

namespace app {

//------------------------------------------------------------------------------
// WorldModel -> JSON. Le os MESMOS dados que
// src/poc/single-thread/include/app/DeterministicDump.hpp e
// app/include/app/DashboardState.hpp ja leem (xboard::get, xtrack::
// nearestHostileTrack, getters nativos de Player/AirVehicle) -- so troca a
// serializacao (texto/struct em memoria) por nlohmann::json, que e o que
// atravessa a stdout do sim-runner ate o server.
//
// Campos BASE (nome/tipo/side/majorType/mode/posicao/altitude/atitude) vem
// de mixr::models::Player e por isso existem para QUALQUER player, do tipo
// que for -- mesmo padrao "agnostico a tipo" de app::discoverPlayers/
// DashboardState (ver o CLAUDE.md, secao "Redesenho: agnostico a tipo de
// modelo"). Campos extras (mach/fuelWtLbs/rollDeg/pitchDeg) so aparecem
// quando dynamic_cast<AirVehicle*> funciona. behaviorLabel/decisions/track/
// alert vem de shared/xboard e shared/xtrack -- presentes so para players
// com um agente (FlightAgentTC) no cenario.
//------------------------------------------------------------------------------
nlohmann::json playerToJson(mixr::models::Player* player, double refLatDeg, double refLonDeg);

nlohmann::json fleetToJson(const std::vector<mixr::models::Player*>& players,
                           double refLatDeg, double refLonDeg);

} // namespace app
