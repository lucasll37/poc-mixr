#ifndef __app_Shutdown_H__
#define __app_Shutdown_H__

namespace mixr { namespace simulation { class Station; } }

namespace app {

//------------------------------------------------------------------------------
// A QUESTAO DESTE ARQUIVO: como encerrar sem travar o processo.
//
// Versao enxuta do padrao ja usado em single-thread/multi-thread/./app (ver
// o "porque" longo la, e xclock/ClockStation.hpp para o auto-deadlock que
// motivou isto): esta poc usa '( Station )' pura no cenario (sem
// ClockStation, ver main.cpp), entao so existe o fallback 100% nativo --
// nao ha 'requestTcStop()'/'waitForTcQuiesced()' para chamar.
//
// O risco e o MESMO das gemeas: app/RealTimeRun.cpp chama
// station->createTimeCriticalProcess() e depois roda station->updateData(dt)
// em laco; sem calar essa thread T/C ANTES do SHUTDOWN_EVENT, ela corre
// contra o teardown da Simulation (auto-deadlock) e/ou segue enfileirando
// registros numa fila sem teto enquanto ninguem mais a drena.
//------------------------------------------------------------------------------

// Deixa a thread de tempo critico nativa ociosa, sem prova positiva (a
// Station nativa nao tem por onde avisar sem um ClockStation -- so a espera
// de alguns periodos T/C). 'true' se nao havia nada a calar.
bool quiesceTimeCritical(mixr::simulation::Station* station);

// SHUTDOWN_EVENT + unref(), com um watchdog que garante que o usuario sempre
// recupera o shell. O caminho limpo continua sendo o caminho normal -- o
// watchdog so existe para o caso ruim (ver o "porque" no .cpp).
void shutdownStation(mixr::simulation::Station* station, double watchdogSec = 10.0);

}

#endif
