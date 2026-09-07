#include "app/ScenarioTemplate.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

// app::generateScenario() -- '.edl.in' -> '.edl', substituicao de
// '@NUM_TC_THREADS@' e de 'extraTokens', mais os dois die() de leitura de
// arquivo (template ausente, '@include:' malformado ou apontando pra um
// fragmento que nao existe). NAO exercita '@include:' com sucesso -- o
// diretorio de fragmentos e fixo (kFragmentsDir, relativo a raiz do
// repositorio) e este teste nao pode depender do cwd de quem roda
// 'meson test'; os dois caminhos de ERRO de '@include:' nao dependem disso
// (o marcador malformado morre ANTES de ler qualquer arquivo, e um nome de
// fragmento inexistente falha em QUALQUER cwd).

namespace {

namespace fs = std::filesystem;

class ScenarioTemplateTest : public ::testing::Test {
protected:
   fs::path dir;

   void SetUp() override
   {
      dir = fs::temp_directory_path()
          / ("scenario_template_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed())
                                        + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
      std::error_code ec;
      fs::remove_all(dir, ec);
      fs::create_directories(dir);
   }

   void TearDown() override
   {
      std::error_code ec;
      fs::remove_all(dir, ec);
   }

   fs::path writeTemplate(const std::string& content, const std::string& name = "modelo.edl.in") const
   {
      const fs::path path{dir / name};
      std::ofstream{path} << content;
      return path;
   }

   fs::path outPath(const std::string& name = "saida.edl") const { return dir / name; }

   static std::string readAll(const fs::path& path)
   {
      std::ifstream in{path};
      std::ostringstream oss;
      oss << in.rdbuf();
      return oss.str();
   }
};

int maxThreadsByCpu()
{
   const unsigned int hw{std::thread::hardware_concurrency()};
   return static_cast<int>(hw > 1 ? hw - 1 : 1);
}

} // namespace

TEST_F(ScenarioTemplateTest, SubstituiPlaceholderDeThreadsPeloValorResolvido)
{
   const fs::path tpl{writeTemplate("numTcThreads: @NUM_TC_THREADS@\n")};
   const fs::path out{outPath()};

   // threadsOverride=1 e sempre valido: resolveTcThreadCount() nunca desce
   // abaixo de 1, entao o resultado independe do hardware da maquina que
   // roda o teste.
   const int resultado{app::generateScenario(tpl.string(), out.string(), 1)};

   EXPECT_EQ(resultado, 1);
   EXPECT_NE(readAll(out).find("numTcThreads: 1"), std::string::npos);
}

TEST_F(ScenarioTemplateTest, ThreadsOverrideZeroOuNegativoUsamOMesmoDefault)
{
   const fs::path tpl{writeTemplate("@NUM_TC_THREADS@\n")};

   const int comZero{app::generateScenario(tpl.string(), outPath("a.edl").string(), 0)};
   const int comNegativo{app::generateScenario(tpl.string(), outPath("b.edl").string(), -7)};

   EXPECT_EQ(comZero, comNegativo);
   EXPECT_GE(comZero, 1);
}

TEST_F(ScenarioTemplateTest, ThreadsOverrideAcimaDoLimiteDeCpuEhClampado)
{
   const fs::path tpl{writeTemplate("@NUM_TC_THREADS@\n")};

   const int resultado{app::generateScenario(tpl.string(), outPath().string(), 100000)};

   EXPECT_GE(resultado, 1);
   EXPECT_LE(resultado, maxThreadsByCpu());
}

TEST_F(ScenarioTemplateTest, SubstituiTokenExtraEAvisaSobreOQueSobrouSemPar)
{
   const fs::path tpl{writeTemplate("nome: @NOME@ / @NUM_TC_THREADS@ / @SEM_PAR@\n")};

   ::testing::internal::CaptureStderr();
   app::generateScenario(tpl.string(), outPath().string(), 1, {{"NOME", "valor123"}});
   const std::string stderrOutput{::testing::internal::GetCapturedStderr()};

   const std::string conteudo{readAll(outPath())};
   EXPECT_NE(conteudo.find("valor123"), std::string::npos);
   // Sem par em 'extraTokens', o token fica LITERAL -- o mecanismo nunca
   // inventa um default.
   EXPECT_NE(conteudo.find("@SEM_PAR@"), std::string::npos);
   EXPECT_NE(stderrOutput.find("SEM_PAR"), std::string::npos);
}

TEST_F(ScenarioTemplateTest, TokenComParVazioNaoDisparaAviso)
{
   const fs::path tpl{writeTemplate("@NOME@ @NUM_TC_THREADS@\n")};

   ::testing::internal::CaptureStderr();
   app::generateScenario(tpl.string(), outPath().string(), 1, {{"NOME", "x"}});
   const std::string stderrOutput{::testing::internal::GetCapturedStderr()};

   EXPECT_EQ(stderrOutput.find("AVISO"), std::string::npos);
}

TEST_F(ScenarioTemplateTest, ArquivoDeTemplateInexistenteMorreComMensagemClara)
{
   EXPECT_EXIT(app::generateScenario((dir / "nao-existe.edl.in").string(), outPath().string(), 1),
              ::testing::ExitedWithCode(EXIT_FAILURE), "nao consegui ler");
}

TEST_F(ScenarioTemplateTest, DiretorioComoTemplateMorre)
{
   // is_regular_file() rejeita ANTES de tentar abrir -- um diretorio nao e
   // "arquivo nao existe" nem daria EOF nunca se lido cru.
   EXPECT_EXIT(app::generateScenario(dir.string(), outPath().string(), 1),
              ::testing::ExitedWithCode(EXIT_FAILURE), "nao consegui ler");
}

TEST_F(ScenarioTemplateTest, IncludeSemFechamentoNaMesmaLinhaMorre)
{
   const fs::path tpl{writeTemplate("antes\n@include:sem_fechar\ndepois\n")};

   EXPECT_EXIT(app::generateScenario(tpl.string(), outPath().string(), 1),
              ::testing::ExitedWithCode(EXIT_FAILURE), "fechamento");
}

TEST_F(ScenarioTemplateTest, IncludeComFragmentoInexistenteMorre)
{
   // O nome nao existe em NENHUM cwd possivel -- o teste nao depende de
   // 'app/configs/fragments/' estar acessivel dali de onde 'meson test' roda.
   const fs::path tpl{writeTemplate("@include:fragmento_que_definitivamente_nao_existe_xyz@\n")};

   EXPECT_EXIT(app::generateScenario(tpl.string(), outPath().string(), 1),
              ::testing::ExitedWithCode(EXIT_FAILURE), "nao consegui ler");
}
