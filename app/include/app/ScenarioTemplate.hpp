#pragma once

#include <map>
#include <string>

namespace app {

//------------------------------------------------------------------------------
// Geracao do cenario a partir do modelo .epp.in.
//
// Questao original: quantas threads de tempo critico o cenario vai declarar
// e como esse numero entra no arquivo. O EDL nao tem variaveis, entao o
// modelo traz o marcador @NUM_TC_THREADS@ e a substituicao acontece aqui,
// ANTES do edl_parser -- e por isso que reconfigurar o pool nao recompila
// nada.
//
// Estendida com dois mecanismos GENERICOS, na mesma tecnica (substituicao
// literal de string, ANTES do parser EDL ver o arquivo):
//   1) '@include:nome@' -- troca pelo conteudo de
//      app/configs/fragments/nome, permitindo que um bloco verdadeiramente
//      identico entre cenarios (ex.: o dataRecorder/TacviewOutput, ~95%
//      byte-identico entre os 3 cenarios de app/configs/) saia de cada
//      '.epp.in' e va para UM fragmento so.
//   2) 'extraTokens' -- um mapa de '@NOME@' -> valor, resolvido DEPOIS dos
//      includes (um fragmento pode conter tokens que so o CHAMADOR sabe
//      preencher, ex.: '@SCENARIO_ID@').
//
// Encerra o processo se o modelo (ou um fragmento incluido) nao puder ser
// lido: sem cenario nao ha simulacao, e um erro parcial aqui so produziria
// mensagens confusas mais adiante.
//------------------------------------------------------------------------------
int generateScenario(const std::string& templatePath, const std::string& outPath,
                     int threadsOverride, const std::map<std::string, std::string>& extraTokens = {});

} // namespace app
