#ifndef __xdrone_runtime_utils_H__
#define __xdrone_runtime_utils_H__

#include <string>

namespace mixr {
namespace xdrone {

//------------------------------------------------------------------------------
// Utilitarios de runtime usados pelos modelos proprios desta PoC.
//
// Existem por causa do multithread: os subsistemas abaixo rodam DENTRO das
// threads de tempo critico criadas pelo pool nativo da Simulation
// (slot 'numTcThreads'), e nao no laco do main.cpp.
//
//  - threadTag(): devolve um indice pequeno e estavel (0,1,2...) para a
//    thread que estiver chamando. O MIXR nao expoe os handles do seu pool
//    (tcThreads e privado em simulation::Simulation -- ver CLAUDE.md), mas
//    o nosso proprio codigo RODA nessas threads, entao podemos registrar
//    de dentro qual thread esta executando cada player. E assim que esta
//    poc demonstra o round-robin do pool de forma observavel.
//
//  - logLine(): std::cout nao e sincronizado entre threads; sem um mutex,
//    linhas impressas por dois players em threads diferentes se
//    entrelacam. Usar apenas em eventos raros (troca de estado da arvore),
//    nunca a cada frame.
//------------------------------------------------------------------------------

int threadTag();

// Liga/desliga o log de transicao de estado. O modo '-deterministic' do
// main.cpp desliga: essas linhas carregam o numero da thread, que depende
// do escalonador e nao do estado da simulacao.
void setLoggingEnabled(bool enabled);

int currentCpu();

void logLine(const std::string& line);

} // namespace xdrone
} // namespace mixr

#endif
