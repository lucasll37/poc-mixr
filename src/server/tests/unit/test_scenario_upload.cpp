#include "app/ScenarioUpload.hpp"

#include <gtest/gtest.h>

#include <string>

using app::validateScenarioBody;

namespace {

bool contains(const std::vector<std::string>& errors, const std::string& needle)
{
   for (const auto& e : errors) {
      if (e.find(needle) != std::string::npos) return true;
   }
   return false;
}

} // namespace

TEST(ScenarioUpload, AceitaCorpoValido)
{
   const auto r = validateScenarioBody("alpha1: ( Aircraft id: 1 )", 1024);
   EXPECT_TRUE(r.valid);
   EXPECT_TRUE(r.errors.empty());
}

TEST(ScenarioUpload, RecusaCorpoVazio)
{
   const auto r = validateScenarioBody("", 1024);
   EXPECT_FALSE(r.valid);
}

TEST(ScenarioUpload, RecusaAcimaDoLimite)
{
   const std::string grande(2000, 'x');
   const auto r = validateScenarioBody(grande, 1024);
   EXPECT_FALSE(r.valid);
}

TEST(ScenarioUpload, RecusaByteNul)
{
   std::string corpo{"a"};
   corpo.push_back('\0');
   corpo += "b";
   const auto r = validateScenarioBody(corpo, 1024);
   EXPECT_FALSE(r.valid);
}

// Os quatro tokens abaixo sao recusados pelo MESMO motivo estrutural: o
// sim-runner ja injeta o proprio PluginLoader (ver scenario_prefix.epp.in),
// entao um corpo de cliente que tente carregar plugin (dlopen arbitrario),
// abrir rede DIS (nao-hermetico) ou declarar seu proprio dataRecorder
// (colide de porta Tacview entre requisicoes concorrentes) e recusado antes
// de chegar perto do edl_parser -- ver src/server/README.md.
TEST(ScenarioUpload, RecusaPluginLoader)
{
   const auto r = validateScenarioBody("plugins: ( PluginLoader )", 1024);
   EXPECT_FALSE(r.valid);
   EXPECT_TRUE(contains(r.errors, "PluginLoader"));
}

TEST(ScenarioUpload, RecusaPluginModule)
{
   const auto r = validateScenarioBody("( PluginModule file: \"x.so\" )", 1024);
   EXPECT_FALSE(r.valid);
   EXPECT_TRUE(contains(r.errors, "PluginModule"));
}

TEST(ScenarioUpload, RecusaNetworks)
{
   const auto r = validateScenarioBody("networks: ( DisNetIO )", 1024);
   EXPECT_FALSE(r.valid);
   EXPECT_TRUE(contains(r.errors, "networks:"));
}

TEST(ScenarioUpload, RecusaDataRecorder)
{
   const auto r = validateScenarioBody("dataRecorder: ( DataRecorder )", 1024);
   EXPECT_FALSE(r.valid);
   EXPECT_TRUE(contains(r.errors, "dataRecorder:"));
}

TEST(ScenarioUpload, AcumulaTodosOsErrosEmVezDeParar)
{
   const auto r = validateScenarioBody("networks: (X) dataRecorder: (Y)", 1024);
   EXPECT_FALSE(r.valid);
   EXPECT_GE(r.errors.size(), 2u);
}
