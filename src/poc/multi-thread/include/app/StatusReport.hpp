#pragma once

#include "app/Fleet.hpp"

#include <string>

namespace app {

//------------------------------------------------------------------------------
// A linha de status legivel por humano, impressa no laco de tempo real.
//
// Uma unica questao: FORMATO. Nao le nada que outros modulos ja nao exponham
// e nao muda estado nenhum -- e o modo determinista tem o seu proprio
// formato (ver app/DeterministicDump.hpp), de proposito: um serve para
// olhar, o outro para comparar byte a byte.
//
// 't' e tempo de PAREDE, 'sim' e tempo SIMULADO. Com o relogio acelerado,
// freado ou pausado os dois deixam de andar juntos -- e essa diferenca e a
// prova de que o controle de tempo esta agindo.
//------------------------------------------------------------------------------
void printStatus(const Fleet& fleet, double elapsedSec, double simSec,
                 const std::string& clockLabel);

} // namespace app
