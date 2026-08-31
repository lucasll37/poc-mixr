#ifndef __xnative_BehaviorBoard_H__
#define __xnative_BehaviorBoard_H__

#include <string>

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Quadro de status: rotulo do comportamento que venceu, por id de player.
//
// Uma unica questao, e ela e pura observabilidade. O player e o
// models::Aircraft nativo, que nao tem campo proprio para isso -- um quadro
// global por id resolve sem subclassear o Player so por causa de uma string.
//
// Escrito pela atuacao (ubf::FlightAction, em thread de tempo critico) e
// lido pelo laco de background -- por isso o acesso e sincronizado.
//------------------------------------------------------------------------------

void setBehaviorLabel(int playerId, const std::string& label);
std::string getBehaviorLabel(int playerId);

//------------------------------------------------------------------------------
// Contagem de decisoes efetivamente ATUADAS, por id de player.
//
// Existe para o dump deterministico poder afirmar -- e nao so imprimir -- que
// a decisao esta amarrada ao frame: em passo fixo o numero tem de ser igual
// ao de frames, com 1, 2 ou 4 threads de tempo critico. Nem duas decisoes no
// mesmo frame, nem frame sem decisao.
//
// Conta no mesmo ponto em que o rotulo e escrito (ubf::FlightAction::execute),
// que roda uma vez por ciclo do agente, ja depois de o UbfArbiter ter
// escolhido o vencedor -- ou seja, conta DECISAO, nao candidatura.
//
// E funcao pura do estado da simulacao (nao carrega tempo de parede nem
// indice de thread), entao respeita o contrato de app/DeterministicDump.hpp.
//------------------------------------------------------------------------------
void bumpDecisionCount(int playerId);
long getDecisionCount(int playerId);

} // namespace xnative
} // namespace mixr

#endif
