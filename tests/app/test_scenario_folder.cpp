#include "app/ScenarioFolder.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace fs = std::filesystem;

class ScenarioFolderTest : public ::testing::Test {
protected:
   fs::path root;

   void SetUp() override
   {
      root = fs::temp_directory_path()
           / ("scenario_folder_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed())
                                       + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
      std::error_code ec;
      fs::remove_all(root, ec);
      fs::create_directories(root);
   }

   void TearDown() override
   {
      std::error_code ec;
      fs::remove_all(root, ec);
   }

   // Cria '<root>/<nome>/configs/<arquivo>' com um conteudo qualquer.
   void criarArquivo(const std::string& nome, const std::string& arquivo)
   {
      fs::create_directories(root / nome / "configs");
      std::ofstream{root / nome / "configs" / arquivo} << "( Station )\n";
   }
};

TEST_F(ScenarioFolderTest, DescobreSubpastaComUmEdl)
{
   criarArquivo("foo", "scenario.edl");

   const auto entradas = app::discoverFolderScenarios(root.string());

   ASSERT_EQ(entradas.size(), 1u);
   EXPECT_EQ(entradas[0].name, "foo");
   EXPECT_NE(entradas[0].edlPath.find("scenario.edl"), std::string::npos);
}

TEST_F(ScenarioFolderTest, AceitaEdlPontoIn)
{
   criarArquivo("bar", "scenario.edl.in");

   const auto entradas = app::discoverFolderScenarios(root.string());

   ASSERT_EQ(entradas.size(), 1u);
   EXPECT_EQ(entradas[0].name, "bar");
}

TEST_F(ScenarioFolderTest, IgnoraGeneratedEdlComoCandidatoUnico)
{
   // 'scenario.generated.edl' sozinho (sem o .edl.in de origem) nao conta
   // como candidato valido -- e sempre um artefato de SAIDA, nunca uma
   // fonte. Contagem tem de dar ZERO, nao um (o que apareceria como
   // "achou", quando na verdade nao ha fonte nenhuma pra esse cenario).
   criarArquivo("baz", "scenario.generated.edl");

   EXPECT_TRUE(app::discoverFolderScenarios(root.string()).empty());
}

TEST_F(ScenarioFolderTest, GeneratedEdlNaoConflitaComOEdlDeOrigem)
{
   // O caso real: 'scenario.edl.in' (a fonte) + 'scenario.generated.edl'
   // (leftover de uma execucao anterior) na MESMA pasta -- tem que achar
   // exatamente 1 candidato (o '.edl.in'), nao 2 (o que pularia a pasta
   // como "ambigua").
   criarArquivo("qux", "scenario.edl.in");
   std::ofstream{root / "qux" / "configs" / "scenario.generated.edl"} << "( Station )\n";

   const auto entradas = app::discoverFolderScenarios(root.string());

   ASSERT_EQ(entradas.size(), 1u);
   EXPECT_EQ(entradas[0].name, "qux");
   EXPECT_NE(entradas[0].edlPath.find("scenario.edl.in"), std::string::npos);
}

TEST_F(ScenarioFolderTest, PulaSubpastaSemConfigs)
{
   fs::create_directories(root / "sem_configs");

   EXPECT_TRUE(app::discoverFolderScenarios(root.string()).empty());
}

TEST_F(ScenarioFolderTest, PulaConfigsVazia)
{
   fs::create_directories(root / "vazio" / "configs");

   EXPECT_TRUE(app::discoverFolderScenarios(root.string()).empty());
}

TEST_F(ScenarioFolderTest, PulaConfigsComMaisDeUmEdlSemQuebrarAsDemais)
{
   criarArquivo("ambiguo", "a.edl");
   std::ofstream{root / "ambiguo" / "configs" / "b.edl"} << "( Station )\n";
   criarArquivo("valido", "scenario.edl");

   const auto entradas = app::discoverFolderScenarios(root.string());

   ASSERT_EQ(entradas.size(), 1u);
   EXPECT_EQ(entradas[0].name, "valido");
}

TEST_F(ScenarioFolderTest, OrdenaPorNome)
{
   criarArquivo("zulu", "z.edl");
   criarArquivo("alfa", "a.edl");

   const auto entradas = app::discoverFolderScenarios(root.string());

   ASSERT_EQ(entradas.size(), 2u);
   EXPECT_EQ(entradas[0].name, "alfa");
   EXPECT_EQ(entradas[1].name, "zulu");
}

TEST_F(ScenarioFolderTest, PastaAusenteDevolveVazio)
{
   EXPECT_TRUE(app::discoverFolderScenarios((root / "nao-existe").string()).empty());
}

} // namespace
