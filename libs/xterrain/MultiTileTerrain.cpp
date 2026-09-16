#include "xterrain/MultiTileTerrain.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/units/angle_utils.hpp"
#include "mixr/base/units/distance_utils.hpp"
#include "mixr/terrain/srtm/SrtmHgtFile.hpp"

#include "xlog/Log.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace mixr {
namespace xterrain {

IMPLEMENT_SUBCLASS(MultiTileTerrain, "MultiTileTerrain")

BEGIN_SLOTTABLE(MultiTileTerrain)
   "dir",         // 1) Diretorio com os tiles SRTM (.hgt/.hgt.gz)
END_SLOTTABLE(MultiTileTerrain)

BEGIN_SLOT_MAP(MultiTileTerrain)
   ON_SLOT(1, setSlotDir, base::String)
END_SLOT_MAP()

MultiTileTerrain::MultiTileTerrain()
{
   STANDARD_CONSTRUCTOR()
}

void MultiTileTerrain::copyData(const MultiTileTerrain& org, const bool)
{
   BaseClass::copyData(org);

   const std::lock_guard<std::mutex> lock(cacheMutex);
   releaseAllLocked();
   dir = org.dir;
   // O clone nasce com cache/indice VAZIOS -- nao ha necessidade de
   // duplicar tiles residentes, so a configuracao (dir). O indice e
   // remontado sob demanda no proximo loadData()/getElevation().
   indexBuilt = false;
}

void MultiTileTerrain::deleteData()
{
   const std::lock_guard<std::mutex> lock(cacheMutex);
   releaseAllLocked();
}

namespace {

// Mesma convencao de 11 caracteres que SrtmHgtFile::determineSrtmInfo() usa
// por dentro ("[NS]DD[EW]DDD.hgt") -- replicada aqui so para ESCOLHER qual
// tile cobre um ponto, nao para ler o arquivo (isso quem faz e o proprio
// SrtmHgtFile, internamente).
bool parseSwCorner(const std::string& filename, int& swLatDeg, int& swLonDeg)
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
   swLatDeg = (ns == 'S') ? -latDeg : latDeg;
   swLonDeg = (ew == 'W') ? -lonDeg : lonDeg;
   return true;
}

}   // namespace

// Monta o indice do diretorio (so' NOME e canto SW de cada tile -- nenhum
// dado de elevacao e' lido aqui). Idempotente: no-op se ja construido.
//
// E' 'const' (mexendo so' em membros 'mutable') de proposito -- assim tanto
// loadData() (nao-const, chamado uma vez por Terrain::reset(), o caminho
// normal via WorldModel::reset()) quanto getElevation()/getElevations()
// (const, herdados de Terrain) podem garantir o indice sozinhos. Sem isso,
// um MultiTileTerrain que nunca passasse por reset() explicito -- o caso de
// um clone() (copyData() zera 'indexBuilt' de proposito, ver acima) --
// ficaria com getElevation() sempre falhando, mesmo com 'dir' configurado.
void MultiTileTerrain::ensureIndexBuiltLocked() const
{
   if (indexBuilt) return;

   releaseAllLocked();

   std::error_code ec;
   if (dir.empty() || !std::filesystem::exists(dir, ec) || ec) {
      LOG(WARNING) << "[xterrain] diretorio de terreno nao encontrado: '" << dir << "'";
      indexBuilt = true;   // tentativa feita -- getElevation() sempre falha depois
      return;
   }

   for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (ec) break;
      if (!entry.is_regular_file(ec)) continue;

      const std::string name{entry.path().filename().string()};

      // Aceita "<tile>.hgt" e "<tile>.hgt.gz" -- um tile so' comprimido e'
      // valido, descomprimido na primeira consulta que caia dentro dele
      // (ver residentTileLocked()).
      std::string base;
      bool gz{false};
      if (name.size() > 7 && name.compare(name.size() - 7, 7, ".hgt.gz") == 0) {
         base = name.substr(0, name.size() - 3);   // tira ".gz"
         gz = true;
      } else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".hgt") == 0) {
         base = name;
      } else {
         continue;
      }

      int swLatDeg{};
      int swLonDeg{};
      if (!parseSwCorner(base, swLatDeg, swLonDeg)) continue;

      TileEntry& e{index[Cell{swLatDeg, swLonDeg}]};
      e.hgtName = base;
      e.hasGz = e.hasGz || gz;
   }

   LOG(INFO) << "[xterrain] " << index.size() << " tile(s) indexado(s) em '" << dir << "'";
   indexBuilt = true;
}

bool MultiTileTerrain::loadData()
{
   const std::lock_guard<std::mutex> lock(cacheMutex);
   ensureIndexBuiltLocked();
   return !index.empty();
}

bool MultiTileTerrain::isDataLoaded() const
{
   const std::lock_guard<std::mutex> lock(cacheMutex);
   return indexBuilt && !index.empty();
}

std::size_t MultiTileTerrain::residentTileCount() const
{
   const std::lock_guard<std::mutex> lock(cacheMutex);
   std::size_t n{};
   for (const auto& kv : index) {
      if (kv.second.tile != nullptr) ++n;
   }
   return n;
}

std::size_t MultiTileTerrain::indexedTileCount() const
{
   const std::lock_guard<std::mutex> lock(cacheMutex);
   return index.size();
}

void MultiTileTerrain::touchLruLocked(const Cell& cell) const
{
   for (auto it = lru.begin(); it != lru.end(); ++it) {
      if (*it == cell) { lru.erase(it); break; }
   }
   lru.push_front(cell);
}

void MultiTileTerrain::evictIfNeededLocked() const
{
   while (lru.size() > kMaxResidentTiles) {
      const Cell victim{lru.back()};
      lru.pop_back();
      const auto it{index.find(victim)};
      if (it == index.end() || it->second.tile == nullptr) continue;
      it->second.tile->unref();   // unica referencia nossa -- destroi o tile
      it->second.tile = nullptr;
   }
}

void MultiTileTerrain::releaseAllLocked() const
{
   for (auto& kv : index) {
      if (kv.second.tile != nullptr) {
         kv.second.tile->unref();
         kv.second.tile = nullptr;
      }
   }
   index.clear();
   lru.clear();
}

terrain::Terrain* MultiTileTerrain::residentTileLocked(const Cell& cell) const
{
   const auto it{index.find(cell)};
   if (it == index.end()) return nullptr;

   TileEntry& e{it->second};
   if (e.tile != nullptr) { touchLruLocked(cell); return e.tile; }
   if (e.loadFailed) return nullptr;

   const std::string hgtPath{dir + e.hgtName};

   // SrtmHgtFile nao le '.gz' -- descomprime sob demanda, so este tile.
   std::error_code ec;
   if (!std::filesystem::exists(hgtPath, ec)) {
      if (!e.hasGz) { e.loadFailed = true; return nullptr; }
      const std::string cmd{"gunzip -kf \"" + hgtPath + ".gz\""};
      if (std::system(cmd.c_str()) != 0 || !std::filesystem::exists(hgtPath, ec)) {
         LOG(WARNING) << "[xterrain] falha ao descomprimir " << hgtPath << ".gz -- tile ignorado";
         e.loadFailed = true;
         return nullptr;
      }
   }

   auto* const tile{new terrain::SrtmHgtFile()};
   auto* const pathStr{new base::String(dir.c_str())};
   auto* const fileStr{new base::String(e.hgtName.c_str())};
   tile->setPathname(pathStr);
   tile->setFilename(fileStr);
   pathStr->unref();
   fileStr->unref();
   tile->reset();   // Terrain::reset() chama loadData() se ainda nao carregado

   if (!tile->isDataLoaded()) {
      tile->unref();
      e.loadFailed = true;   // tamanho invalido/arquivo truncado: nao insistir
      LOG(WARNING) << "[xterrain] tile invalido (tamanho/formato): " << hgtPath;
      return nullptr;
   }

   e.tile = tile;
   touchLruLocked(cell);
   evictIfNeededLocked();
   return e.tile;
}

bool MultiTileTerrain::getElevation(
      double* const elev, const double lat, const double lon, const bool interp) const
{
   const Cell cell{static_cast<int>(std::floor(lat)), static_cast<int>(std::floor(lon))};

   const std::lock_guard<std::mutex> lock(cacheMutex);
   ensureIndexBuiltLocked();

   terrain::Terrain* const tile{residentTileLocked(cell)};
   if (tile == nullptr) return false;

   return tile->getElevation(elev, lat, lon, interp);
}

// Mesma formula de passo de DataFile::getElevations() (bearing/range ->
// deltaLat/deltaLon, calculados uma vez a partir do ponto de partida), so
// que cada ponto e' resolvido por getElevation() -- ou seja, pode atravessar
// tile no meio do caminho, ao contrario do DataFile original (um array so).
unsigned int MultiTileTerrain::getElevations(
      double* const elevations, bool* const validFlags, const unsigned int n,
      const double lat, const double lon, const double direction,
      const double maxRng, const bool interp) const
{
   unsigned int num{};

   if (elevations == nullptr || validFlags == nullptr || n < 2 ||
       (lat < -89.0 || lat > 89.0) || maxRng <= 0) return num;

   const double deltaPoint{maxRng / (n - 1)};
   const double dirR{direction * base::angle::D2RCC};
   const double deltaNorth{deltaPoint * std::cos(dirR) * base::distance::M2NM};
   const double deltaEast{deltaPoint * std::sin(dirR) * base::distance::M2NM};
   const double deltaLat{deltaNorth / 60.0};
   const double deltaLon{deltaEast / (60.0 * std::cos(lat * base::angle::D2RCC))};

   double curLat{lat};
   double curLon{lon};

   for (unsigned int i = 0; i < n; i++) {
      if (!validFlags[i]) {
         double elev{};
         if (getElevation(&elev, curLat, curLon, interp)) {
            elevations[i] = elev;
            validFlags[i] = true;
            num++;
         }
      }
      curLat += deltaLat;
      curLon += deltaLon;
   }

   return num;
}

bool MultiTileTerrain::setSlotDir(const base::String* const x)
{
   bool ok{};
   if (x != nullptr) {
      const std::lock_guard<std::mutex> lock(cacheMutex);
      dir = x->getString();
      if (!dir.empty() && dir.back() != '/') dir += '/';
      releaseAllLocked();
      indexBuilt = false;   // proximo reset()/getElevation() reconstroi o indice
      ok = true;
   }
   return ok;
}

}
}
