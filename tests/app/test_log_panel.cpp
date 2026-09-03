#include "app/LogPanel.hpp"

#include "xlog/Log.hpp"

#include <gtest/gtest.h>

#include <string>

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
