#include "app/BackgroundPanel.hpp"

#include <gtest/gtest.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <string>

// app/BackgroundPanel.cpp -- a aba "Tempo Nao-Critico" (F4). BackgroundInfo
// e struct pura (app/DashboardState.hpp so encaminha forward-declarations de
// MIXR, nunca usadas por este arquivo), entao a unica dependencia real e
// FTXUI, exercitada por Screen::ToString() sobre o Element renderizado.

namespace {

std::string renderToText(const app::BackgroundInfo& bg, const int w = 200, const int h = 60)
{
   auto screen{ftxui::Screen::Create(ftxui::Dimension::Fixed(w), ftxui::Dimension::Fixed(h))};
   ftxui::Render(screen, app::renderBackgroundPanel(bg));
   return screen.ToString();
}

} // namespace

//------------------------------------------------------------------------------
// Secao Tacview -- seis estados distintos do socket, nenhum deles booleano.
//------------------------------------------------------------------------------
TEST(BackgroundPanelTacview, DesligadoMostraMensagemDeAusencia)
{
   const app::BackgroundInfo bg;   // tacviewEnabled falso por default
   EXPECT_NE(renderToText(bg).find("nenhum ( TacviewOutput )"), std::string::npos);
}

TEST(BackgroundPanelTacview, FalhouNoBindMostraFalhou)
{
   app::BackgroundInfo bg;
   bg.tacviewEnabled = true;
   bg.tacviewInitFailed = true;

   EXPECT_NE(renderToText(bg).find("FALHOU"), std::string::npos);
}

TEST(BackgroundPanelTacview, PortaOcupadaSoGravandoEmArquivo)
{
   app::BackgroundInfo bg;
   bg.tacviewEnabled = true;
   bg.tacviewInitialized = true;
   bg.tacviewListening = false;

   EXPECT_NE(renderToText(bg).find("porta OCUPADA"), std::string::npos);
}

TEST(BackgroundPanelTacview, AindaNaoInicializado)
{
   app::BackgroundInfo bg;
   bg.tacviewEnabled = true;
   bg.tacviewInitialized = false;

   EXPECT_NE(renderToText(bg).find("ainda nao inicializado"), std::string::npos);
}

TEST(BackgroundPanelTacview, ClienteConectadoAgora)
{
   app::BackgroundInfo bg;
   bg.tacviewEnabled = true;
   bg.tacviewInitialized = true;
   bg.tacviewListening = true;
   bg.tacviewConnected = true;

   EXPECT_NE(renderToText(bg).find("cliente conectado"), std::string::npos);
}

TEST(BackgroundPanelTacview, ClienteJaConectouECaiu)
{
   app::BackgroundInfo bg;
   bg.tacviewEnabled = true;
   bg.tacviewInitialized = true;
   bg.tacviewListening = true;
   bg.tacviewConnected = false;
   bg.tacviewConnections = 3;

   // A coluna tem largura FIXA (kColumnWidth em BackgroundPanel.cpp) --
   // o final da frase e recortado nesse ponto, entao o substring afirmado
   // fica dentro do orcamento visivel (kColumnWidth - kColKey caracteres).
   EXPECT_NE(renderToText(bg).find("cliente ja conectou"), std::string::npos);
}

TEST(BackgroundPanelTacview, EscutandoSemNinguemTerConectado)
{
   app::BackgroundInfo bg;
   bg.tacviewEnabled = true;
   bg.tacviewInitialized = true;
   bg.tacviewListening = true;
   bg.tacviewConnected = false;
   bg.tacviewConnections = 0;

   EXPECT_NE(renderToText(bg).find("ninguem conectou ainda"), std::string::npos);
}

TEST(BackgroundPanelTacview, GravandoMostraSoONomeDoArquivoNaoOCaminhoInteiro)
{
   app::BackgroundInfo bg;
   bg.tacviewEnabled = true;
   bg.tacviewInitialized = true;
   bg.tacviewListening = true;
   bg.tacviewRecording = true;
   bg.tacviewFile = "./app/data/recordings/mission.acmi";

   const std::string text{renderToText(bg)};
   EXPECT_NE(text.find("mission.acmi"), std::string::npos);
   EXPECT_EQ(text.find("recordings"), std::string::npos);
}

TEST(BackgroundPanelTacview, SemGravacaoMostraNenhuma)
{
   app::BackgroundInfo bg;
   bg.tacviewEnabled = true;
   bg.tacviewInitialized = true;
   bg.tacviewListening = true;
   bg.tacviewRecording = false;

   EXPECT_NE(renderToText(bg).find("gravacao"), std::string::npos);
   EXPECT_EQ(renderToText(bg).find("mission.acmi"), std::string::npos);
}

TEST(BackgroundPanelTacview, BytesEnviadosFormatamNaUnidadeCerta)
{
   app::BackgroundInfo base;
   base.tacviewEnabled = true;
   base.tacviewInitialized = true;
   base.tacviewListening = true;

   app::BackgroundInfo pequeno{base};
   pequeno.tacviewBytesSent = 500;
   EXPECT_NE(renderToText(pequeno).find("500 B"), std::string::npos);

   app::BackgroundInfo kib{base};
   kib.tacviewBytesSent = 2048;
   EXPECT_NE(renderToText(kib).find("2.0 KiB"), std::string::npos);

   app::BackgroundInfo mib{base};
   mib.tacviewBytesSent = 3UL * 1024UL * 1024UL;
   EXPECT_NE(renderToText(mib).find("3.0 MiB"), std::string::npos);
}

//------------------------------------------------------------------------------
// Secao Mundo / Rede / Processo
//------------------------------------------------------------------------------
TEST(BackgroundPanelWorld, TerrenoCarregadoOuNao)
{
   app::BackgroundInfo bg;
   bg.terrainLoaded = true;
   EXPECT_NE(renderToText(bg).find("carregado"), std::string::npos);

   bg.terrainLoaded = false;
   EXPECT_NE(renderToText(bg).find("nao carregado"), std::string::npos);
}

TEST(BackgroundPanelNetwork, SemHandlersMostraCenarioHermetico)
{
   const app::BackgroundInfo bg;   // networkHandlerCount == 0 por default
   EXPECT_NE(renderToText(bg).find("cenario hermetico"), std::string::npos);
}

TEST(BackgroundPanelNetwork, ComHandlersMostraATaxaDeclarada)
{
   app::BackgroundInfo bg;
   bg.networkHandlerCount = 2;
   bg.networkRateHz = 5.0;

   EXPECT_NE(renderToText(bg).find("5.0 Hz"), std::string::npos);
}

TEST(BackgroundPanelProcess, OmiteASecaoQuandoResidentKbEhZero)
{
   const app::BackgroundInfo bg;   // residentKb == 0 por default
   EXPECT_EQ(renderToText(bg).find("residentes"), std::string::npos);
}

TEST(BackgroundPanelProcess, MostraMemoriaResidenteEmMiB)
{
   app::BackgroundInfo bg;
   bg.residentKb = 204800;   // 200 MiB

   EXPECT_NE(renderToText(bg).find("200 MiB residentes"), std::string::npos);
}
