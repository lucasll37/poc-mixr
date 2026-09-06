#pragma once

#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// A pasta navegavel de '-folder <pasta>': N subpastas, cada uma com uma UNICA
// 'configs/*.edl' (ou '.edl.in') dentro. Ao contrario do catalogo estatico
// (ScenarioCatalog.hpp, uma tabela fixa no codigo), isto e descoberta em
// DISCO, em tempo de execucao -- pensado para cenarios de sandbox/
// experimentacao do usuario, que nao precisam entrar em C++ pra isso.
//------------------------------------------------------------------------------
struct FolderScenarioEntry
{
   std::string name;      // nome da subpasta -- tambem o valor aceito por
                          // '-folder <pasta> -scenario <name>' pra pular a
                          // tela de navegacao
   std::string edlPath;   // o .edl/.edl.in UNICO achado em '<subpasta>/configs/'
};

// Varre '<pasta>/*/configs/*.edl' (ou '.edl.in'):
//   - uma subpasta sem 'configs/' e simplesmente ignorada, sem aviso (nem
//     toda subpasta de <pasta> precisa ser cenario);
//   - 'configs/' com 0 ou mais de 1 arquivo .edl/.edl.in gera um AVISO em
//     stderr e e pulada -- a navegacao e por SUBPASTA, precisa de exatamente
//     um arquivo pra ser inequivoco;
//   - '*.generated.edl' NAO conta como candidato, mesmo terminando em
//     '.edl' -- e o artefato de SAIDA do proprio pipeline deste app
//     (ScenarioTemplate::generateScenario(), ver main.cpp), nunca uma fonte
//     valida. Sem essa exclusao, uma pasta com 'scenario.edl.in' +
//     'scenario.generated.edl' (leftover de uma execucao anterior) seria
//     descartada como ambigua por engano.
//   - 'pasta' ausente devolve lista vazia (main.cpp decide se isso e fatal).
// Resultado ordenado por nome, para navegacao previsivel.
std::vector<FolderScenarioEntry> discoverFolderScenarios(const std::string& pasta);

} // namespace app
