#pragma once

#include <string>

namespace mixr {
namespace simulation { class Station; }
namespace models { class WorldModel; }
namespace xclock { class ClockStation; }
}

namespace app {

//------------------------------------------------------------------------------
// Do arquivo EDL a Station de pe.
//
// Uma unica questao: transformar o .epp em objetos vivos e conferir que o
// que veio de la e o que a aplicacao espera. Toda falha aqui e fatal -- uma
// Station meio construida nao tem uso.
//------------------------------------------------------------------------------

// edl_parser + desembrulho do Pair de topo.
mixr::simulation::Station* buildStation(const std::string& filename);

// RESET_EVENT + um frame de partida (e o que faz os subsistemas resolverem
// os ponteiros entre si antes do primeiro uso).
void primeStation(mixr::simulation::Station* station);

mixr::models::WorldModel* worldModelOf(mixr::simulation::Station* station);

// O cenario declara ( ClockStation ): uma Station com controle de velocidade
// do tempo. Trocar por ( Station ) no .epp continua rodando -- so sem as
// teclas -- entao um nullptr aqui e aviso, nao erro fatal.
mixr::xclock::ClockStation* clockStationOf(mixr::simulation::Station* station);

} // namespace app
