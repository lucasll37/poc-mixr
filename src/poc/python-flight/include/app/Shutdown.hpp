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
// Nao e cerimonia. Medido no ./app (raiz), sob pty, com um cliente de teste
// que so conecta na porta do Tacview e nunca le: o processo NAO terminava --
// a thread do laco de tempo real travada num ::send() sem teto, a thread T/C
// nativa seguindo enfileirando registros numa base::List SEM TETO (RSS
// subindo), e nenhum dos dois se resolvia com Ctrl+C/saida normal. O teto de
// escrita (SO_SNDTIMEO, ver shared/xtacview/RealtimeTelemetryServer.cpp) so
// resolve METADE do problema -- a outra metade e a ORDEM de encerramento, e e
// essa a funcao deste arquivo.
//
// Esta poc corre o MESMO risco que o ./app corria: app/RealTimeRun.cpp chama
// station->createTimeCriticalProcess() e depois roda station->updateData(dt)
// em laco (aqui na propria thread principal, sem thread separada -- mas o
// efeito e o mesmo: a thread T/C nativa sobrevive ao fim do laco). Sem esta
// correcao, 'main.cpp' fazia so 'station->event(SHUTDOWN_EVENT); unref();' na
// volta de runRealTime() -- exatamente a ordem que produz o auto-deadlock
// documentado em xclock/ClockStation.hpp.
//
// A ordem CORRETA de encerramento, e o porque de cada passo:
//
//   1. quiesceTimeCritical()  -- cala a PRODUTORA primeiro. A thread T/C
//      nativa nao morre com o fim do laco de tempo real, e o framework nao
//      oferece jeito de para-la (ver o comentario grande em
//      xclock/ClockStation.hpp).
//   2. shutdownStation()      -- so agora o SHUTDOWN_EVENT, sem ninguem
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
