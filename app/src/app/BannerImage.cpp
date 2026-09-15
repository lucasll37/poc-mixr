#include "app/BannerImage.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>

// So os dois formatos que app/assets/banner.* de fato usa -- reduz o
// decodificador compilado (sem BMP/GIF/PSD/TGA/HDR/PIC/PNM) ao escopo
// pedido. Unica translation unit deste repositorio que define
// STB_IMAGE_IMPLEMENTATION -- ver app/thirdparty/README.md; sem ela o
// header vira so a declaracao.
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace app {

std::vector<BannerCell> resampleBannerForTerminal(const BannerPixels& src, const int cellsWide, const int cellsTall)
{
   if (src.width <= 0 || src.height <= 0 || cellsWide <= 0 || cellsTall <= 0) return {};

   const int targetW{cellsWide};
   const int targetH{cellsTall * 2};   // 2 subpixels verticais por celula (meio-bloco)

   // "cover": a maior das duas razoes garante que a imagem escalada cubra o
   // alvo inteiro nos dois eixos; o excesso no eixo que sobra e' cortado
   // (offsetX/offsetY), nunca distorcido.
   const double scale{std::max(static_cast<double>(targetW) / src.width,
                               static_cast<double>(targetH) / src.height)};
   const double offsetX{(src.width * scale - targetW) / 2.0};
   const double offsetY{(src.height * scale - targetH) / 2.0};

   const auto sampleAt = [&](const int tx, const int ty) -> BannerRgb {
      int sx{static_cast<int>((tx + offsetX) / scale)};
      int sy{static_cast<int>((ty + offsetY) / scale)};
      sx = std::clamp(sx, 0, src.width - 1);
      sy = std::clamp(sy, 0, src.height - 1);
      return src.pixels[static_cast<std::size_t>(sy) * static_cast<std::size_t>(src.width) + static_cast<std::size_t>(sx)];
   };

   std::vector<BannerCell> out;
   out.reserve(static_cast<std::size_t>(cellsWide) * static_cast<std::size_t>(cellsTall));
   for (int cy = 0; cy < cellsTall; ++cy) {
      for (int cx = 0; cx < cellsWide; ++cx) {
         out.push_back(BannerCell{sampleAt(cx, cy * 2), sampleAt(cx, cy * 2 + 1)});
      }
   }
   return out;
}

BannerPixels decodeBannerFile(const std::string& path)
{
   if (path.empty()) return {};

   int w{};
   int h{};
   int channelsInFile{};
   // O '3' final forca saida RGB8 sempre, mesmo com fonte em escala de
   // cinza/RGBA/CMYK -- um so caminho de leitura abaixo.
   unsigned char* data{stbi_load(path.c_str(), &w, &h, &channelsInFile, 3)};
   if (data == nullptr || w <= 0 || h <= 0) return {};

   BannerPixels img;
   img.width = w;
   img.height = h;
   img.pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
   for (std::size_t i = 0; i < img.pixels.size(); ++i) {
      const std::size_t base{i * 3};
      img.pixels[i] = BannerRgb{data[base], data[base + 1], data[base + 2]};
   }
   stbi_image_free(data);
   return img;
}

std::string firstExistingPath(const std::vector<std::string>& candidatos)
{
   for (const std::string& caminho : candidatos) {
      std::error_code ec;
      if (std::filesystem::exists(caminho, ec) && !ec) return caminho;
   }
   return {};
}

std::string findBannerAssetPath()
{
   return firstExistingPath({
      "app/assets/banner.png",
      "app/assets/banner.jpg",
      "app/assets/banner.jpeg",
   });
}

BannerPixels loadBannerImage()
{
   return decodeBannerFile(findBannerAssetPath());
}

} // namespace app
