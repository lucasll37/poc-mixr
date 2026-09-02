#include "app/TerrainData.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace app {

namespace {

// Os dois unicos tamanhos que o SrtmHgtFile::determineSrtmInfo() reconhece.
// 1201*1201*2 e 3601*3601*2 -- posts de 2 bytes, big-endian.
constexpr std::streamoff SRTM3_BYTES{2884802};    // 3 arc-sec (~90 m)
constexpr std::streamoff SRTM1_BYTES{25934402};   // 1 arc-sec (~30 m)

std::streamoff fileSize(const std::string& path)
{
   std::ifstream in(path, std::ios::binary | std::ios::ate);
   if (!in.good()) return -1;
   return in.tellg();
}

bool isValidSrtmSize(const std::streamoff size)
{
   return (size == SRTM3_BYTES || size == SRTM1_BYTES);
}

} // namespace

void ensureTerrainData(const std::string& dir, const std::string& baseName)
{
   const std::string hgt{dir + baseName + ".hgt"};
   const std::string gz{hgt + ".gz"};

   if (isValidSrtmSize(fileSize(hgt))) return;   // ja descomprimido e integro

   if (fileSize(gz) < 0) {
      std::cerr << "[main] banco de elevacao ausente: " << gz << std::endl;
      std::exit(EXIT_FAILURE);
   }

   std::cout << "[main] descomprimindo " << gz << " ..." << std::endl;
   // -k mantem o .gz (que e o que fica versionado); -f sobrescreve um .hgt
   // truncado de uma execucao interrompida.
   const std::string cmd{"gunzip -kf \"" + gz + "\""};
   std::system(cmd.c_str());

   const std::streamoff size{fileSize(hgt)};
   if (!isValidSrtmSize(size)) {
      std::cerr << "[main] " << hgt << " tem " << size << " bytes; o SrtmHgtFile so aceita "
                << SRTM3_BYTES << " (SRTM3) ou " << SRTM1_BYTES << " (SRTM1)" << std::endl;
      std::exit(EXIT_FAILURE);
   }
}

void ensureAllTerrainTiles(const std::string& dir)
{
   namespace fs = std::filesystem;
   std::error_code ec;
   if (!fs::exists(dir, ec)) return;

   const std::string suffix{".hgt.gz"};
   for (const auto& entry : fs::directory_iterator(dir, ec)) {
      if (ec) break;
      if (!entry.is_regular_file()) continue;

      const std::string name{entry.path().filename().string()};
      if (name.size() <= suffix.size() ||
          name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
         continue;
      }

      const std::string baseName{name.substr(0, name.size() - suffix.size())};
      const std::string hgt{dir + baseName + ".hgt"};
      if (isValidSrtmSize(fileSize(hgt))) continue;   // ja descomprimido e integro

      const std::string gz{dir + name};
      std::cout << "[main] descomprimindo " << gz << " ..." << std::endl;
      const std::string cmd{"gunzip -kf \"" + gz + "\""};
      std::system(cmd.c_str());

      if (!isValidSrtmSize(fileSize(hgt))) {
         std::cerr << "[main] aviso: " << hgt << " nao ficou com tamanho SRTM valido -- "
                   << "ignorado (a vista de terreno so cobre os tiles que carregarem)" << std::endl;
      }
   }
}

} // namespace app
