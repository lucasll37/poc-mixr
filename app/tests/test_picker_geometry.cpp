//
// app/PickerGeometry.hpp -- o clamp puro por tras da tela generica de
// selecao (app/ScenarioPickerScreen.cpp). Sem isto extraido, a unica forma
// de exercitar a decisao seria simular um screen.Loop() interativo de
// verdade -- ver o cabecalho do header.
//
#include "app/PickerGeometry.hpp"

#include <gtest/gtest.h>

using app::pickergeometry::computeGeometry;
using app::pickergeometry::kChromeLines;
using app::pickergeometry::kMaxMenuLines;
using app::pickergeometry::kMinMenuLines;

TEST(PickerGeometry, PoucosItensUsaOPiso)
{
   const auto g{computeGeometry(1)};
   EXPECT_EQ(g.menuLines, kMinMenuLines);
   EXPECT_EQ(g.totalHeight, kChromeLines + kMinMenuLines);
}

TEST(PickerGeometry, ZeroItensAindaUsaOPiso)
{
   EXPECT_EQ(computeGeometry(0).menuLines, kMinMenuLines);
}

TEST(PickerGeometry, MuitosItensUsaOTeto)
{
   const auto g{computeGeometry(1000)};
   EXPECT_EQ(g.menuLines, kMaxMenuLines);
   EXPECT_EQ(g.totalHeight, kChromeLines + kMaxMenuLines);
}

TEST(PickerGeometry, ContagemDentroDaFaixaUsaOProprioValor)
{
   const auto g{computeGeometry(5)};
   ASSERT_GT(5, kMinMenuLines);
   ASSERT_LT(5, kMaxMenuLines);
   EXPECT_EQ(g.menuLines, 5);
   EXPECT_EQ(g.totalHeight, kChromeLines + 5);
}

TEST(PickerGeometry, NoLimiteDoPisoEDoTetoNaoEstoura)
{
   EXPECT_EQ(computeGeometry(kMinMenuLines).menuLines, kMinMenuLines);
   EXPECT_EQ(computeGeometry(kMaxMenuLines).menuLines, kMaxMenuLines);
}
