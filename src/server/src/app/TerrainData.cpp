#include "app/TerrainData.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace app {

namespace {

// Os dois unicos tamanhos que o SrtmHgtFile::determineSrtmInfo() reconhece.
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
      std::cerr << "[sim-runner] banco de elevacao ausente: " << gz << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const std::string cmd{"gunzip -kf \"" + gz + "\""};
   std::system(cmd.c_str());

   const std::streamoff size{fileSize(hgt)};
   if (!isValidSrtmSize(size)) {
      std::cerr << "[sim-runner] " << hgt << " tem " << size << " bytes; o SrtmHgtFile so aceita "
                << SRTM3_BYTES << " (SRTM3) ou " << SRTM1_BYTES << " (SRTM1)" << std::endl;
      std::exit(EXIT_FAILURE);
   }
}

} // namespace app
