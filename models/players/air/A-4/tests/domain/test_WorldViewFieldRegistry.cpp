//
// domain::worldViewFieldRegistry() -- o catalogo dos 38 campos numericos/
// booleanos de domain::WorldView, construido via a MESMA macro que
// libs/xrlbridge/RLBridge.cpp expande contra xrlbridge::Observation (ver o
// cabecalho de WorldViewFieldRegistry.hpp). Camada 'domain' -- sem MIXR, sem
// link contra libxrlbridge.so (FieldRegistry<State>/Schema sao header-only).
//
// O que este arquivo NAO cobre: resolveObservationSchema() (que chama
// xrlbridge::classicSchema28(), um simbolo LINKADO de libxrlbridge.so) --
// isso e' testado em tests/native/test_onnx_nodes.cpp, que ja linka o SDK
// inteiro.
//
#include "domain/WorldViewFieldRegistry.hpp"

#include "xrlbridge/Schema.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using namespace mixr::models::xA_4::domain;
using mixr::xrlbridge::FieldKind;
using mixr::xrlbridge::Schema;
using mixr::xrlbridge::bind;
using mixr::xrlbridge::pack;

TEST(WorldViewFieldRegistry, Tem38CamposNaOrdemDeObservationFields)
{
   const auto& reg = worldViewFieldRegistry();
   const std::vector<std::string> esperado{
      "northM", "eastM", "altitudeM", "headingDeg", "speedKts", "rollDeg",
      "pitchDeg", "fuelFraction", "mach", "gLoad", "alphaDeg", "terrainElevM",
      "altitudeAglM", "contactRangeM", "contactRelBearingDeg", "contactDeltaAltM",
      "contactNorthM", "contactEastM", "contactAltitudeM", "alertNorthM",
      "alertEastM", "alertAltitudeM", "alertRangeM",
      "valid", "terrainValid", "hasContact", "hasAlert", "weaponReady",
      "rwrThreatRangeM", "rwrThreatRelBearingDeg", "rwrThreatDeltaAltM", "hasRwrThreat",
      "navTrueBrgDeg", "navCmdAltM", "navCmdSpeedKts",
      "hasNavSteering", "hasNavCmdAlt", "hasNavCmdSpeed"};

   ASSERT_EQ(reg.all().size(), esperado.size());
   for (std::size_t i = 0; i < esperado.size(); ++i) {
      EXPECT_EQ(reg.all()[i].name, esperado[i]) << "posicao " << i;
   }
}

TEST(WorldViewFieldRegistry, OsNoveBooleanosTemKindCorreto)
{
   const auto& reg = worldViewFieldRegistry();
   const std::vector<std::string> booleanos{
      "valid", "terrainValid", "hasContact", "hasAlert", "weaponReady",
      "hasRwrThreat", "hasNavSteering", "hasNavCmdAlt", "hasNavCmdSpeed"};
   for (const auto& nome : booleanos) {
      const auto* const decl = reg.find(nome);
      ASSERT_NE(decl, nullptr) << nome;
      EXPECT_EQ(decl->kind, FieldKind::kBool) << nome;
   }
   // Um float qualquer, para confirmar que o teste distingue kind de verdade.
   const auto* const norte = reg.find("northM");
   ASSERT_NE(norte, nullptr);
   EXPECT_EQ(norte->kind, FieldKind::kFloat);
}

// Cada read() tem que ler o campo CERTO -- um WorldView com um valor
// sentinela DISTINTO por campo pega lambda copiada/colada errada (o
// contrario de "todos zerados", que não distinguiria northM lendo eastM por
// engano).
TEST(WorldViewFieldRegistry, ReadDeCadaCampoBateComOMembroCerto)
{
   WorldView s;
   s.northM = 1.0;
   s.eastM = 2.0;
   s.altitudeM = 3.0;
   s.rwrThreatRangeM = 30.0;
   s.hasRwrThreat = true;
   s.navCmdSpeedKts = 34.0;
   s.hasNavCmdSpeed = true;

   const auto& reg = worldViewFieldRegistry();
   EXPECT_DOUBLE_EQ(reg.find("northM")->read(s), 1.0);
   EXPECT_DOUBLE_EQ(reg.find("eastM")->read(s), 2.0);
   EXPECT_DOUBLE_EQ(reg.find("altitudeM")->read(s), 3.0);
   EXPECT_DOUBLE_EQ(reg.find("rwrThreatRangeM")->read(s), 30.0);
   EXPECT_DOUBLE_EQ(reg.find("hasRwrThreat")->read(s), 1.0);
   EXPECT_DOUBLE_EQ(reg.find("navCmdSpeedKts")->read(s), 34.0);
   EXPECT_DOUBLE_EQ(reg.find("hasNavCmdSpeed")->read(s), 1.0);
   // Um bool nao setado le 0.0 -- confirma que o read() de bool converte
   // (nao devolve o bool cru reinterpretado).
   EXPECT_DOUBLE_EQ(reg.find("hasContact")->read(s), 0.0);
}

TEST(WorldViewFieldRegistry, FindDeNomeInexistenteDevolveNulo)
{
   EXPECT_EQ(worldViewFieldRegistry().find("campoQueNaoExiste"), nullptr);
}

// bind()/pack() contra o registro REAL, com um schema ad-hoc que so pede
// os campos de RWR -- prova que a selecao por nome funciona ponta a ponta
// contra domain::WorldView, nao so contra o FakeState de
// tests/domain/test_field_registry.cpp (no core).
TEST(WorldViewFieldRegistry, BindEPackContraUmSchemaAdHocDeCamposDeRwr)
{
   WorldView s;
   s.rwrThreatRangeM = 11.0;
   s.rwrThreatRelBearingDeg = 22.0;
   s.hasRwrThreat = true;

   const Schema schema{"rwr", {"rwrThreatRangeM", "rwrThreatRelBearingDeg", "hasRwrThreat"}};
   const auto bound = bind(schema, worldViewFieldRegistry());
   ASSERT_EQ(bound.resolved.size(), 3U);

   float saida[3]{};
   pack(bound, s, saida);
   EXPECT_FLOAT_EQ(saida[0], 11.0F);
   EXPECT_FLOAT_EQ(saida[1], 22.0F);
   EXPECT_FLOAT_EQ(saida[2], 1.0F);
}

TEST(WorldViewFieldRegistry, BindComNomeDesconhecidoLanca)
{
   const Schema schema{"ruim", {"northM", "campoInventado"}};
   EXPECT_THROW(bind(schema, worldViewFieldRegistry()), mixr::xrlbridge::SchemaError);
}

} // namespace
