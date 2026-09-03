#include "app/ScenarioAssembler.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;

std::string readFile(const fs::path& p)
{
   std::ifstream in(p);
   std::ostringstream ss;
   ss << in.rdbuf();
   return ss.str();
}

void writeFile(const fs::path& p, const std::string& content)
{
   std::ofstream out(p);
   out << content;
}

class ScenarioAssemblerTest : public ::testing::Test
{
protected:
   fs::path dir;

   void SetUp() override
   {
      dir = fs::temp_directory_path() / ("scenario-assembler-test-" + std::to_string(::getpid()));
      fs::create_directories(dir);
   }

   void TearDown() override
   {
      std::error_code ec;
      fs::remove_all(dir, ec);
   }

   fs::path p(const std::string& name) const { return dir / name; }
};

} // namespace

TEST_F(ScenarioAssemblerTest, ConcatenaNaOrdemPrefixoCorpoSufixo)
{
   writeFile(p("prefix.epp.in"), "A\n");
   writeFile(p("body.epp"), "B\n");
   writeFile(p("suffix.epp.in"), "C\n");

   app::assembleScenario(p("prefix.epp.in").string(), p("body.epp").string(),
                         p("suffix.epp.in").string(), p("out.epp").string(), 1);

   EXPECT_EQ(readFile(p("out.epp")), "A\nB\nC\n");
}

TEST_F(ScenarioAssemblerTest, SubstituiTodasAsOcorrenciasDoToken)
{
   writeFile(p("prefix.epp.in"), "numTcThreads: @NUM_TC_THREADS@ -- de novo: @NUM_TC_THREADS@\n");
   writeFile(p("body.epp"), "\n");
   writeFile(p("suffix.epp.in"), "\n");

   // threadsOverride=1 e o UNICO valor que da resultado determinístico em
   // qualquer maquina: resolveTcThreadCount() clampa em
   // [1, max(hardware_concurrency()-1, 1)], e esse intervalo sempre inclui
   // 1 -- um override maior poderia ser clampado para baixo numa maquina de
   // poucos nucleos, o que tornaria o teste dependente do hardware do CI.
   const int n{app::assembleScenario(p("prefix.epp.in").string(), p("body.epp").string(),
                                     p("suffix.epp.in").string(), p("out.epp").string(), 1)};

   EXPECT_EQ(n, 1);
   const std::string out{readFile(p("out.epp"))};
   EXPECT_NE(out.find("numTcThreads: 1 -- de novo: 1"), std::string::npos);
   EXPECT_EQ(out.find("@NUM_TC_THREADS@"), std::string::npos) << "token nao pode sobrar no arquivo montado";
}

TEST_F(ScenarioAssemblerTest, SemOverrideDevolveValorPositivo)
{
   writeFile(p("prefix.epp.in"), "@NUM_TC_THREADS@\n");
   writeFile(p("body.epp"), "\n");
   writeFile(p("suffix.epp.in"), "\n");

   const int n{app::assembleScenario(p("prefix.epp.in").string(), p("body.epp").string(),
                                     p("suffix.epp.in").string(), p("out.epp").string(), 0)};

   EXPECT_GE(n, 1);
}
