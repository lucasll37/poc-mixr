#include "app/MemoryPanel.hpp"

#include <gtest/gtest.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <string>

// app/MemoryPanel.cpp -- classRowText()/renderClassRow(), a linha da aba
// "Memoria" por classe amostrada (app/MetaObjectSnapshot.hpp). Puro em
// relacao a MIXR (ClassStat e so struct de dados); a unica dependencia real
// e FTXUI, para renderizar e ler de volta via Screen::ToString().

namespace {

app::ClassStat makeStat()
{
   app::ClassStat s;
   s.factoryName = "FlightAction";
   s.fromPlugin = true;
   s.count = 4;
   s.mc = 12;
   s.tc = 340;
   return s;
}

std::string renderToText(const ftxui::Element& element)
{
   auto screen{ftxui::Screen::Create(ftxui::Dimension::Fixed(160), ftxui::Dimension::Fixed(10))};
   ftxui::Render(screen, element);
   return screen.ToString();
}

} // namespace

//------------------------------------------------------------------------------
// classRowText() -- fallback de texto plano.
//------------------------------------------------------------------------------
TEST(ClassRowText, MostraPrefixoDeOrigemPorPluginOuHost)
{
   app::ClassStat s{makeStat()};
   s.fromPlugin = true;
   EXPECT_NE(app::classRowText(s).find("[plugin]"), std::string::npos);

   s.fromPlugin = false;
   EXPECT_NE(app::classRowText(s).find("[host]"), std::string::npos);
}

TEST(ClassRowText, MostraOsTresContadores)
{
   const std::string text{app::classRowText(makeStat())};
   EXPECT_NE(text.find("FlightAction"), std::string::npos);
   EXPECT_NE(text.find("count=4"), std::string::npos);
   EXPECT_NE(text.find("mc=12"), std::string::npos);
   EXPECT_NE(text.find("tc=340"), std::string::npos);
}

TEST(ClassRowText, RacaTemPrioridadeSobreCrescendo)
{
   app::ClassStat s{makeStat()};
   s.racyCounter = true;
   s.suspectedLeak = true;

   const std::string text{app::classRowText(s)};
   EXPECT_NE(text.find("RACA"), std::string::npos);
   EXPECT_EQ(text.find("CRESCENDO"), std::string::npos);
}

TEST(ClassRowText, CrescendoApareceSemRaca)
{
   app::ClassStat s{makeStat()};
   s.racyCounter = false;
   s.suspectedLeak = true;

   const std::string text{app::classRowText(s)};
   EXPECT_EQ(text.find("RACA"), std::string::npos);
   EXPECT_NE(text.find("CRESCENDO"), std::string::npos);
}

TEST(ClassRowText, SemAlertaNenhumSufixoAparece)
{
   const app::ClassStat s{makeStat()};   // racyCounter e suspectedLeak falsos por default
   const std::string text{app::classRowText(s)};
   EXPECT_EQ(text.find("RACA"), std::string::npos);
   EXPECT_EQ(text.find("CRESCENDO"), std::string::npos);
}

//------------------------------------------------------------------------------
// renderClassRow() -- o mesmo veredito, agora pelo caminho de renderizacao
// de verdade (a barra em Canvas + o texto ao lado).
//------------------------------------------------------------------------------
TEST(RenderClassRow, MostraContagensEOAlertaCorreto)
{
   app::ClassStat s{makeStat()};
   s.suspectedLeak = true;

   const std::string text{renderToText(app::renderClassRow(s, false))};
   EXPECT_NE(text.find("FlightAction"), std::string::npos);
   EXPECT_NE(text.find("count=4"), std::string::npos);
   EXPECT_NE(text.find("pico=12"), std::string::npos);
   EXPECT_NE(text.find("criados=340"), std::string::npos);
   EXPECT_NE(text.find("CRESCENDO"), std::string::npos);
}

TEST(RenderClassRow, SemAlertaNenhumSufixoAparece)
{
   const std::string text{renderToText(app::renderClassRow(makeStat(), false))};
   EXPECT_EQ(text.find("CRESCENDO"), std::string::npos);
   EXPECT_EQ(text.find("RACA"), std::string::npos);
}

TEST(RenderClassRow, RacaApareceQuandoOContadorEhSuspeito)
{
   app::ClassStat s{makeStat()};
   s.racyCounter = true;

   const std::string text{renderToText(app::renderClassRow(s, false))};
   EXPECT_NE(text.find("RACA"), std::string::npos);
}
