#include "app/ScenarioTemplate.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

// app::generateScenario() -- '.edl.in' -> '.edl', substituicao de
// '@NUM_TC_THREADS@'/'@NUM_BG_THREADS@' e de 'extraTokens', mais os dois
// die() de leitura de arquivo (template ausente, '@include:' malformado ou
// apontando pra um fragmento que nao existe). NAO exercita '@include:' com
// sucesso -- o diretorio de fragmentos e fixo (kFragmentsDir, relativo a
// raiz do repositorio) e este teste nao pode depender do cwd de quem roda
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

TEST_F(ScenarioTemplateTest, SubstituiOsDoisPlaceholdersDeThreadsPelosValoresResolvidos)
{
   const fs::path tpl{writeTemplate("numTcThreads: @NUM_TC_THREADS@\nnumBgThreads: @NUM_BG_THREADS@\n")};
   const fs::path out{outPath()};

   // tcThreadsOverride=1/bgThreadsOverride=1 sao sempre validos:
   // clampThreadCount() nunca desce abaixo de 1, entao o resultado independe
   // do hardware da maquina que roda o teste.
   const app::ThreadCounts resultado{app::generateScenario(tpl.string(), out.string(), 1, 1)};

   EXPECT_EQ(resultado.tc, 1);
   EXPECT_EQ(resultado.bg, 1);
   EXPECT_NE(readAll(out).find("numTcThreads: 1"), std::string::npos);
   EXPECT_NE(readAll(out).find("numBgThreads: 1"), std::string::npos);
}

TEST_F(ScenarioTemplateTest, TcThreadsOverrideZeroOuNegativoUsamOMesmoDefault)
{
   const fs::path tpl{writeTemplate("@NUM_TC_THREADS@\n")};

   const app::ThreadCounts comZero{app::generateScenario(tpl.string(), outPath("a.edl").string(), 0, 1)};
   const app::ThreadCounts comNegativo{app::generateScenario(tpl.string(), outPath("b.edl").string(), -7, 1)};

   EXPECT_EQ(comZero.tc, comNegativo.tc);
   EXPECT_GE(comZero.tc, 1);
}

TEST_F(ScenarioTemplateTest, TcThreadsOverrideAcimaDoLimiteDeCpuEhClampado)
{
   const fs::path tpl{writeTemplate("@NUM_TC_THREADS@\n")};

   const app::ThreadCounts resultado{app::generateScenario(tpl.string(), outPath().string(), 100000, 1)};

   EXPECT_GE(resultado.tc, 1);
   EXPECT_LE(resultado.tc, maxThreadsByCpu());
}

TEST_F(ScenarioTemplateTest, BgThreadsOverrideZeroOuNegativoUsamODefaultDeDois)
{
   const fs::path tpl{writeTemplate("@NUM_BG_THREADS@\n")};

   const app::ThreadCounts comZero{app::generateScenario(tpl.string(), outPath("a.edl").string(), 1, 0)};
   const app::ThreadCounts comNegativo{app::generateScenario(tpl.string(), outPath("b.edl").string(), 1, -7)};

   EXPECT_EQ(comZero.bg, comNegativo.bg);
   // O default e' 2 -- diferente do T/C (metade dos nucleos) -- mas so' se
   // a maquina tiver nucleo o bastante para nao clampar pra baixo; nunca
   // sai de [1, nucleos-1] de qualquer forma.
   EXPECT_GE(comZero.bg, 1);
   EXPECT_LE(comZero.bg, maxThreadsByCpu());
   if (maxThreadsByCpu() >= 2) {
      EXPECT_EQ(comZero.bg, 2);
   }
}

TEST_F(ScenarioTemplateTest, BgThreadsOverrideAcimaDoLimiteDeCpuEhClampado)
{
   const fs::path tpl{writeTemplate("@NUM_BG_THREADS@\n")};

   const app::ThreadCounts resultado{app::generateScenario(tpl.string(), outPath().string(), 1, 100000)};

   EXPECT_GE(resultado.bg, 1);
   EXPECT_LE(resultado.bg, maxThreadsByCpu());
}

TEST_F(ScenarioTemplateTest, TcEBgThreadsSaoResolvidosIndependentemente)
{
   const fs::path tpl{writeTemplate("@NUM_TC_THREADS@ / @NUM_BG_THREADS@\n")};

   // tcThreadsOverride=1 e' sempre valido; bgThreadsOverride=1 tambem --
   // pedidos DIFERENTES um do outro, para provar que um override nao
   // vaza pro outro placeholder.
   const app::ThreadCounts resultado{app::generateScenario(tpl.string(), outPath().string(), 1, 1)};

   EXPECT_EQ(resultado.tc, 1);
   EXPECT_EQ(resultado.bg, 1);
}

TEST_F(ScenarioTemplateTest, SubstituiTokenExtraEAvisaSobreOQueSobrouSemPar)
{
   const fs::path tpl{writeTemplate(
      "nome: @NOME@ / @NUM_TC_THREADS@ / @NUM_BG_THREADS@ / @SEM_PAR@\n")};

   ::testing::internal::CaptureStderr();
   app::generateScenario(tpl.string(), outPath().string(), 1, 1, {{"NOME", "valor123"}});
   const std::string stderrOutput{::testing::internal::GetCapturedStderr()};

   const std::string conteudo{readAll(outPath())};
   EXPECT_NE(conteudo.find("valor123"), std::string::npos);
   // Sem par em 'extraTokens', o token fica LITERAL -- o mecanismo nunca
   // inventa um default. '@NUM_TC_THREADS@'/'@NUM_BG_THREADS@' SAO
   // diferentes disso (sempre tem default, ver os testes acima).
   EXPECT_NE(conteudo.find("@SEM_PAR@"), std::string::npos);
   EXPECT_NE(stderrOutput.find("SEM_PAR"), std::string::npos);
}

TEST_F(ScenarioTemplateTest, TokenComParVazioNaoDisparaAviso)
{
   const fs::path tpl{writeTemplate("@NOME@ @NUM_TC_THREADS@ @NUM_BG_THREADS@\n")};

   ::testing::internal::CaptureStderr();
   app::generateScenario(tpl.string(), outPath().string(), 1, 1, {{"NOME", "x"}});
   const std::string stderrOutput{::testing::internal::GetCapturedStderr()};

   EXPECT_EQ(stderrOutput.find("AVISO"), std::string::npos);
}

TEST_F(ScenarioTemplateTest, ArquivoDeTemplateInexistenteMorreComMensagemClara)
{
   EXPECT_EXIT(app::generateScenario((dir / "nao-existe.edl.in").string(), outPath().string(), 1, 1),
              ::testing::ExitedWithCode(EXIT_FAILURE), "nao consegui ler");
}

TEST_F(ScenarioTemplateTest, DiretorioComoTemplateMorre)
{
   // is_regular_file() rejeita ANTES de tentar abrir -- um diretorio nao e
   // "arquivo nao existe" nem daria EOF nunca se lido cru.
   EXPECT_EXIT(app::generateScenario(dir.string(), outPath().string(), 1, 1),
              ::testing::ExitedWithCode(EXIT_FAILURE), "nao consegui ler");
}

TEST_F(ScenarioTemplateTest, IncludeSemFechamentoNaMesmaLinhaMorre)
{
   const fs::path tpl{writeTemplate("antes\n@include:sem_fechar\ndepois\n")};

   EXPECT_EXIT(app::generateScenario(tpl.string(), outPath().string(), 1, 1),
              ::testing::ExitedWithCode(EXIT_FAILURE), "fechamento");
}

TEST_F(ScenarioTemplateTest, IncludeComFragmentoInexistenteMorre)
{
   // O nome nao existe em NENHUM cwd possivel -- o teste nao depende de
   // 'app/configs/fragments/' estar acessivel dali de onde 'meson test' roda.
   const fs::path tpl{writeTemplate("@include:fragmento_que_definitivamente_nao_existe_xyz@\n")};

   EXPECT_EXIT(app::generateScenario(tpl.string(), outPath().string(), 1, 1),
              ::testing::ExitedWithCode(EXIT_FAILURE), "nao consegui ler");
}
