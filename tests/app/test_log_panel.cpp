#include "app/LogPanel.hpp"

#include "xlog/Log.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

// O buffer em memoria de shared/xlog (a fonte da aba "Log" do ./app) e as
// duas regras puras do painel: filtro por nivel minimo e o ciclo do
// filtro.
//
// O buffer e estado GLOBAL do processo (uma copia so, de proposito -- e o
// que faz o LOG(...) do plugin do modelo cair no mesmo lugar que o do
// host). Por isso cada teste ancora no seq corrente em vez de supor que
// comeca vazio: rodar em qualquer ordem tem de dar o mesmo resultado.

using mixr::xlog::Level;

namespace {

// O console fica desligado durante a suite pelo mesmo motivo do ./app: nao
// sujar a saida de quem e dono dela (aqui, o relatorio do gtest). Nao
// afeta o buffer -- que e justamente o que se quer medir.
class XlogBuffer : public ::testing::Test {
protected:
   void SetUp() override { mixr::xlog::setConsoleEnabled(false); }
   void TearDown() override { mixr::xlog::setLoggingEnabled(true); }
};

// Diretorio temporario de verdade -- para provar que init()/o sink de
// arquivo funcionam quando o disco coopera. mkdtemp() e nao um nome
// inventado: evita a corrida entre gerar o nome e criar o diretorio,
// mesmo raciocinio do mkstemp() ja usado em tests/domain/test_xinfer.cpp.
class DiretorioTemporario {
public:
   DiretorioTemporario()
   {
      char molde[]{"/tmp/xlog-teste-XXXXXX"};
      const char* const dir{::mkdtemp(molde)};
      caminho_ = (dir != nullptr) ? dir : "";
   }
   ~DiretorioTemporario()
   {
      if (!caminho_.empty()) ::rmdir(caminho_.c_str());
   }

   const std::string& caminho() const { return caminho_; }

private:
   std::string caminho_;
};

} // namespace

TEST_F(XlogBuffer, GuardaNivelCarimboETextoSeparados)
{
   const std::uint64_t before{mixr::xlog::lastSeq()};
   LOG(WARNING) << "alerta " << 42;

   const auto entries{mixr::xlog::snapshot()};
   ASSERT_FALSE(entries.empty());
   const auto& last{entries.back()};

   EXPECT_EQ(last.seq, before + 1);
   EXPECT_EQ(last.level, Level::WARNING);
   EXPECT_EQ(last.text, "alerta 42");        // so a mensagem: sem carimbo, sem nivel
   EXPECT_FALSE(last.time.empty());          // "HH:MM:SS.mmm"
}

TEST_F(XlogBuffer, OrdemEhDoMaisAntigoParaOMaisNovo)
{
   LOG(INFO) << "primeira";
   LOG(INFO) << "segunda";

   const auto entries{mixr::xlog::snapshot()};
   ASSERT_GE(entries.size(), 2u);
   const auto& penultimate{entries[entries.size() - 2]};
   const auto& last{entries.back()};

   EXPECT_EQ(penultimate.text, "primeira");
   EXPECT_EQ(last.text, "segunda");
   EXPECT_LT(penultimate.seq, last.seq);
}

TEST_F(XlogBuffer, DesligadoNaoRegistraNada)
{
   const std::uint64_t before{mixr::xlog::lastSeq()};
   mixr::xlog::setLoggingEnabled(false);
   LOG(ERROR) << "isto nao pode aparecer";
   mixr::xlog::setLoggingEnabled(true);

   EXPECT_EQ(mixr::xlog::lastSeq(), before);
   for (const auto& e : mixr::xlog::snapshot()) {
      EXPECT_NE(e.text, "isto nao pode aparecer");
   }
}

TEST_F(XlogBuffer, PassadaACapacidadeDescartaOMaisAntigo)
{
   // Enche com folga: a janela tem de ficar exatamente em kMemoryCapacity,
   // e a entrada mais antiga tem de ser a que sobrou depois do descarte --
   // 'seq' e o que permite afirmar isso (ele nunca reinicia).
   const std::size_t extra{25};
   for (std::size_t i = 0; i < mixr::xlog::kMemoryCapacity + extra; i++) {
      LOG(DEBUG) << "linha " << i;
   }

   const auto entries{mixr::xlog::snapshot()};
   ASSERT_EQ(entries.size(), mixr::xlog::kMemoryCapacity);
   EXPECT_EQ(entries.back().seq, mixr::xlog::lastSeq());
   EXPECT_EQ(entries.back().seq - entries.front().seq, mixr::xlog::kMemoryCapacity - 1);
   EXPECT_EQ(entries.back().text, "linha " + std::to_string(mixr::xlog::kMemoryCapacity + extra - 1));
}

// Bug real encontrado nesta bateria (Log.cpp, ~linha 118 antes do fix):
// mixr::recorder::PrintHandler::printToOutput() tem o proprio fallback
// nativo -- quando o arquivo NAO esta aberto (setFilename() apontou para
// um diretorio que nao existe, a mesma armadilha 1 ja documentada em
// Log.hpp: "data/logs/ precisa existir no disco antes do init()"), ele
// escreve em std::cout por conta propria, por FORA de 'g_consoleEnabled'.
// Sem a guarda 'g_sink->isOpen()' em Stream::~Stream(), isso quebrava
// exatamente o que setConsoleEnabled(false) existe para garantir -- o
// ./app desliga o console para o FTXUI nao ter a tela suja por baixo, e
// aqui a linha vazava do mesmo jeito.
TEST_F(XlogBuffer, ConsoleDesligadoNaoVazaPeloSinkQuandoArquivoFalhaAoAbrir)
{
   const DiretorioTemporario dir;
   ASSERT_FALSE(dir.caminho().empty());

   // Subdiretorio que nao existe -- PrintHandler::openFile() falha e
   // isOpen() fica false para sempre (nada aqui volta a tentar abrir).
   mixr::xlog::init(dir.caminho() + "/subdir-inexistente/mission.log");
   mixr::xlog::setConsoleEnabled(false);

   ::testing::internal::CaptureStdout();
   LOG(ERROR) << "isto nao pode vazar pro console";
   const std::string captured{::testing::internal::GetCapturedStdout()};

   EXPECT_TRUE(captured.empty())
      << "console desligado, mas a linha vazou via PrintHandler: " << captured;

   // A linha continua indo pro buffer -- console desligado nao e
   // logging desligado.
   const auto entries{mixr::xlog::snapshot()};
   ASSERT_FALSE(entries.empty());
   EXPECT_EQ(entries.back().text, "isto nao pode vazar pro console");
}

// Mesma causa raiz, efeito diferente: com o console LIGADO e o arquivo
// falhando ao abrir, a linha saia DUAS vezes -- uma da nossa propria
// escrita (g_consoleEnabled) e outra do fallback do PrintHandler.
TEST_F(XlogBuffer, ConsoleLigadoNaoDuplicaLinhaQuandoArquivoFalhaAoAbrir)
{
   const DiretorioTemporario dir;
   ASSERT_FALSE(dir.caminho().empty());

   mixr::xlog::init(dir.caminho() + "/outro-subdir-inexistente/mission.log");
   mixr::xlog::setConsoleEnabled(true);

   ::testing::internal::CaptureStdout();
   LOG(WARNING) << "soh uma vez no console";
   const std::string captured{::testing::internal::GetCapturedStdout()};
   mixr::xlog::setConsoleEnabled(false);   // devolve o invariante da fixture

   std::size_t count{};
   std::size_t pos{};
   while ((pos = captured.find("soh uma vez no console", pos)) != std::string::npos) {
      count++;
      pos += 1;
   }
   EXPECT_EQ(count, 1u) << "linha duplicada no console:\n" << captured;
}

// Contraprova: quando o diretorio EXISTE de verdade, o sink continua
// escrevendo no arquivo normalmente -- a guarda 'isOpen()' do fix nao pode
// silenciar o caminho feliz.
TEST_F(XlogBuffer, ArquivoAbertoComSucessoRecebeALinhaSemVazarProConsole)
{
   const DiretorioTemporario dir;
   ASSERT_FALSE(dir.caminho().empty());
   const std::string logPath{dir.caminho() + "/mission.log"};

   mixr::xlog::init(logPath);
   mixr::xlog::setConsoleEnabled(false);

   ::testing::internal::CaptureStdout();
   LOG(INFO) << "linha gravada no arquivo";
   const std::string captured{::testing::internal::GetCapturedStdout()};

   EXPECT_TRUE(captured.empty()) << "console desligado nao deveria imprimir nada: " << captured;

   std::ifstream file{logPath};
   ASSERT_TRUE(file.is_open()) << "arquivo de log deveria ter sido criado em " << logPath;
   std::ostringstream fileContents;
   fileContents << file.rdbuf();
   EXPECT_NE(fileContents.str().find("linha gravada no arquivo"), std::string::npos);

   file.close();
   std::remove(logPath.c_str());
}

TEST(LogPanelFilter, NivelMinimoDeixaPassarDoNivelParaCima)
{
   EXPECT_TRUE(app::passesLevelFilter(Level::DEBUG, Level::DEBUG));
   EXPECT_TRUE(app::passesLevelFilter(Level::ERROR, Level::DEBUG));

   EXPECT_FALSE(app::passesLevelFilter(Level::DEBUG, Level::WARNING));
   EXPECT_FALSE(app::passesLevelFilter(Level::INFO, Level::WARNING));
   EXPECT_TRUE(app::passesLevelFilter(Level::WARNING, Level::WARNING));
   EXPECT_TRUE(app::passesLevelFilter(Level::ERROR, Level::WARNING));
}

TEST(LogPanelFilter, CicloDoFiltroVoltaAoInicio)
{
   EXPECT_EQ(app::nextLevelFilter(Level::DEBUG), Level::INFO);
   EXPECT_EQ(app::nextLevelFilter(Level::INFO), Level::WARNING);
   EXPECT_EQ(app::nextLevelFilter(Level::WARNING), Level::ERROR);
   EXPECT_EQ(app::nextLevelFilter(Level::ERROR), Level::DEBUG);
}

TEST(LogPanelFilter, TextoPlanoDaLinhaCarregaOsQuatroCampos)
{
   // E o rotulo de fallback do ftxui::Menu (o desenho de verdade vem de
   // renderLogRow) -- tem de conter tudo que identifica a linha.
   const mixr::xlog::Entry e{7, Level::ERROR, "12:34:56.789", "falhou feio"};
   const std::string text{app::logRowText(e)};

   EXPECT_NE(text.find("#7"), std::string::npos);
   EXPECT_NE(text.find("12:34:56.789"), std::string::npos);
   EXPECT_NE(text.find("ERROR"), std::string::npos);
   EXPECT_NE(text.find("falhou feio"), std::string::npos);
}
