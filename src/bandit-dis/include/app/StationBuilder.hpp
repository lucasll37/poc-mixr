#pragma once

#include <string>

namespace mixr {
namespace simulation { class Station; }
namespace models { class WorldModel; }
namespace linkage { class IoHandler; }
}

namespace app {

//------------------------------------------------------------------------------
// Do arquivo EDL a Station de pe.
//
// Versao enxuta da mesma questao de single-thread/multi-thread (ver os
// comentarios la): sem ClockStation aqui -- esta poc nao precisa de
// controle de velocidade do tempo, so do joystick/DIS.
//------------------------------------------------------------------------------

// edl_parser + desembrulho do Pair de topo.
mixr::simulation::Station* buildStation(const std::string& filename);

// RESET_EVENT + um frame de partida.
void primeStation(mixr::simulation::Station* station);

mixr::models::WorldModel* worldModelOf(mixr::simulation::Station* station);

// O cenario declara ( JoystickIoHandler ) no slot NATIVO 'ioHandler:' da
// Station -- nullptr aqui e aviso, nao erro fatal (mesmo raciocinio de
// single-thread/multi-thread).
mixr::linkage::IoHandler* ioHandlerOf(mixr::simulation::Station* station);

} // namespace app
