#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// Geracao do cenario a partir do modelo .epp.in.
//
// Uma unica questao: quantas threads de tempo critico o cenario vai declarar
// e como esse numero entra no arquivo. O EDL nao tem variaveis, entao o
// modelo traz o marcador @NUM_TC_THREADS@ e a substituicao acontece aqui,
// ANTES do edl_parser -- e por isso que reconfigurar o pool nao recompila
// nada.
//
// Encerra o processo se o modelo nao puder ser lido: sem cenario nao ha
// simulacao, e um erro parcial aqui so produziria mensagens confusas mais
// adiante.
//------------------------------------------------------------------------------
int generateScenario(const std::string& templatePath, const std::string& outPath,
                     int threadsOverride);

} // namespace app
