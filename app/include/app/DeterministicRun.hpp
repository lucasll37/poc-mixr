#pragma once

#include "app/Fleet.hpp"

namespace mixr { namespace simulation { class Station; }
namespace xtacview { class TacviewOutput; } }

namespace app {

//------------------------------------------------------------------------------
// O laco de PASSO FIXO ('-deterministic N').
//
// Uma unica questao: rodar N frames sem relogio de parede, para que o
// resultado dependa so do estado da simulacao. Por isso ele chama
// station->tcFrame(dt) DIRETO -- sem passar por processTimeCriticalTasks(),
// que e onde vivem o pool de threads e o controle de velocidade do tempo.
//
// Nao ha sleep, nao ha teclado e nao ha status humano: a saida e o dump de
// app/DeterministicDump.hpp, que os alvos 'make check-*' comparam.
//
// updateData() e chamado no MESMO passo, logo depois do tcFrame(): drena o
// gravador. A decisao em si ja aconteceu na fase 3 do tcFrame -- todo agente
// deste repositorio e um ( FlightAgentTC ), componente do player, dentro do
// pool de tempo critico.
//------------------------------------------------------------------------------
// 'tacviewOutput' pode ser nullptr (cenario sem exportacao). Quando dado, a
// identidade real de cada player e publicada antes de cada updateData() --
// mesma razao e mesma ordem do laco de tempo real (ver
// TacviewOutput::publishIdentities()), para que o .acmi gerado neste modo
// hermetico traga os mesmos Type=/Color=/Name= que a execucao interativa.
int runDeterministic(mixr::simulation::Station* station, const Fleet& fleet, long frames,
                     mixr::xtacview::TacviewOutput* tacviewOutput = nullptr);

} // namespace app
