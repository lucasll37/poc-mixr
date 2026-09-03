#pragma once

#include <vector>

namespace mixr {
namespace models { class Player; class WorldModel; }
}

namespace app {

//------------------------------------------------------------------------------
// TODOS os players do cenario recebido, do tipo que forem -- sem lista de
// nomes fixa, sem suposicao de que sao AirVehicle. O sim-runner nunca sabe
// de antemao quais players o cliente vai declarar, entao a descoberta tem
// de ser generica -- mesma logica de app::discoverPlayers
// (app/include/app/Fleet.hpp), copiada aqui porque os dois subprojetos nao
// compartilham codigo entre si (convencao do repo).
//------------------------------------------------------------------------------
std::vector<mixr::models::Player*> discoverPlayers(mixr::models::WorldModel* wm);

} // namespace app
