#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// Preparacao do banco de elevacao antes do parse do cenario.
//
// Copia byte-a-byte de src/poc/single-thread/include/app/TerrainData.hpp --
// mesma questao, mesmo antidoto (ver o cabecalho de la para o detalhe das
// duas armadilhas do SrtmHgtFile). O sim-runner usa o MESMO tile que as
// outras pocs (e o terreno do CENARIO, nao do modelo).
//------------------------------------------------------------------------------
void ensureTerrainData(const std::string& dir, const std::string& baseName);

} // namespace app
