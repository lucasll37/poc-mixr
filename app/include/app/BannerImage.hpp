#pragma once

#include <cstdint>
#include <string>
#include <vector>

//------------------------------------------------------------------------------
// A imagem de fundo da tela de selecao de cenario (app/ScenarioPickerScreen.cpp)
// -- decodificada de app/assets/banner.{png,jpg,jpeg} em TEMPO DE EXECUCAO (via
// app/thirdparty/stb_image.h, ver o README daquela pasta), nunca embutida no
// binario em tempo de compilacao: trocar o arquivo em disco e' o suficiente,
// sem rebuild.
//
// Este header fica PURO de proposito (so <cstdint>/<string>/<vector>, nada de
// FTXUI nem de stb_image) -- mesmo padrao ja usado por app/MapGeometry.hpp: a
// amostragem (resampleBannerForTerminal) e' logica pura, testavel com pixels
// sinteticos, sem levantar terminal nenhum. A ponte para ftxui::Color/Element
// fica em app/ScenarioPickerScreen.cpp, unico consumidor.
//------------------------------------------------------------------------------
namespace app {

struct BannerRgb
{
   std::uint8_t r{};
   std::uint8_t g{};
   std::uint8_t b{};
};

// width==0 significa "sem imagem" (arquivo ausente ou decodificacao falhou) --
// o mesmo sentinela de ausencia que TerrainQuery/xjoystick ja usam em outros
// cantos deste app: degrada em silencio, a tela de selecao continua
// funcionando sem fundo nenhum.
struct BannerPixels
{
   int width{};
   int height{};
   std::vector<BannerRgb> pixels;   // width*height, linha a linha, topo -> base
};

// Um par de subpixels verticais por CELULA de terminal -- a tecnica de
// "meio-bloco": um glifo unico ("(U+2580) upper half block") com cor de
// primeiro plano == 'top' e cor de fundo == 'bottom' dobra a resolucao
// vertical sem custo extra de largura.
struct BannerCell
{
   BannerRgb top;
   BannerRgb bottom;
};

// Amostragem PURA (nearest-neighbor, ajuste "cover": preserva a proporcao da
// imagem original, cortando o excesso, nunca distorcendo) de 'src' para uma
// grade de 'cellsWide' x 'cellsTall' CELULAS de terminal. Cada celula ocupa
// DOIS subpixels verticais (cellsTall*2 no total) -- e' o que da' a esta
// grade uma proporcao de subpixel ~1:1 contra uma fonte de terminal tipica
// (~1 celula de largura : 2 de altura), entao "cover" sobre essa grade ja
// sai sem distorcer o desenho original.
//
// Vetor vazio se 'src' estiver vazia ou 'cellsWide'/'cellsTall' nao forem
// positivos -- nunca lanca, o chamador decide o que fazer com "sem imagem".
std::vector<BannerCell> resampleBannerForTerminal(const BannerPixels& src, int cellsWide, int cellsTall);

// Decodifica 'path' (PNG/JPG, qualquer formato que stb_image aceite) em
// memoria. BannerPixels{} (width==0) se o arquivo nao existir ou a
// decodificacao falhar -- nunca lanca.
BannerPixels decodeBannerFile(const std::string& path);

// O primeiro de 'candidatos' que existe em disco, ou "" se nenhum existir.
// Extraido de findBannerAssetPath() pra ficar testavel sem depender do
// diretorio de trabalho do processo (que em teste roda de build/, nao da
// raiz do repositorio -- mesma razao ja registrada em
// app/ScenarioFolder.cpp).
std::string firstExistingPath(const std::vector<std::string>& candidatos);

// Os caminhos DE VERDADE que este app usa -- relativos a raiz do
// repositorio, mesma convencao de todo caminho deste app (ver CLAUDE.md,
// "Todos os binarios leem configs/data por caminho relativo"). "" se
// nenhuma das tres extensoes existir.
std::string findBannerAssetPath();

// Atalho para o caminho comum: decodeBannerFile(findBannerAssetPath()).
// BannerPixels{} se nao houver banner nenhum -- degradacao silenciosa.
BannerPixels loadBannerImage();

} // namespace app
