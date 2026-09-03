#ifndef __app_Shutdown_H__
#define __app_Shutdown_H__

namespace mixr {
namespace simulation { class Station; }
namespace xclock { class ClockStation; }
}

namespace app {

//------------------------------------------------------------------------------
// A QUESTAO DESTE ARQUIVO: como encerrar sem travar o processo.
//
// Nao e cerimonia. Medido no ./app, sob pty, com um cliente de teste que so
// conecta na porta do Tacview e nunca le: ao apertar [q] o processo NAO
// terminava -- main travada em futex (o join() da thread do laco de
// background), a thread do laco travada num ::send() sem teto, quatro
// threads do pool de tempo critico girando a ~100% em base::lock() (que e
// spin puro, sem yield -- atomics_linux.hpp:18-24) e o RSS subindo ~1,9 MB/s
// porque a thread de tempo critico seguia enfileirando registros numa fila
// que e uma base::List SEM TETO. O terminal nunca voltava. As duas metades da
// correcao sao o teto de escrita no socket (shared/xtacview) e as duas
// funcoes abaixo.
//
// A ordem CORRETA de encerramento, e o porque de cada passo:
//
//   1. quiesceTimeCritical()  -- cala a PRODUTORA primeiro. A thread T/C
//      nativa sobrevive ao fim do TUI e o framework nao oferece jeito de
//      para-la (ver o comentario grande em xclock/ClockStation.hpp).
//   2. parar o laco de background e dar uma ultima drenagem -- com a
//      producao parada, a fila do gravador fecha em uma passada.
//   3. shutdownStation()      -- so agora o SHUTDOWN_EVENT, sem ninguem
//      correndo contra ele.
//------------------------------------------------------------------------------

// Deixa a thread de tempo critico nativa ociosa e PROVA isso (nao apenas
// dorme um tanto). 'true' se confirmou; 'false' se o teto venceu -- nesse
// caso o chamador segue de qualquer forma, que e o comportamento de hoje.
//
// 'clockStation' pode ser nullptr (cenario com '( Station )' pura em vez de
// '( ClockStation )'): cai num fallback 100% nativo, sem prova positiva.
bool quiesceTimeCritical(mixr::simulation::Station* station,
                         mixr::xclock::ClockStation* clockStation,
                         double timeoutSec = 2.0);

// SHUTDOWN_EVENT + unref(), com um watchdog que garante que o usuario sempre
// recupera o shell. O caminho limpo continua sendo o caminho normal -- o
// watchdog so existe para o caso ruim (ver o "porque" no .cpp).
void shutdownStation(mixr::simulation::Station* station, double watchdogSec = 10.0);

}

#endif
