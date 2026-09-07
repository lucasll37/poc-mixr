#include "app/FleetPanel.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"

#include <gtest/gtest.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <string>

// app/FleetPanel.cpp -- o vocabulario visual comum as abas Players e Mapa
// (cor/rotulo de lado, glifo de especie, rotulo/cor de modo, cor de
// comportamento) e a formatacao das linhas/detalhe. Ate agora este arquivo
// so era linkado (por test_map_canvas_fit.cpp/test_component_tree_layout.cpp,
// para satisfazer o link de MapPanel.cpp/ComponentTreeQuery.cpp) sem NENHUMA
// asserção sobre a logica dele -- nem os mapeamentos de cor/rotulo, nem a
// formatacao condicional dos campos exclusivos de AirVehicle.

using mixr::models::Player;
using mixr::simulation::AbstractPlayer;

namespace {

app::EntityState makeEntity()
{
   app::EntityState e;
   e.id = 1;
   e.name = "falcon1";
   e.typeLabel = "C310";
   e.majorType = static_cast<unsigned int>(Player::AIR_VEHICLE);
   e.side = Player::BLUE;
   e.mode = static_cast<int>(AbstractPlayer::ACTIVE);
   e.altitudeM = 1500.0;
   e.speedKts = 250.0;
   e.behaviorLabel = "EVADE";
   e.threadTag = 2;
   return e;
}

std::string renderToText(const ftxui::Element& element, const int w = 160, const int h = 40)
{
   auto screen{ftxui::Screen::Create(ftxui::Dimension::Fixed(w), ftxui::Dimension::Fixed(h))};
   ftxui::Render(screen, element);
   return screen.ToString();
}

} // namespace

//------------------------------------------------------------------------------
// sideColor()/sideLabel()
//------------------------------------------------------------------------------
TEST(FleetPanelSide, RotuloECorPorLadoConhecido)
{
   EXPECT_EQ(app::sideLabel(Player::BLUE), "BLUE");
   EXPECT_EQ(app::sideColor(Player::BLUE), ftxui::Color::CyanLight);

   EXPECT_EQ(app::sideLabel(Player::RED), "RED");
   EXPECT_EQ(app::sideColor(Player::RED), ftxui::Color::Red);

   EXPECT_EQ(app::sideLabel(Player::YELLOW), "YELLOW");
   EXPECT_EQ(app::sideColor(Player::YELLOW), ftxui::Color::Yellow);

   EXPECT_EQ(app::sideLabel(Player::CYAN), "CYAN");
   EXPECT_EQ(app::sideColor(Player::CYAN), ftxui::Color::Cyan);

   EXPECT_EQ(app::sideLabel(Player::WHITE), "WHITE");
   EXPECT_EQ(app::sideColor(Player::WHITE), ftxui::Color::White);

   EXPECT_EQ(app::sideLabel(Player::GRAY), "GRAY");
   EXPECT_EQ(app::sideColor(Player::GRAY), ftxui::Color::GrayDark);
}

TEST(FleetPanelSide, LadoDesconhecidoCaiNoFallbackCinza)
{
   const unsigned int ladoInvalido{9999u};
   EXPECT_EQ(app::sideLabel(ladoInvalido), "GRAY");
   EXPECT_EQ(app::sideColor(ladoInvalido), ftxui::Color::GrayDark);
}

//------------------------------------------------------------------------------
// majorTypeGlyph()
//------------------------------------------------------------------------------
TEST(FleetPanelMajorType, UmGlifoPorEspecie)
{
   EXPECT_EQ(app::majorTypeGlyph(static_cast<unsigned int>(Player::AIR_VEHICLE)), "A");
   EXPECT_EQ(app::majorTypeGlyph(static_cast<unsigned int>(Player::WEAPON)), "W");
   EXPECT_EQ(app::majorTypeGlyph(static_cast<unsigned int>(Player::GROUND_VEHICLE)), "G");
   EXPECT_EQ(app::majorTypeGlyph(static_cast<unsigned int>(Player::SHIP)), "S");
   EXPECT_EQ(app::majorTypeGlyph(static_cast<unsigned int>(Player::SPACE_VEHICLE)), "X");
   EXPECT_EQ(app::majorTypeGlyph(static_cast<unsigned int>(Player::BUILDING)), "B");
   EXPECT_EQ(app::majorTypeGlyph(static_cast<unsigned int>(Player::LIFE_FORM)), "L");
}

TEST(FleetPanelMajorType, SemNenhumBitConhecidoDevolveInterrogacao)
{
   EXPECT_EQ(app::majorTypeGlyph(0u), "?");
}

TEST(FleetPanelMajorType, BitsCombinadosSeguemAOrdemDePrioridadeDoCodigo)
{
   const unsigned int combinado{static_cast<unsigned int>(Player::AIR_VEHICLE)
                                | static_cast<unsigned int>(Player::WEAPON)};
   // AIR_VEHICLE e testado primeiro no if-chain -- vence mesmo com o bit de
   // WEAPON tambem ligado.
   EXPECT_EQ(app::majorTypeGlyph(combinado), "A");
}

//------------------------------------------------------------------------------
// modeLabel()/modeColor()
//------------------------------------------------------------------------------
TEST(FleetPanelMode, RotuloECorPorModoConhecido)
{
   EXPECT_EQ(app::modeLabel(static_cast<int>(AbstractPlayer::INACTIVE)), "inativo");
   EXPECT_EQ(app::modeColor(static_cast<int>(AbstractPlayer::INACTIVE)), ftxui::Color::GrayDark);

   EXPECT_EQ(app::modeLabel(static_cast<int>(AbstractPlayer::ACTIVE)), "ativo");
   EXPECT_EQ(app::modeColor(static_cast<int>(AbstractPlayer::ACTIVE)), ftxui::Color::Green);

   EXPECT_EQ(app::modeLabel(static_cast<int>(AbstractPlayer::PRE_RELEASE)), "pre-liberacao");
   EXPECT_EQ(app::modeColor(static_cast<int>(AbstractPlayer::PRE_RELEASE)), ftxui::Color::Yellow);

   // As tres condicoes de "morto" -- KILLED/CRASHED/DETONATED -- e
   // DELETE_REQUEST caem todas no mesmo vermelho (default do switch).
   EXPECT_EQ(app::modeLabel(static_cast<int>(AbstractPlayer::KILLED)), "abatido");
   EXPECT_EQ(app::modeColor(static_cast<int>(AbstractPlayer::KILLED)), ftxui::Color::Red);
   EXPECT_EQ(app::modeLabel(static_cast<int>(AbstractPlayer::CRASHED)), "destruido");
   EXPECT_EQ(app::modeColor(static_cast<int>(AbstractPlayer::CRASHED)), ftxui::Color::Red);
   EXPECT_EQ(app::modeLabel(static_cast<int>(AbstractPlayer::DETONATED)), "detonado");
   EXPECT_EQ(app::modeColor(static_cast<int>(AbstractPlayer::DETONATED)), ftxui::Color::Red);
   EXPECT_EQ(app::modeLabel(static_cast<int>(AbstractPlayer::DELETE_REQUEST)), "removendo");
   EXPECT_EQ(app::modeColor(static_cast<int>(AbstractPlayer::DELETE_REQUEST)), ftxui::Color::Red);
}

TEST(FleetPanelMode, ModoDesconhecidoVirouInterrogacaoEVermelho)
{
   EXPECT_EQ(app::modeLabel(999999), "?");
   EXPECT_EQ(app::modeColor(999999), ftxui::Color::Red);
}

//------------------------------------------------------------------------------
// behaviorColor()
//------------------------------------------------------------------------------
TEST(FleetPanelBehaviorColor, TracoEhSempreCinza)
{
   EXPECT_EQ(app::behaviorColor("--"), ftxui::Color::GrayDark);
}

TEST(FleetPanelBehaviorColor, EhDeterministicoEDiferenteDeCinzaParaRotuloReal)
{
   const ftxui::Color c1{app::behaviorColor("PATROL")};
   const ftxui::Color c2{app::behaviorColor("PATROL")};
   EXPECT_EQ(c1, c2);
   EXPECT_NE(c1, ftxui::Color::GrayDark);
}

//------------------------------------------------------------------------------
// entityRowText()
//------------------------------------------------------------------------------
TEST(EntityRowText, ContemOsCamposPrincipaisDaLinha)
{
   app::EntityState e{makeEntity()};
   e.hasFuel = true;
   e.fuelFrac = 0.42;

   const std::string text{app::entityRowText(e)};
   EXPECT_NE(text.find("falcon1"), std::string::npos);
   EXPECT_NE(text.find("C310"), std::string::npos);
   EXPECT_NE(text.find("EVADE"), std::string::npos);
   EXPECT_NE(text.find("T2"), std::string::npos);
   EXPECT_NE(text.find("1500"), std::string::npos);
   EXPECT_NE(text.find("250"), std::string::npos);
   EXPECT_NE(text.find("42%"), std::string::npos);
}

TEST(EntityRowText, SemThreadTagMostraTraco)
{
   app::EntityState e{makeEntity()};
   e.threadTag = -1;
   EXPECT_NE(app::entityRowText(e).find("-"), std::string::npos);
   EXPECT_EQ(app::entityRowText(e).find("T-1"), std::string::npos);
}

TEST(EntityRowText, SemCombustivelOmiteOCampoDePorcentagem)
{
   app::EntityState e{makeEntity()};
   e.hasFuel = false;
   EXPECT_EQ(app::entityRowText(e).find('%'), std::string::npos);
}

//------------------------------------------------------------------------------
// renderEntityRow()/renderEntityListHeader() -- renderizados de verdade,
// texto lido de volta via Screen::ToString().
//------------------------------------------------------------------------------
TEST(RenderEntityRow, MostraORotuloDeModoQuandoDiferenteDeAtivo)
{
   app::EntityState e{makeEntity()};
   e.mode = static_cast<int>(AbstractPlayer::CRASHED);

   const std::string text{renderToText(app::renderEntityRow(e, false))};
   EXPECT_NE(text.find("destruido"), std::string::npos);
}

TEST(RenderEntityRow, NaoMostraORotuloDeModoQuandoAtivo)
{
   app::EntityState e{makeEntity()};
   e.mode = static_cast<int>(AbstractPlayer::ACTIVE);

   const std::string text{renderToText(app::renderEntityRow(e, false))};
   EXPECT_EQ(text.find("ativo"), std::string::npos);
   EXPECT_EQ(text.find("destruido"), std::string::npos);
}

TEST(RenderEntityListHeader, ContemUmRotuloPorColuna)
{
   const std::string text{renderToText(app::renderEntityListHeader())};
   for (const std::string coluna : {"nome", "tipo", "bt", "thread", "altitude", "vel(kt)", "combust."}) {
      EXPECT_NE(text.find(coluna), std::string::npos) << "coluna ausente: " << coluna;
   }
}

//------------------------------------------------------------------------------
// renderEntityDetail() -- os campos condicionais (fuel/g/thrust/track/alert)
// so aparecem quando o 'has*' correspondente esta ligado.
//------------------------------------------------------------------------------
TEST(RenderEntityDetail, OmiteCamposOpcionaisQuandoAusentes)
{
   const app::EntityState e{makeEntity()};   // has* todos falsos por default
   const std::string text{renderToText(app::renderEntityDetail(e))};

   EXPECT_EQ(text.find("pista="), std::string::npos);
   EXPECT_EQ(text.find("alerta<-"), std::string::npos);
   EXPECT_EQ(text.find("fuel"), std::string::npos);
}

TEST(RenderEntityDetail, MostraPistaEAlertaQuandoPresentes)
{
   app::EntityState e{makeEntity()};
   e.hasTrack = true;
   e.trackName = "bandit1";
   e.trackRangeNm = 12.3;
   e.hasAlert = true;
   e.alertSender = "falcon2";
   e.alertContact = "bandit1";

   const std::string text{renderToText(app::renderEntityDetail(e))};
   EXPECT_NE(text.find("bandit1@12.3NM"), std::string::npos);
   EXPECT_NE(text.find("alerta<-falcon2(bandit1)"), std::string::npos);
}

TEST(RenderEntityDetail, MostraCombustivelQuandoPresente)
{
   app::EntityState e{makeEntity()};
   e.hasFuel = true;
   e.fuelFrac = 0.5;

   const std::string text{renderToText(app::renderEntityDetail(e))};
   EXPECT_NE(text.find("50%"), std::string::npos);
}
