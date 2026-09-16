#ifndef __xterrain_MultiTileTerrain_H__
#define __xterrain_MultiTileTerrain_H__

#include "mixr/terrain/Terrain.hpp"

#include <cstddef>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace mixr {
namespace base { class String; }
namespace xterrain {

//------------------------------------------------------------------------------
// Class: MultiTileTerrain
//
// Description: banco de elevacao que serve VARIOS tiles SRTM (.hgt/.hgt.gz,
//              um por grau de lat/lon) a partir de um UNICO diretorio, com
//              carga sob demanda e um cache LRU de tiles residentes --
//              alternativa ao ( SrtmHgtFile ) nativo (que so serve UM tile)
//              para cenarios cuja area de voo excede 1x1 grau (ex.: voo
//              livre por qualquer parte do Brasil, com scripts/fetch_srtm.sh
//              --brasil chegando a ~1600 tiles em disco).
//
// getElevation(lat, lon) resolve, a cada chamada, a CELULA (floor(lat),
// floor(lon)) que cobre o ponto e delega para um SrtmHgtFile interno daquela
// celula -- carregado (e descomprimido, se preciso) na primeira consulta,
// mantido residente ate o cache exceder kMaxResidentTiles, quando o tile
// menos recentemente usado e descartado (unref()'d). NAO ha atribuicao de
// player a tile: cada chamada e independente, e varios players em regioes
// diferentes simplesmente mantem seus proprios tiles residentes ao mesmo
// tempo, enquanto o numero de tiles DISTINTOS em uso nao excede o teto --
// acima disso, o cache passa a descarregar/recarregar o tile de quem for
// despejado (thrashing de I/O, nunca erro de resultado: o valor devolvido
// depois de recarregar continua correto).
//
// CONCORRENCIA -- por que o mutex e OBRIGATORIO aqui, ao contrario do
// mecanismo irmao em app/TerrainQuery.cpp (que so tem UM chamador, a thread
// de UI do FTXUI): Player::updateElevation() roda em Player::updateData()
// (fase de FUNDO), e mixr::simulation::Simulation::updateData() despacha os
// players por um pool de ATE 'numBgThreads' threads OS reais quando esse
// slot e > 1 (Simulation.cpp) -- e este projeto ja usa numBgThreads: 2 por
// padrao nos cenarios de producao (@NUM_BG_THREADS@). Como todos os players
// compartilham o MESMO WorldModel::terrain, duas (ou mais) threads podem
// chamar getElevation() no MESMO objeto, ao mesmo tempo -- e o framework
// nativo (Terrain/DataFile/SrtmHgtFile) nao serializa nada por conta
// propria (zero mutex nos tres). Por isso 'cacheMutex' e 'mutable' e
// protege TODO acesso ao indice/cache/LRU, mesmo a partir dos metodos
// 'const' herdados de Terrain.
//
// getMinElevation()/getMaxElevation() (herdados de Terrain) NAO sao
// mantidos -- ficam no default (0/0). Nao sao virtuais (so devolvem campos
// PRIVADOS da propria base, sem override possivel) e nada na fisica/dominio
// deste projeto os consulta; acumular um min/max GLOBAL a cada tile que
// entra/sai do LRU teria custo sem beneficio aqui.
//
// getElevations() (plural, usado so por Terrain::targetOcculting() --
// mascaramento de radar por terreno, nao exercitado por nenhum cenario
// deste repositorio, confirmado por busca) e implementado de forma correta
// mas simples: delega para getElevation() ponto a ponto, com a MESMA
// formula de passo que DataFile::getElevations() usa -- sem otimizar
// leitura em lote por tile.
//
// Factory name: MultiTileTerrain
// Slots:
//    dir   <String>   ! Diretorio com os tiles ("./shared/data/terrain/srtm/").
//                      ! Sem este slot (ou com um diretorio inexistente),
//                      ! getElevation() sempre devolve false -- nunca aborta
//                      ! o processo (ao contrario de app::ensureTerrainData(),
//                      ! que e' so para o tile OBRIGATORIO de um cenario de
//                      ! teste hermetico).
//------------------------------------------------------------------------------
class MultiTileTerrain final : public terrain::Terrain
{
   DECLARE_SUBCLASS(MultiTileTerrain, terrain::Terrain)

public:
   MultiTileTerrain();

   // Numero de tiles ATUALMENTE residentes (carregados) no cache -- nunca
   // excede kMaxResidentTiles. Publico para diagnostico/teste: a alternativa
   // (so observar efeito colateral externo) nao distinguiria "despejado e
   // recarregado" de "nunca tocado".
   std::size_t residentTileCount() const;

   // Numero de tiles DISTINTOS que o indice do diretorio encontrou --
   // independente de estarem residentes ou nao.
   std::size_t indexedTileCount() const;

   // Teto de tiles residentes simultaneos (~310 MB em SRTM1, 12 x ~26 MB).
   // Publico de proposito: e o parametro que precisa ser dimensionado pelo
   // padrao de uso real do cenario (quantos tiles DISTINTOS os players dele
   // visitam ao mesmo tempo) -- ver o cabecalho desta classe.
   static constexpr std::size_t kMaxResidentTiles{12};

   bool isDataLoaded() const override;

   unsigned int getElevations(
         double* const elevations,
         bool* const validFlags,
         const unsigned int n,
         const double lat,
         const double lon,
         const double direction,
         const double maxRng,
         const bool interp = false
      ) const override;

   bool getElevation(
         double* const elev,
         const double lat,
         const double lon,
         const bool interp = false
      ) const override;

private:
   bool loadData() override;

   // Canto SW da celula (graus inteiros -- floor(lat), floor(lon)).
   using Cell = std::pair<int, int>;

   struct TileEntry {
      std::string hgtName;             // "S23W043.hgt", sem caminho
      bool hasGz{};                    // existe "<hgtName>.gz"?
      terrain::Terrain* tile{nullptr}; // nulo = ainda nao carregado
      bool loadFailed{false};          // ja tentou e falhou; nao repetir
   };

   // PRE-CONDICAO nos cinco: 'cacheMutex' ja travado pelo chamador.
   void ensureIndexBuiltLocked() const;
   terrain::Terrain* residentTileLocked(const Cell& cell) const;
   void touchLruLocked(const Cell& cell) const;
   void evictIfNeededLocked() const;
   void releaseAllLocked() const;

   std::string dir;

   mutable std::mutex cacheMutex;
   mutable std::map<Cell, TileEntry> index;
   mutable std::list<Cell> lru;   // frente = mais recentemente usado
   mutable bool indexBuilt{};     // 'mutable': getElevation() const monta o
                                  // indice sob demanda se ainda nao existir
                                  // (ver ensureIndexBuiltLocked()) -- nao
                                  // depende de reset() ter sido chamado
                                  // antes, o que corrigiria um clone() nunca
                                  // resetado explicitamente.

private:
   // slot table helper methods
   bool setSlotDir(const base::String* const x);
};

}
}

#endif
