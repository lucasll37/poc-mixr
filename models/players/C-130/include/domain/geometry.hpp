#pragma once

// namespace ANINHADO em mixr::models::xC_130 -- nao um "domain" solto no
// escopo global (que e o que models/players/A-4 faz, por ter chegado
// primeiro e ja estar documentado em dezenas de lugares daquele jeito).
// Isto e deliberado: um cenario pode carregar MAIS DE UM plugin no mesmo
// processo, e dois tipos com o MESMO nome qualificado ("domain::Foo") em
// dois .so's distintos tem o MESMO simbolo mangled -- a comparacao de
// type_info deste toolchain degrada para strcmp entre objetos RTLD_LOCAL.
// Ver CONTRATO.md/ARCHITECTURE.md deste modelo, secao de namespace.
namespace mixr {
namespace models {
namespace xC_130 {
namespace domain {

// Normaliza um angulo para (-180, 180].
double wrap180(double deg);

// Normaliza um angulo para [0, 360).
double wrap360(double deg);

} // namespace domain
} // namespace xC_130
} // namespace models
} // namespace mixr
