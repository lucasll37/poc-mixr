#include "app/EdlHighlightRender.hpp"

#include <gtest/gtest.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <string>

// app/EdlHighlightRender.cpp -- a parte COM FTXUI do destaque de sintaxe da
// aba "EDL" (F7). Cobre em especial o bug de UTF-8 achado por auditoria:
// 'renderEdlLine()' cortava o glifo sob o cursor com 'substr(local, 1)' --
// sempre 1 BYTE -- fazendo qualquer caractere acentuado (comum em comentario
// pt-BR, a convencao deste repositorio) sumir da tela quando o cursor caia
// sobre ele. Renderiza de verdade (Screen::Create + Render + ToString(), o
// mesmo padrao de app/tests/test_memory_panel.cpp) para provar que o glifo
// sobrevive no PIXEL final, nao so' que o codigo nao lanca excecao.

namespace {

std::string renderToText(const ftxui::Element& element)
{
   auto screen{ftxui::Screen::Create(ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(10))};
   ftxui::Render(screen, element);
   return screen.ToString();
}

} // namespace

//------------------------------------------------------------------------------
// utf8GlyphBytes() -- a largura em bytes do glifo UTF-8 que comeca em 'pos'.
//------------------------------------------------------------------------------
TEST(Utf8GlyphBytes, AsciiEUmByte)
{
   EXPECT_EQ(app::utf8GlyphBytes("abc", 0), 1u);
}

TEST(Utf8GlyphBytes, DoisBytesParaLetraAcentuadaLatina)
{
   // 'a' + 'a-til' (U+00E3, 0xC3 0xA3) + 'b'.
   const std::string s{"a\xC3\xA3" "b"};
   EXPECT_EQ(app::utf8GlyphBytes(s, 1), 2u); // posicao do lead byte 0xC3
}

TEST(Utf8GlyphBytes, TresBytesParaCaractereDoBmp)
{
   // U+2764 (coracao pesado), 0xE2 0x9D 0xA4.
   const std::string s{"x\xE2\x9D\xA4" "y"};
   EXPECT_EQ(app::utf8GlyphBytes(s, 1), 3u);
}

TEST(Utf8GlyphBytes, QuatroBytesParaCaractereSuplementar)
{
   // U+1F600 (emoji), 0xF0 0x9F 0x98 0x80.
   const std::string s{"\xF0\x9F\x98\x80"};
   EXPECT_EQ(app::utf8GlyphBytes(s, 0), 4u);
}

TEST(Utf8GlyphBytes, ContinuationByteIsoladoDegradaParaUmByte)
{
   // 0xA3 sozinho (o segundo byte de 'a-til', sem o lead byte antes) nao e'
   // um lead byte valido -- nenhuma das faixas bate, cai no fallback.
   const std::string s{"\xA3"};
   EXPECT_EQ(app::utf8GlyphBytes(s, 0), 1u);
}

TEST(Utf8GlyphBytes, SequenciaTruncadaNoFimDaStringDegradaParaUmByte)
{
   // Lead byte de 2 bytes (0xC3) mas SEM o segundo byte -- string termina
   // ali. Mesmo fallback que ftxui::EatCodePoint() usa (*end = start+1,
   // return false) quando a sequencia nao cabe inteira na string.
   const std::string s{"x\xC3"};
   EXPECT_EQ(app::utf8GlyphBytes(s, 1), 1u);
}

TEST(Utf8GlyphBytes, PosicaoNoFimDaStringDevolveUm)
{
   const std::string s{"abc"};
   EXPECT_EQ(app::utf8GlyphBytes(s, 3), 1u);
}

//------------------------------------------------------------------------------
// renderEdlLine()/renderHighlightedEdlText() -- o cursor sobre um glifo
// multibyte precisa preservar o glifo inteiro na tela renderizada.
//------------------------------------------------------------------------------
TEST(RenderHighlightedEdlText, CursorSobreCaractereAcentuadoPreservaOGlifoNaTela)
{
   // "// n" (4 bytes, indices 0..3) + 'a-til' (2 bytes, indices 4..5) +
   // "o redescobrir". O cursor (byte 4) cai EXATAMENTE no lead byte do
   // glifo acentuado -- o caso que a auditoria reproduziu.
   const std::string line{"// n\xC3\xA3" "o redescobrir"};
   const ftxui::Element el{app::renderHighlightedEdlText(line, /*cursorPosition=*/4,
                                                          /*focused=*/true, /*hovered=*/false)};
   const std::string rendered{renderToText(el)};
   EXPECT_NE(rendered.find("n\xC3\xA3o redescobrir"), std::string::npos)
      << "glifo acentuado sumiu da tela com o cursor sobre ele:\n"
      << rendered;
}

TEST(RenderHighlightedEdlText, CursorAntesDoCaractereAcentuadoPreservaOGlifo)
{
   const std::string line{"// n\xC3\xA3" "o redescobrir"};
   const ftxui::Element el{app::renderHighlightedEdlText(line, /*cursorPosition=*/3,
                                                          /*focused=*/true, /*hovered=*/false)};
   const std::string rendered{renderToText(el)};
   EXPECT_NE(rendered.find("n\xC3\xA3o redescobrir"), std::string::npos)
      << "glifo acentuado sumiu da tela com o cursor no byte anterior:\n"
      << rendered;
}

TEST(RenderHighlightedEdlText, CursorSobreCaractereAsciiContinuaFuncionando)
{
   const std::string line{"// simples"};
   const ftxui::Element el{app::renderHighlightedEdlText(line, /*cursorPosition=*/3,
                                                          /*focused=*/true, /*hovered=*/false)};
   const std::string rendered{renderToText(el)};
   EXPECT_NE(rendered.find("simples"), std::string::npos) << rendered;
}

TEST(RenderHighlightedEdlText, CursorNoFimDaLinhaNaoEstoura)
{
   const std::string line{"// n\xC3\xA3o"};
   const ftxui::Element el{app::renderHighlightedEdlText(
      line, /*cursorPosition=*/static_cast<int>(line.size()), /*focused=*/true, /*hovered=*/false)};
   EXPECT_NE(renderToText(el).find("n\xC3\xA3o"), std::string::npos);
}

TEST(RenderHighlightedEdlText, LinhaVaziaNaoEstoura)
{
   const ftxui::Element el{app::renderHighlightedEdlText("", 0, true, false)};
   EXPECT_NO_THROW(renderToText(el));
}

TEST(RenderHighlightedEdlText, MultiplasLinhasComGlifoAcentuadoNaSegunda)
{
   const std::string text{"( ClockStation\n   // n\xC3\xA3o redescobrir\n)"};
   const auto cursorPos{static_cast<int>(text.find("\xC3\xA3"))};
   const ftxui::Element el{app::renderHighlightedEdlText(text, cursorPos, true, false)};
   EXPECT_NE(renderToText(el).find("n\xC3\xA3o redescobrir"), std::string::npos);
}
