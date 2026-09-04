#pragma once

#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// O catalogo de cenarios que o ./app sabe carregar -- uma tabela FIXA, nao uma
// descoberta dinamica de arquivo.
//
// Duas familias convivem aqui, e a diferenca entre elas e so de EDL:
//
//   * os cenarios DO PROPRIO app (configs/scenario_*.epp.in) -- herméticos,
//     porta de Tacview e diretorio de dados proprios (1236, ./app/data/), e
//     escritos com '@include:tacview_recorder.epp.frag@' + os quatro tokens
//     de Tacview abaixo, que so eles usam;
//   * os cenarios DAS POCS (src/poc/**/configs/*.epp.in) -- cada um com a
//     porta e o diretorio de dados dele, e com o bloco 'dataRecorder:'
//     escrito inteiro no proprio arquivo. Para esses os quatro campos de
//     Tacview ficam VAZIOS: a substituicao de token simplesmente nao acha
//     nada para trocar, e o arquivo passa intacto.
//
// O ./app e o UNICO runner das pocs -- elas nao tem mais executavel proprio.
// Era uma camada de aplicacao de ~1.500 linhas copiada em cada pasta,
// sustentada pela guarda check_duplication.sh; hoje a duplicacao esta
// dissolvida por construcao, do mesmo jeito que aconteceu quando o MODELO
// saiu para models/flight.
//------------------------------------------------------------------------------
struct ScenarioEntry
{
   std::string key;            // '-scenario <chave>'
   std::string label;          // titulo curto, para a tela de selecao e o cabecalho do dashboard
   std::string description;    // uma linha, para a tela de selecao
   std::string templatePath;   // o .epp.in

   // Os 4 tokens que app/configs/fragments/tacview_recorder.epp.frag
   // precisa (ver app::generateScenario()/ScenarioTemplate.cpp) -- o
   // esqueleto do dataRecorder/TacviewOutput e IDENTICO nos tres cenarios
   // deste catalogo; so estes 4 pedacos mudam. 'tacviewId' pode divergir
   // de 'key' (ex.: key="intercept_missile" mas tacviewId=
   // "intercept-missile", pelo nome de arquivo .acmi ja em producao).
   std::string tacviewId;
   std::string tacviewModelMap;
   std::string tacviewTypeMap;
   std::string tacviewColorMap;

   // A FROTA observada: quais players entram no dump de '-deterministic' e
   // recebem o manete de cruzeiro. E por cenario, nao global, porque nem todo
   // cenario tem falcon1..4 -- o 'bandit' tem UM player so. collectFleet()
   // aborta se um nome daqui nao existir, e e isso que se quer: um cenario
   // que perdeu um player tem de falhar alto, nao silenciosamente imprimir
   // menos linhas.
   std::vector<std::string> fleet;
};

// A frota das pocs de voo e do proprio app: os quatro falcons. O intruso
// (bandit1), quando existe, NAO entra -- ele nao e observado, e nenhum dump
// deste repositorio jamais o imprimiu.
const std::vector<std::string>& falconFleet();

const std::vector<ScenarioEntry>& scenarioCatalog();

// nullptr se a chave nao existir no catalogo.
const ScenarioEntry* findScenario(const std::string& key);

// Entrada SINTETICA para '-f <arquivo>': um cenario fora do catalogo (o caso
// das fixtures de teste, geradas por tests/scenario/make_fixture.py). Herda a
// frota dos falcons e nao substitui token de Tacview nenhum -- a fixture ja
// traz o bloco 'dataRecorder:' inteiro, como o cenario de poc de que ela
// deriva.
ScenarioEntry adHocScenario(const std::string& path);

} // namespace app
