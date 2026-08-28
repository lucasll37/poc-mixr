#pragma once

namespace domain {

// Comando de voo "de alto nivel" produzido pelas regras de negocio puras
// (patrulha, RTB, evasao) e consumido pelos nos da arvore de comportamento,
// que o traduzem para o dynamics model. Deliberadamente NAO conhece nenhum
// tipo do MIXR nem do BehaviorTree.CPP -- e so um DTO.
//
// Unidades explicitas no nome do campo: a armadilha classica desta PoC
// (documentada no CLAUDE.md, poc/03) e misturar pes/metros/nos.
struct FlightCommand
{
   double headingDeg{};    // rumo verdadeiro comandado (graus)
   double altitudeM{};     // altitude comandada (metros)
   double speedKts{};      // velocidade comandada (nos)
};

} // namespace domain
