#pragma once

#include <string>

namespace mixr {
namespace simulation { class Station; }
namespace models { class WorldModel; }
}

namespace app {

//------------------------------------------------------------------------------
// Do arquivo EDL a Station de pe.
//
// Versao enxuta de src/poc/single-thread/include/app/StationBuilder.hpp: so
// buildStation/primeStation/worldModelOf. O sim-runner nao declara
// ( ClockStation ), ( JoystickIoHandler ) nem Tacview -- so ( Station ) pura
// -- entao clockStationOf/ioHandlerOf/tacviewOutputOf nao fazem sentido
// aqui. Toda falha em buildStation() e fatal -- uma Station meio construida
// nao tem uso.
//------------------------------------------------------------------------------

// edl_parser + desembrulho do Pair de topo.
mixr::simulation::Station* buildStation(const std::string& filename);

// RESET_EVENT + um frame de partida (e o que faz os subsistemas resolverem
// os ponteiros entre si antes do primeiro uso).
void primeStation(mixr::simulation::Station* station);

mixr::models::WorldModel* worldModelOf(mixr::simulation::Station* station);

} // namespace app
