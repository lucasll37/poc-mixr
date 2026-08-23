#ifndef __xnative_runtime_utils_H__
#define __xnative_runtime_utils_H__

#include <string>

namespace mixr {
namespace xnative {

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

//------------------------------------------------------------------------------
// Quadro de status: rotulo do comportamento que venceu, por id de player.
//
// Na poc/12 esse rotulo morava num campo do NOSSO player (xair::Airplane).
// Aqui o player e o models::Aircraft nativo -- que, obviamente, nao tem um
// campo para isso. Como e pura observabilidade, um quadro global por id
// resolve sem subclassear o Player so por causa de uma string. E um bom
// exemplo do preco de herdar tudo: some o lugar natural para guardar o que
// e SEU.
//------------------------------------------------------------------------------
void setBehaviorLabel(int playerId, const std::string& label);
std::string getBehaviorLabel(int playerId);

} // namespace xnative
} // namespace mixr

#endif
