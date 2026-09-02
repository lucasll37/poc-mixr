#pragma once

#include <functional>

namespace mixr {
namespace models { class WorldModel; }
}

//------------------------------------------------------------------------------
// Ponte ENTRE o terreno nativo do MIXR (mixr::terrain::Terrain, so alcancavel
// por lat/lon) e app/MapPanel.cpp (que so conhece N/E em metros, relativos ao
// ponto de referencia do cenario) -- ver o comentario grande de
// TerrainSampler, abaixo.
//
// MapPanel.cpp continua "puro" (sem incluir headers do MIXR): so recebe um
// std::function ja pronto pra chamar com (northM, eastM).
//
// NAO usa mais so o tile que o cenario declarou em EDL (WorldModel::
// getTerrain(), um Terrain so) -- carrega TODOS os '.hgt' validos de
// shared/data/terrain/srtm/ (ver app/TerrainData.hpp::ensureAllTerrainTiles())
// e escolhe o tile certo por coordenada a cada consulta. A vista de terreno
// do Mapa passa a cobrir qualquer area pra qual haja tile no diretorio, nao
// so o 1x1 grau que o cenario referencia pro proprio anti-CFIT nativo
// (que continua usando SO o tile do EDL, sem mudanca nenhuma aqui).
//------------------------------------------------------------------------------
namespace app {

// false = sem dado de terreno naquele ponto (fora de todos os tiles
// carregados) OU nenhum tile carregado -- ver a armadilha ja documentada no
// CLAUDE.md ("Player::updateElevation() ignora o retorno de
// getElevation()"): AQUI o retorno E' respeitado, porque desenhar
// "elevacao 0" fora de todo tile seria ativamente enganoso na vista de
// terreno (diferente do piso absoluto que o MODELO usa de proposito como
// rede de seguranca).
using TerrainSampler = std::function<bool(double northM, double eastM, double& elevM)>;

// 'worldModel' e usado SO pra converter N/E -> lat/lon (getRefLatitude()/
// getRefLongitude()) -- o repositorio de tiles em si e carregado uma unica
// vez (ver tileRepository() em TerrainQuery.cpp, cache 'static' local),
// nao depende do cenario carregado. Criar o SAMPLER (a lambda) de novo a
// cada redesenho e barato (so os dois doubles + um ponteiro pro cache sao
// capturados) -- o trabalho caro (ler os arquivos) roda uma vez so.
// Devolve um std::function vazio (contextualmente "falso" -- ver
// std::function::operator bool) se nao houver NENHUM tile valido no
// diretorio.
TerrainSampler makeTerrainSampler(mixr::models::WorldModel* worldModel);

} // namespace app
