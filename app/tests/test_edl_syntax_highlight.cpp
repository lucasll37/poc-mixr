#include "app/EdlSyntaxHighlight.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

// Espelha, ponto a ponto, a bateria de src/ui/edl_builder.test.js sobre
// tokenizeEdlText() -- ver o cabecalho de app/EdlSyntaxHighlight.hpp para o
// porque de existir uma segunda implementacao (C++, sem FTXUI) da MESMA
// gramatica.

namespace {

using app::EdlToken;
using app::EdlTokenKind;

std::string joinTokens(const std::vector<EdlToken>& tokens)
{
   std::string out;
   for (const auto& t : tokens) out += t.text;
   return out;
}

std::optional<EdlTokenKind> kindOf(const std::vector<EdlToken>& tokens, const std::string& text)
{
   for (const auto& t : tokens) {
      if (t.text == text) return t.kind;
   }
   return std::nullopt;
}

} // namespace

TEST(EdlSyntaxHighlight, RoundTripReproduzOTextoOriginal)
{
   const std::string sample{
      "( ClockStation\n   tcRate: ( Hertz 50 )\n   // comentario\n   "
      "ownship: \"bandit1\"\n   ativo: true\n   cor: <ff00ff>\n   "
      "ranges: [ 1 2 3 ]\n) // ClockStation\n"};
   EXPECT_EQ(joinTokens(app::tokenizeEdlText(sample)), sample);
}

TEST(EdlSyntaxHighlight, RoundTripPreservaEspacoEQuebraDeLinhaEmTextoQualquer)
{
   const std::string sample{"  ( A\n\tb: 1\n  )\n// fim sem quebra"};
   EXPECT_EQ(joinTokens(app::tokenizeEdlText(sample)), sample);
}

TEST(EdlSyntaxHighlight, NomeDeClasseLogoAposAbreParentesesViraClassName)
{
   const auto tokens{app::tokenizeEdlText("( Aircraft type: C310 )")};
   EXPECT_EQ(kindOf(tokens, "Aircraft"), EdlTokenKind::ClassName);
}

TEST(EdlSyntaxHighlight, NomeDeSlotLogoAntesDeDoisPontosViraSlotName)
{
   const auto tokens{app::tokenizeEdlText("( Aircraft type: C310 )")};
   EXPECT_EQ(kindOf(tokens, "type"), EdlTokenKind::SlotName);
}

TEST(EdlSyntaxHighlight, ComentarioViraCommentAteFimDaLinha)
{
   const auto tokens{app::tokenizeEdlText("( Aircraft ) // fim\n")};
   EXPECT_EQ(kindOf(tokens, "// fim"), EdlTokenKind::Comment);
}

TEST(EdlSyntaxHighlight, StringEntreAspasViraString)
{
   const auto tokens{app::tokenizeEdlText("nome: \"falcon 1\"")};
   EXPECT_EQ(kindOf(tokens, "\"falcon 1\""), EdlTokenKind::String);
}

TEST(EdlSyntaxHighlight, StringEntreAngulosTambemViraString)
{
   const auto tokens{app::tokenizeEdlText("cor: <ff00ff>")};
   EXPECT_EQ(kindOf(tokens, "<ff00ff>"), EdlTokenKind::String);
}

TEST(EdlSyntaxHighlight, AnguloSemFechamentoNaoViraStringMasContinuaTokenizando)
{
   // Sem '>' na mesma linha o lookahead da string-angulo falha (mesma regra
   // do regex original) -- "<foo" cai na regra generica de identificador
   // ('<' tambem esta' em EDL_IDENT_CHARS), e o resto da linha continua
   // sendo tokenizado normalmente.
   const auto tokens{app::tokenizeEdlText("a: <foo\nb: 1")};
   EXPECT_EQ(kindOf(tokens, "<foo"), EdlTokenKind::Value);
   EXPECT_EQ(kindOf(tokens, "b"), EdlTokenKind::SlotName);
   EXPECT_EQ(kindOf(tokens, "1"), EdlTokenKind::Num);
}

TEST(EdlSyntaxHighlight, TrueFalseSoMinusculoOuMaiusculoViraBool)
{
   const auto tokens{app::tokenizeEdlText("a: true b: FALSE")};
   EXPECT_EQ(kindOf(tokens, "true"), EdlTokenKind::Bool);
   EXPECT_EQ(kindOf(tokens, "FALSE"), EdlTokenKind::Bool);
}

TEST(EdlSyntaxHighlight, CaseMistaNaoEBooleanoViraValue)
{
   const auto tokens{app::tokenizeEdlText("a: True")};
   EXPECT_EQ(kindOf(tokens, "True"), EdlTokenKind::Value);
}

TEST(EdlSyntaxHighlight, SlotNameTemPrioridadeSobreBool)
{
   // Mesma regra do lado JS: a forma "identificador+':'" e' tentada antes
   // da forma "true|TRUE|false|FALSE" -- um SLOT chamado "true" continua
   // slot, nunca o literal booleano.
   const auto tokens{app::tokenizeEdlText("true: false")};
   EXPECT_EQ(kindOf(tokens, "true"), EdlTokenKind::SlotName);
   EXPECT_EQ(kindOf(tokens, "false"), EdlTokenKind::Bool);
}

TEST(EdlSyntaxHighlight, NumeroFloatEInteiroViramNum)
{
   const auto tokens{app::tokenizeEdlText("x: 1750.5 y: 42")};
   EXPECT_EQ(kindOf(tokens, "1750.5"), EdlTokenKind::Num);
   EXPECT_EQ(kindOf(tokens, "42"), EdlTokenKind::Num);
}

TEST(EdlSyntaxHighlight, FloatSemDigitoInteiroENotacaoCientificaTambemViramNum)
{
   const auto tokens{app::tokenizeEdlText("a: .5 b: 5e10 c: -3.")};
   EXPECT_EQ(kindOf(tokens, ".5"), EdlTokenKind::Num);
   EXPECT_EQ(kindOf(tokens, "5e10"), EdlTokenKind::Num);
   EXPECT_EQ(kindOf(tokens, "-3."), EdlTokenKind::Num);
}

TEST(EdlSyntaxHighlight, PontuacaoDeBlocoEListaViraPunct)
{
   const auto tokens{app::tokenizeEdlText("components: { 1: ( A ) } ranges: [ 1 2 ]")};
   EXPECT_EQ(kindOf(tokens, "{"), EdlTokenKind::Punct);
   EXPECT_EQ(kindOf(tokens, "}"), EdlTokenKind::Punct);
   EXPECT_EQ(kindOf(tokens, "["), EdlTokenKind::Punct);
   EXPECT_EQ(kindOf(tokens, "]"), EdlTokenKind::Punct);
}

TEST(EdlSyntaxHighlight, IdentificadorSoltoViraValue)
{
   const auto tokens{app::tokenizeEdlText("alvo: falcon1")};
   EXPECT_EQ(kindOf(tokens, "falcon1"), EdlTokenKind::Value);
}

TEST(EdlSyntaxHighlight, StringComEscapeDeAspaContinuaUmUnicoToken)
{
   const auto tokens{app::tokenizeEdlText(R"(nome: "a \"b\" c")")};
   EXPECT_EQ(kindOf(tokens, R"("a \"b\" c")"), EdlTokenKind::String);
}
