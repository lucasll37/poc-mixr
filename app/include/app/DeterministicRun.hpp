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
// updateData() e chamado no MESMO passo. Onde o agente do UBF e um
// ( SimAgent ) da Station (single-thread), e essa chamada que o faz decidir,
// uma vez por frame; onde ele e um ( FlightAgentTC ) do player
// (multi-thread), a decisao ja aconteceu na fase 3 do tcFrame e updateData()
// so drena o gravador. Nos dois casos o dump sai igual -- e por isso que o
// laco pode ser o mesmo arquivo nos dois subprojetos.
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 'parallelDecision' EXISTE PARA QUEBRAR O DETERMINISMO, de proposito.
//
// Com ele, updateData() nao e mais chamado DEPOIS do tcFrame(): os dois rodam
// CONCORRENTEMENTE, que e exatamente a configuracao do tempo real
// (createTimeCriticalProcess() poe o frame numa thread propria enquanto o laco
// de background chama updateData na outra) -- so que sem o relogio de parede,
// para isolar a concorrencia como unica variavel.
//
// O que isso demonstra: onde a decisao roda no laco de BACKGROUND
// (( SimAgent ), poc single-thread), ela passa a ler o estado do player NO MEIO
// da integracao da dinamica, num ponto que muda a cada execucao -- e o dump
// deixa de ser reproduzivel. Onde a decisao roda na FASE 3 do frame
// (( FlightAgentTC ), poc multi-thread), ela continua dentro da barreira e o
// dump nao muda.
//
// E o controle NEGATIVO do 'make check-*': prova que o determinismo vem de ONDE
// a decisao roda, e nao de sorte.
//------------------------------------------------------------------------------
// 'tacviewOutput' pode ser nullptr (cenario sem exportacao). Quando dado, a
// identidade real de cada player e publicada antes de cada updateData() --
// mesma razao e mesma ordem do laco de tempo real (ver
// TacviewOutput::publishIdentities()), para que o .acmi gerado neste modo
// hermetico traga os mesmos Type=/Color=/Name= que a execucao interativa.
// So no ramo SEQUENCIAL: com 'parallelDecision' o updateData() roda noutra
// thread, e publicar daqui seria uma corrida de verdade sobre o cache do
// handler.
int runDeterministic(mixr::simulation::Station* station, const Fleet& fleet, long frames,
                     mixr::xtacview::TacviewOutput* tacviewOutput = nullptr,
                     bool parallelDecision = false);

} // namespace app
