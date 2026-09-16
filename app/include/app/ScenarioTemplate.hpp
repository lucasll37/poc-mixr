#pragma once

#include <map>
#include <string>

namespace app {

//------------------------------------------------------------------------------
// Geracao do cenario a partir do modelo .edl.in.
//
// Questao original: quantas threads de tempo critico e de background o
// cenario vai declarar e como esses numeros entram no arquivo. O EDL nao
// tem variaveis, entao o modelo traz os marcadores @NUM_TC_THREADS@ e
// @NUM_BG_THREADS@ e a substituicao acontece aqui, ANTES do edl_parser --
// e por isso que reconfigurar os dois pools nao recompila nada. Os dois
// slots (numTcThreads/numBgThreads) sao nativos do framework -- ver
// mixr::simulation::Simulation::setSlotNumTcThreads()/setSlotNumBgThreads()
// -- e ja compartilhavam o mesmo clamp [1, nucleos-1]; so o DEFAULT quando
// nada e' pedido difere entre os dois (ver resolveTcThreadCount()/
// resolveBgThreadCount() no .cpp).
//
// Estendida com dois mecanismos GENERICOS, na mesma tecnica (substituicao
// literal de string, ANTES do parser EDL ver o arquivo):
//   1) '@include:nome@' -- troca pelo conteudo de
//      app/configs/fragments/nome, permitindo que um bloco verdadeiramente
//      identico entre cenarios (ex.: o dataRecorder/TacviewOutput, ~95%
//      byte-identico entre os 3 cenarios de app/configs/) saia de cada
//      '.edl.in' e va para UM fragmento so.
//   2) 'extraTokens' -- um mapa de '@NOME@' -> valor, resolvido DEPOIS dos
//      includes (um fragmento pode conter tokens que so o CHAMADOR sabe
//      preencher, ex.: '@SCENARIO_ID@', ou '@RUN_ID@' -- um carimbo por
//      PROCESSO que main.cpp injeta para TODO cenario, ver runIdNow() la).
//
// NAO HA DEFAULT para 'extraTokens': a substituicao so percorre os pares do
// mapa e troca cada um no texto -- nunca o caminho inverso (varrer o texto
// atras de tokens e perguntar se ha valor). Um '@ALGO@' que sobra sem par
// fica LITERAL no '.generated.edl' e quase sempre vira "syntax error" do
// edl_parser, sem dizer qual token faltou -- quem usa um token novo num
// '.edl.in' tem que garantir que o chamador (main.cpp) sempre o preenche.
// @NUM_TC_THREADS@/@NUM_BG_THREADS@ SAO diferentes disso: sempre tem
// default (ver o .cpp), nunca sobram sem substituicao.
//
// Encerra o processo se o modelo (ou um fragmento incluido) nao puder ser
// lido: sem cenario nao ha simulacao, e um erro parcial aqui so produziria
// mensagens confusas mais adiante.
//------------------------------------------------------------------------------
struct ThreadCounts
{
   int tc{};   // numTcThreads resolvido -- substitui @NUM_TC_THREADS@
   int bg{};   // numBgThreads resolvido -- substitui @NUM_BG_THREADS@
};

ThreadCounts generateScenario(const std::string& templatePath, const std::string& outPath,
                              int tcThreadsOverride, int bgThreadsOverride,
                              const std::map<std::string, std::string>& extraTokens = {});

} // namespace app
