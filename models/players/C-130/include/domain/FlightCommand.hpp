#pragma once

// namespace ANINHADO em mixr::models::xC_130 -- ver o comentario equivalente
// em domain/geometry.hpp para o "porque" (colisao de RTTI entre .so's
// RTLD_LOCAL).
namespace mixr {
namespace models {
namespace xC_130 {
namespace domain {

// Comando de voo "de alto nivel" produzido pela arvore de comportamento e
// consumido por ubf::FlightAction, que o traduz para o Autopilot nativo.
// Deliberadamente NAO conhece nenhum tipo do MIXR nem do BehaviorTree.CPP --
// e so um DTO. Mesmo desenho de models/players/A-4/include/domain/FlightCommand.hpp.
//
// Unidades explicitas no nome do campo -- a armadilha classica e misturar
// pes/metros/nos (ver "Gotchas de unidades e de modelo" no CLAUDE.md raiz).
struct FlightCommand
{
   double headingDeg{};    // rumo verdadeiro comandado (graus)
   double altitudeM{};     // altitude comandada (metros)
   double speedKts{};      // velocidade comandada (nos)
};

} // namespace domain
} // namespace xC_130
} // namespace models
} // namespace mixr
