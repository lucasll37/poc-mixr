//
// app/BannerImage.hpp -- a amostragem PURA por tras do fundo da tela de
// selecao de cenario (app/ScenarioPickerScreen.cpp): reamostragem "cover"
// de pixels pra celulas de terminal, e a busca do arquivo em disco. Sem
// FTXUI, sem decodificar imagem nenhuma de verdade (isso fica a cargo de
// stb_image, tratado como dependencia confiavel -- so a degradacao
// graciosa quando o arquivo nao existe e' testada aqui).
//
#include "app/BannerImage.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>

using app::BannerCell;
using app::BannerPixels;
using app::BannerRgb;

namespace {

BannerPixels solidImage(const int w, const int h, const BannerRgb color)
{
   BannerPixels img;
   img.width = w;
   img.height = h;
   img.pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), color);
   return img;
}

// Imagem 2x2 com um pixel diferente em cada quadrante -- o suficiente pra
// distinguir "veio do canto certo" de "veio de qualquer lugar".
BannerPixels fourQuadrantImage()
{
   BannerPixels img;
   img.width = 2;
   img.height = 2;
   img.pixels = {
      BannerRgb{255, 0, 0}, BannerRgb{0, 255, 0},     // topo: vermelho | verde
      BannerRgb{0, 0, 255}, BannerRgb{255, 255, 0},   // base: azul | amarelo
   };
   return img;
}

} // namespace

TEST(BannerImageResample, ImagemVaziaDevolveVazio)
{
   EXPECT_TRUE(app::resampleBannerForTerminal(BannerPixels{}, 10, 10).empty());
}

TEST(BannerImageResample, DimensaoNaoPositivaDevolveVazio)
{
   const BannerPixels img{solidImage(4, 4, BannerRgb{10, 20, 30})};
   EXPECT_TRUE(app::resampleBannerForTerminal(img, 0, 10).empty());
   EXPECT_TRUE(app::resampleBannerForTerminal(img, 10, 0).empty());
   EXPECT_TRUE(app::resampleBannerForTerminal(img, -1, 10).empty());
}

TEST(BannerImageResample, ContagemDeCelulasBateComLarguraVezesAltura)
{
   const BannerPixels img{solidImage(8, 8, BannerRgb{1, 2, 3})};
   const auto cells{app::resampleBannerForTerminal(img, 5, 3)};
   EXPECT_EQ(cells.size(), 5u * 3u);
}

TEST(BannerImageResample, ImagemDeCorSolidaProduzTodasAsCelulasNaMesmaCor)
{
   const BannerRgb cor{42, 84, 126};
   const BannerPixels img{solidImage(3, 3, cor)};
   const auto cells{app::resampleBannerForTerminal(img, 6, 4)};
   ASSERT_EQ(cells.size(), 24u);
   for (const BannerCell& cell : cells) {
      EXPECT_EQ(cell.top.r, cor.r);
      EXPECT_EQ(cell.top.g, cor.g);
      EXPECT_EQ(cell.top.b, cor.b);
      EXPECT_EQ(cell.bottom.r, cor.r);
      EXPECT_EQ(cell.bottom.g, cor.g);
      EXPECT_EQ(cell.bottom.b, cor.b);
   }
}

// "cover" preserva proporcao -- pedir uma grade com a MESMA proporcao da
// fonte (2x2 pixels -> 1 celula de largura, 1 de altura => 1 subpixel de
// largura, 2 de altura, ou seja largura:altura de subpixel = 1:2, a mesma
// razao da fonte) nao deveria cortar nada: o pixel de cada quadrante
// aparece no subpixel correspondente.
TEST(BannerImageResample, CoverSemCorteMantemOsQuatroCantos)
{
   const BannerPixels img{fourQuadrantImage()};
   // 1 celula de largura, 1 de altura -> 1 subpixel largura x 2 subpixels
   // altura -- ainda nao cobre os dois quadrantes de largura da fonte (2
   // px) numa unica coluna de subpixel, entao usamos 2 celulas de largura
   // (2 subpixels W x 2 subpixels H, exatamente a resolucao da fonte).
   const auto cells{app::resampleBannerForTerminal(img, 2, 1)};
   ASSERT_EQ(cells.size(), 2u);

   // Celula esquerda: topo = vermelho, base = azul (coluna esquerda da
   // fonte). Celula direita: topo = verde, base = amarelo.
   EXPECT_EQ(cells[0].top.r, 255);
   EXPECT_EQ(cells[0].top.g, 0);
   EXPECT_EQ(cells[0].bottom.b, 255);

   EXPECT_EQ(cells[1].top.g, 255);
   EXPECT_EQ(cells[1].bottom.r, 255);
   EXPECT_EQ(cells[1].bottom.g, 255);
}

TEST(BannerImageDecode, ArquivoInexistenteDevolveImagemVazia)
{
   const BannerPixels img{app::decodeBannerFile("/caminho/que/nao/existe/banner.png")};
   EXPECT_EQ(img.width, 0);
   EXPECT_TRUE(img.pixels.empty());
}

TEST(BannerImageDecode, CaminhoVazioDevolveImagemVazia)
{
   EXPECT_EQ(app::decodeBannerFile("").width, 0);
}

TEST(BannerImageFindPath, ListaVaziaDevolveVazio)
{
   EXPECT_TRUE(app::firstExistingPath({}).empty());
}

TEST(BannerImageFindPath, NenhumCandidatoExisteDevolveVazio)
{
   EXPECT_TRUE(app::firstExistingPath({"/a/b/c.png", "/d/e/f.jpg"}).empty());
}

TEST(BannerImageFindPath, PrimeiroCandidatoExistenteVence)
{
   const std::filesystem::path tmp{std::filesystem::temp_directory_path()
                                    / "poc-mixr-banner-image-test.png"};
   {
      std::FILE* f{std::fopen(tmp.string().c_str(), "wb")};
      ASSERT_NE(f, nullptr);
      std::fputs("nao precisa ser um PNG de verdade -- so existir", f);
      std::fclose(f);
   }

   const std::string achado{app::firstExistingPath({"/nao/existe.png", tmp.string(), "/tambem/nao.jpg"})};
   EXPECT_EQ(achado, tmp.string());

   std::filesystem::remove(tmp);
}
