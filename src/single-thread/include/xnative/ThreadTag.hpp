#ifndef __xnative_ThreadTag_H__
#define __xnative_ThreadTag_H__

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Identidade da thread que esta executando.
//
// Uma unica questao, e ela existe por causa do multithread: os subsistemas
// desta poc rodam DENTRO das threads de tempo critico criadas pelo pool
// nativo da Simulation (slot 'numTcThreads'), e nao no laco do main.cpp.
//
// O MIXR nao expoe os handles do seu pool ('tcThreads' e privado em
// simulation::Simulation), mas o nosso codigo RODA nessas threads -- entao
// podemos registrar, de dentro, qual thread esta processando cada player. E
// assim que o round-robin do pool fica observavel.
//------------------------------------------------------------------------------

// Indice pequeno e estavel (0,1,2...) para a thread chamadora.
int threadTag();

// Nucleo em que a thread chamadora esta neste instante -- diagnostico de
// afinidade; muda a qualquer momento, por conta do escalonador.
int currentCpu();

} // namespace xnative
} // namespace mixr

#endif
