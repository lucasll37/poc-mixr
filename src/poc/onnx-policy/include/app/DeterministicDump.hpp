#pragma once

#include "app/Fleet.hpp"

namespace app {

//------------------------------------------------------------------------------
// O dump 'frame=' do modo determinista.
//
// Uma unica questao, e ela e um CONTRATO: as linhas produzidas aqui sao o
// que os alvos 'make check-*' comparam entre execucoes com 1, 2 e 4 threads
// T/C. Duas regras seguem disso:
//
//   * so entram grandezas que dependem do ESTADO DA SIMULACAO. Numero de
//     thread, tempo de parede e uso de CPU dependem do escalonador e ficam
//     de fora;
//   * precisao fixa e alta (9 casas), para que uma divergencia numerica
//     minima apareca em vez de ser arredondada.
//------------------------------------------------------------------------------
void printDeterministicDump(const Fleet& fleet, long frame);

} // namespace app
