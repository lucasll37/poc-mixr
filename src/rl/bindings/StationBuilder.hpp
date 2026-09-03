#pragma once

#include <string>

namespace mixr {
namespace simulation { class Station; }
namespace models { class WorldModel; }
}

namespace rl {

//------------------------------------------------------------------------------
// Do arquivo EDL a Station de pe -- mesma questao, mesma implementacao de
// app/include/app/StationBuilder.hpp, so que SEM clockStationOf()/
// ioHandlerOf()/tacviewOutputOf(): este host nunca precisa alcancar esses
// objetos de volta (nao ha teclado, nao ha joystick, e a exportacao pro
// Tacview roda sozinha por dentro de station->updateData(), sem nenhum
// codigo daqui empurrando dado nela).
//------------------------------------------------------------------------------

// edl_parser + desembrulho do Pair de topo.
mixr::simulation::Station* buildStation(const std::string& filename);

// RESET_EVENT + um frame de partida (resolve ponteiros entre subsistemas
// antes do primeiro uso). Tambem o que NativeSimulation::reset() chama em
// toda chamada DEPOIS da primeira, no MESMO processo -- ver o cabecalho de
// NativeSimulation.hpp para o porque disso ser um risco a verificar.
void primeStation(mixr::simulation::Station* station);

mixr::models::WorldModel* worldModelOf(mixr::simulation::Station* station);

} // namespace rl
