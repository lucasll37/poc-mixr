#pragma once

namespace mixr {
namespace simulation { class Station; }
namespace xclock { class ClockStation; }
}

//------------------------------------------------------------------------------
// O laco de tempo real (sem TUI, sem status line -- so o que station->
// updateData(dt) ja loga por conta propria via LOG(...)) e o encerramento
// limpo de uma Station. A ordem entre as duas funcoes importa -- ver os
// comentarios de run()/shutdownStation() em Run.cpp.
//------------------------------------------------------------------------------
namespace node {

// Roda ate SIGINT/SIGTERM. Ao sair, ja cala a thread de tempo critico e
// drena o gravador uma ultima vez -- devolve o controle com a Station
// pronta para shutdownStation(), nao antes.
int run(mixr::simulation::Station* station, mixr::xclock::ClockStation* clockStation);

// SHUTDOWN_EVENT + unref(), com um watchdog que forca a saida do processo
// se o teardown nativo travar (ver o comentario da funcao em Run.cpp).
void shutdownStation(mixr::simulation::Station* station, double watchdogSec = 10.0);

}
