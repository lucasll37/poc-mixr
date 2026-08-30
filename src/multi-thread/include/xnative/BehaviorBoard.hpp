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

} // namespace xnative
} // namespace mixr

#endif
