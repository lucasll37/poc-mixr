//
// libs/xrlbridge/FieldRegistry.hpp + Schema.hpp -- o mecanismo GENERICO por
// tras da porta 'schema' dos nos de arvore de models/players/air/A-4 (ver
// bt/ObservationSchema.hpp la). Testado aqui, isolado de qualquer WorldView
// de verdade -- so um struct fake, para provar a MECANICA (ordem, deteccao
// de nome desconhecido, pack) sem depender de nenhum modelo.
//
#include "xrlbridge/FieldRegistry.hpp"
#include "xrlbridge/Schema.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

namespace {

using namespace mixr::xrlbridge;

// Um estado fake, so para exercitar o registro -- 2 floats e 1 bool,
// suficiente para provar ordem/selecao sem arrastar nenhum tipo real.
struct FakeState
{
   double a{};
   double b{};
   bool c{};
};

FieldRegistry<FakeState> registroDeTeste()
{
   FieldRegistry<FakeState> r;
   r.add({"a", FieldKind::kFloat, [](const FakeState& s) { return s.a; }});
   r.add({"b", FieldKind::kFloat, [](const FakeState& s) { return s.b; }});
   r.add({"c", FieldKind::kBool, [](const FakeState& s) { return s.c ? 1.0 : 0.0; }});
   return r;
}

TEST(FieldRegistry, AddComNomeDuplicadoLanca)
{
   FieldRegistry<FakeState> r;
   r.add({"a", FieldKind::kFloat, [](const FakeState& s) { return s.a; }});
   EXPECT_THROW(
      r.add({"a", FieldKind::kFloat, [](const FakeState& s) { return s.b; }}),
      std::logic_error);
}

TEST(FieldRegistry, FindDevolveNuloParaNomeAusente)
{
   const auto r = registroDeTeste();
   EXPECT_EQ(r.find("z"), nullptr);
   EXPECT_NE(r.find("a"), nullptr);
}

TEST(FieldRegistry, AllDevolveNaOrdemDeRegistro)
{
   const auto r = registroDeTeste();
   ASSERT_EQ(r.all().size(), 3U);
   EXPECT_EQ(r.all()[0].name, "a");
   EXPECT_EQ(r.all()[1].name, "b");
   EXPECT_EQ(r.all()[2].name, "c");
}

// bind() preserva a ORDEM DO SCHEMA -- nao a ordem de declaracao do
// registro. E' o que permite "b a" (invertido) virar um tensor na ordem
// pedida, nao na ordem em que o modelo declarou os campos.
TEST(Schema, BindPreservaAOrdemDoSchemaNaoDoRegistro)
{
   const auto r = registroDeTeste();
   const Schema schema{"invertido", {"b", "a"}};
   const auto bound = bind(schema, r);
   ASSERT_EQ(bound.resolved.size(), 2U);
   EXPECT_EQ(bound.resolved[0]->name, "b");
   EXPECT_EQ(bound.resolved[1]->name, "a");
}

TEST(Schema, BindComSubconjuntoIgnoraOsCamposNaoPedidos)
{
   const auto r = registroDeTeste();
   const Schema schema{"so-a", {"a"}};
   const auto bound = bind(schema, r);
   ASSERT_EQ(bound.resolved.size(), 1U);
   EXPECT_EQ(bound.resolved[0]->name, "a");
}

// Nome desconhecido: SchemaError, e a mensagem tem de coletar TODOS os
// nomes desconhecidos de uma vez -- um schema errado tipicamente erra em
// mais de um nome (typo sistematico, lista colada de outro modelo).
TEST(Schema, BindComNomesDesconhecidosLancaColetandoTodos)
{
   const auto r = registroDeTeste();
   const Schema schema{"ruim", {"a", "zzz", "b", "www"}};
   try {
      bind(schema, r);
      FAIL() << "bind() deveria ter lancado SchemaError";
   } catch (const SchemaError& ex) {
      const std::string msg{ex.what()};
      EXPECT_NE(msg.find("'zzz'"), std::string::npos);
      EXPECT_NE(msg.find("'www'"), std::string::npos);
      EXPECT_EQ(msg.find("'a'"), std::string::npos)
         << "nome VALIDO nao deveria aparecer na lista de desconhecidos";
      // Os nomes validos do registro tambem aparecem, para quem le o log
      // nao precisar abrir o cabecalho do WorldView.
      EXPECT_NE(msg.find("a, b, c"), std::string::npos);
   }
}

TEST(Schema, BindComSchemaVazioDevolveResolvedVazio)
{
   const auto r = registroDeTeste();
   const Schema schema{"vazio", {}};
   const auto bound = bind(schema, r);
   EXPECT_TRUE(bound.resolved.empty());
}

// pack() -- bool ja vira 0.0/1.0 dentro do proprio FieldDecl::read(), na
// ordem do schema resolvido.
TEST(Schema, PackEscreveNaOrdemDoSchemaComBoolComo0Ou1)
{
   const auto r = registroDeTeste();
   const Schema schema{"c-a-b", {"c", "a", "b"}};
   const auto bound = bind(schema, r);

   FakeState s;
   s.a = 1.5;
   s.b = 2.5;
   s.c = true;

   std::array<float, 3> saida{};
   pack(bound, s, saida.data());
   EXPECT_FLOAT_EQ(saida[0], 1.0F);   // c
   EXPECT_FLOAT_EQ(saida[1], 1.5F);   // a
   EXPECT_FLOAT_EQ(saida[2], 2.5F);   // b
}

// pack() e' templado tambem no tipo de SAIDA -- o no que fala com Python
// embutido (PyDecideAction) quer double, os que falam com ONNX (via
// libs/xinfer) querem float. Mesmo laco, dois tipos.
TEST(Schema, PackFuncionaComSaidaEmDouble)
{
   const auto r = registroDeTeste();
   const Schema schema{"a-b", {"a", "b"}};
   const auto bound = bind(schema, r);

   FakeState s;
   s.a = 3.0;
   s.b = 4.0;

   std::array<double, 2> saida{};
   pack(bound, s, saida.data());
   EXPECT_DOUBLE_EQ(saida[0], 3.0);
   EXPECT_DOUBLE_EQ(saida[1], 4.0);
}

TEST(Schema, PackComPonteiroNuloNaoAborta)
{
   const auto r = registroDeTeste();
   const auto bound = bind(Schema{"a", {"a"}}, r);
   const FakeState s;
   pack<FakeState, float>(bound, s, nullptr);
   SUCCEED();
}

} // namespace
