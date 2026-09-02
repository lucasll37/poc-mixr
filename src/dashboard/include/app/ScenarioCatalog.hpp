#pragma once

#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// O catalogo de cenarios que este poc sabe carregar -- uma tabela FIXA, nao
// uma descoberta dinamica de arquivo. Os tres sao os que o poc traz consigo
// (ver configs/), cada um hermetico (sem 'networks:') e com porta de
// Tacview/diretorio de dados PROPRIOS (1236, ./src/dashboard/data/), para
// poder rodar ao lado de single-thread/multi-thread (porta 1234) sem
// colidir.
//------------------------------------------------------------------------------
struct ScenarioEntry
{
   std::string key;            // '-scenario <chave>'
   std::string label;          // titulo curto, para a tela de selecao e o cabecalho do dashboard
   std::string description;    // uma linha, para a tela de selecao
   std::string templatePath;   // o .epp.in
};

const std::vector<ScenarioEntry>& scenarioCatalog();

// nullptr se a chave nao existir no catalogo.
const ScenarioEntry* findScenario(const std::string& key);

} // namespace app
