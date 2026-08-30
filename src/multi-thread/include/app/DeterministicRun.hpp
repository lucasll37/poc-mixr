#pragma once

#include "app/Fleet.hpp"

namespace mixr { namespace simulation { class Station; } }

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
// updateData() e chamado no MESMO passo. Onde o agente do UBF e um
// ( SimAgent ) da Station (single-thread), e essa chamada que o faz decidir,
// uma vez por frame; onde ele e um ( FlightAgentTC ) do player
// (multi-thread), a decisao ja aconteceu na fase 3 do tcFrame e updateData()
// so drena o gravador. Nos dois casos o dump sai igual -- e por isso que o
// laco pode ser o mesmo arquivo nos dois subprojetos.
//------------------------------------------------------------------------------
int runDeterministic(mixr::simulation::Station* station, const Fleet& fleet, long frames);

} // namespace app
