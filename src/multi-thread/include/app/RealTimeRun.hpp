#pragma once

#include "app/Fleet.hpp"

namespace mixr {
namespace simulation { class Station; }
namespace models { class WorldModel; }
namespace xclock { class ClockStation; }
}

namespace app {

//------------------------------------------------------------------------------
// O laco de TEMPO REAL -- o modo normal de execucao.
//
// Uma unica questao: manter o laco de background andando junto com o relogio
// de parede. Ele cuida de tres coisas e so delas:
//
//   * teclado de controle de tempo (xclock::TimeControls, um read() nao
//     bloqueante por volta);
//   * station->updateData(dt) -- que DRENA A FILA DO GRAVADOR para o
//     Tacview (sem isso nada chega la);
//   * o sleep que acerta o passo com o tempo de parede.
//
// O frame de tempo critico NAO acontece aqui: quem o roda e o pool nativo
// criado por createTimeCriticalProcess().
//
// ONDE A DECISAO ENTRA depende do agente do cenario, e essa e a unica coisa
// que muda entre a poc/single-thread e a poc/multi-thread: um ( SimAgent ) e componente da
// Station e decide DENTRO deste updateData() -- na taxa deste laco, 10 Hz;
// um ( FlightAgentTC ) e componente do player e decide na fase 3 do frame --
// na taxa do tempo critico, 50 Hz. O codigo do laco e o mesmo nos dois casos.
//
// Ctrl+C encerra -- o handler so marca uma flag; sair do laco e devolver o
// terminal ao normal e responsabilidade do fluxo normal de saida.
//------------------------------------------------------------------------------
void runRealTime(mixr::simulation::Station* station, const Fleet& fleet,
                 mixr::models::WorldModel* worldModel,
                 mixr::xclock::ClockStation* clockStation);

} // namespace app
