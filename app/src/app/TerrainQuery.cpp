#include "app/TerrainQuery.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/terrain/Terrain.hpp"
#include "mixr/terrain/srtm/SrtmHgtFile.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/osg/Vec3d"
#include "mixr/base/util/nav_utils.hpp"

#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace app {

namespace {

// Mesmo diretorio que main.cpp usa pra 'ensureAllTerrainTiles()' (ver
// app/TerrainData.hpp) -- literal repetido de proposito (mesmo raciocinio
// de 'terrainDir'/'terrainTile' ja duplicados em cada main.cpp das quatro
// pocs deste repositorio: copia pequena, nao vale uma abstracao pra
// atravessar so essa fronteira).
const char* const kTerrainDir{"./shared/data/terrain/srtm/"};

// SrtmHgtFile marca "sem dado" (void, buraco no tile) com um valor de
// SENTINELA em torno de -32767/-32768 -- e DataFile::getElevation() (lido
// em contexts/src/mixr/src/terrain/DataFile.cpp antes de escrever isto)
// devolve esse valor como se fosse uma elevacao REAL (nao filtra void
// nenhum, so retorna 'false' se o ponto cair FORA da caixa do tile). Sem
// este filtro, um void vira "elevacao -32768 m" na vista Lateral -- a
// linha de contorno despencava pro fundo do canvas (o pedido que motivou
// isto: "hoje esta algo em torno de -32k, nao faz sentido"). Qualquer
// ponto da Terra de verdade fica acima disto por larga margem (o mais
// fundo, o Mar Morto, e so uns -430 m) -- tratado como void e descartado
// (a mesma semantica de "false" que ja usamos pra "fora de todo tile").
const double kMinPlausibleElevM{-1000.0};

// Um tile carregado (qualquer resolucao/origem -- SrtmHgtFile aceita
// SRTM1 e SRTM3, ver a validacao de tamanho em bytes no proprio arquivo)
// mais a caixa 1x1 grau que ele cobre, pelo nome (canto SW).
//
// 'terrain' e um ponteiro CRU de proposito, nao base::safe_ptr -- este
// repositorio e um cache 'static' de vida INTEIRA do processo (carregado
// uma vez, nunca esvaziado ate o programa sair), e safe_ptr<T> nao e
// copiavel/movivel do jeito que std::vector precisa pra crescer (o
// construtor de copia pede 'safe_ptr<T>&' nao-const, sem construtor de
// mover -- nao entra num vector sem gambiarra). Como este objeto nunca e
// destruido em vida de processo, o refcount inicial de 'new' (1) e o
// suficiente: nao ha unref() de volta, e o SO recupera tudo na saida.
struct LoadedTile
{
   double swLat{};
   double swLon{};
   mixr::terrain::Terrain* terrain{};
};

// Extrai o canto SW do NOME do arquivo -- mesma convencao que
// SrtmHgtFile::determineSrtmInfo() usa por dentro (ultimos 11 caracteres,
// "[NS]DD[EW]DDD.hgt"), repetida aqui so pra ESCOLHER qual tile cobre um
// ponto, nao pra ler o arquivo (isso quem faz e o proprio SrtmHgtFile).
bool parseSwCorner(const std::string& filename, double& swLat, double& swLon)
{
   if (filename.size() < 11) return false;
   const std::string tag{filename.substr(filename.size() - 11, 7)};
   const char ns{static_cast<char>(std::toupper(static_cast<unsigned char>(tag[0])))};
   const char ew{static_cast<char>(std::toupper(static_cast<unsigned char>(tag[3])))};
   if ((ns != 'N' && ns != 'S') || (ew != 'E' && ew != 'W')) return false;
   if (!std::isdigit(static_cast<unsigned char>(tag[1])) ||
       !std::isdigit(static_cast<unsigned char>(tag[2])) ||
       !std::isdigit(static_cast<unsigned char>(tag[4])) ||
       !std::isdigit(static_cast<unsigned char>(tag[5])) ||
       !std::isdigit(static_cast<unsigned char>(tag[6]))) {
      return false;
   }
   const int latDeg{std::stoi(tag.substr(1, 2))};
   const int lonDeg{std::stoi(tag.substr(4, 3))};
   swLat = (ns == 'S') ? -latDeg : latDeg;
   swLon = (ew == 'W') ? -lonDeg : lonDeg;
   return true;
}

// Carrega TODOS os '.hgt' validos de 'kTerrainDir' -- nao so o tile que um
// cenario declarou em EDL (ver o comentario grande de TerrainSampler, no
// header). 'static' local: escaneado e carregado UMA vez (arquivos de ate
// 25 MB cada; refazer isso por redesenho seria caro) -- ver o comentario
// de 'makeTerrainSampler()' abaixo sobre por que isso e seguro aqui (unica
// thread que chama esta funcao e a de UI do FTXUI).
const std::vector<LoadedTile>& tileRepository()
{
   static const std::vector<LoadedTile> tiles = [] {
      std::vector<LoadedTile> loaded;
      std::error_code ec;
      if (!std::filesystem::exists(kTerrainDir, ec)) return loaded;

      for (const auto& entry : std::filesystem::directory_iterator(kTerrainDir, ec)) {
         if (ec) break;
         if (!entry.is_regular_file()) continue;
         const std::string name{entry.path().filename().string()};
         if (name.size() < 4 || name.substr(name.size() - 4) != ".hgt") continue;   // pula .hgt.gz

         double swLat{};
         double swLon{};
         if (!parseSwCorner(name, swLat, swLon)) continue;

         auto* const tile{new mixr::terrain::SrtmHgtFile()};
         auto* const pathStr{new mixr::base::String(kTerrainDir)};
         auto* const fileStr{new mixr::base::String(name.c_str())};
         tile->setPathname(pathStr);
         tile->setFilename(fileStr);
         pathStr->unref();
         fileStr->unref();
         tile->reset();   // Terrain::reset() chama loadData() se ainda nao carregado

         if (!tile->isDataLoaded()) {
            tile->unref();
            continue;
         }

         LoadedTile lt;
         lt.swLat = swLat;
         lt.swLon = swLon;
         lt.terrain = tile;
         loaded.push_back(std::move(lt));
      }
      return loaded;
   }();
   return tiles;
}

}   // namespace

TerrainSampler makeTerrainSampler(mixr::models::WorldModel* const worldModel)
{
   if (worldModel == nullptr) return {};

   const std::vector<LoadedTile>* const tiles{&tileRepository()};
   if (tiles->empty()) return {};

   const double refLat{worldModel->getRefLatitude()};
   const double refLon{worldModel->getRefLongitude()};

   return [tiles, refLat, refLon](const double northM, const double eastM,
                                  double& elevM) -> bool {
      double lat{};
      double lon{};
      double alt{};
      const mixr::base::Vec3d pos{northM, eastM, 0.0};
      if (!mixr::base::nav::convertPosVec2llE(refLat, refLon, pos, &lat, &lon, &alt)) {
         return false;
      }
      for (const auto& tile : *tiles) {
         if (lat < tile.swLat || lat >= tile.swLat + 1.0) continue;
         if (lon < tile.swLon || lon >= tile.swLon + 1.0) continue;
         double elev{};
         if (tile.terrain->getElevation(&elev, lat, lon, true) && elev >= kMinPlausibleElevM) {
            elevM = elev;
            return true;
         }
      }
      return false;
   };
}

} // namespace app
