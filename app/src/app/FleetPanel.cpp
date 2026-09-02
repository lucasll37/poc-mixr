#include "app/FleetPanel.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;

std::string formatFixed(const double v, const int precision, const int width)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(precision) << std::setw(width) << v;
   return oss.str();
}

Element kv(const std::string& k, const std::string& v)
{
   return hbox({text(k) | dim, text(v)});
}

// FNV-1a de 32 bits -- so precisa ser estavel DENTRO de uma execucao, nao
// entre execucoes (ver o comentario de behaviorColor() no header).
std::uint32_t fnv1a(const std::string& s)
{
   std::uint32_t h{2166136261u};
   for (const char c : s) { h ^= static_cast<unsigned char>(c); h *= 16777619u; }
   return h;
}

const Color kPalette[]{
   Color::Green, Color::Cyan, Color::Yellow, Color::Magenta,
   Color::Blue, Color::Red, Color::GreenLight, Color::CyanLight,
};
const int kPaletteSize{static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]))};

// -1 = "nao se aplica" (xboard::Readout::threadTag, ver Board.hpp) -- so
// faz sentido pra quem decide no pool de tempo critico (a multi-thread; na
// single-thread a decisao roda no laco de background, fora do pool).
std::string threadTagText(const int threadTag)
{
   return (threadTag >= 0) ? ("T" + std::to_string(threadTag)) : "-";
}

}

Color sideColor(const unsigned int side)
{
   using mixr::models::Player;
   switch (side) {
      case Player::BLUE:   return Color::CyanLight;
      case Player::RED:    return Color::Red;
      case Player::YELLOW: return Color::Yellow;
      case Player::CYAN:   return Color::Cyan;
      case Player::WHITE:  return Color::White;
      case Player::GRAY:
      default:             return Color::GrayDark;
   }
}

std::string sideLabel(const unsigned int side)
{
   using mixr::models::Player;
   switch (side) {
      case Player::BLUE:   return "BLUE";
      case Player::RED:    return "RED";
      case Player::YELLOW: return "YELLOW";
      case Player::CYAN:   return "CYAN";
      case Player::WHITE:  return "WHITE";
      case Player::GRAY:
      default:             return "GRAY";
   }
}

std::string majorTypeGlyph(const unsigned int majorType)
{
   using mixr::models::Player;
   if (majorType & static_cast<unsigned int>(Player::AIR_VEHICLE))    return "A";
   if (majorType & static_cast<unsigned int>(Player::WEAPON))         return "W";
   if (majorType & static_cast<unsigned int>(Player::GROUND_VEHICLE)) return "G";
   if (majorType & static_cast<unsigned int>(Player::SHIP))           return "S";
   if (majorType & static_cast<unsigned int>(Player::SPACE_VEHICLE))  return "X";
   if (majorType & static_cast<unsigned int>(Player::BUILDING))       return "B";
   if (majorType & static_cast<unsigned int>(Player::LIFE_FORM))      return "L";
   return "?";
}

std::string modeLabel(const int mode)
{
   using mixr::simulation::AbstractPlayer;
   switch (mode) {
      case AbstractPlayer::INACTIVE:       return "inativo";
      case AbstractPlayer::ACTIVE:         return "ativo";
      case AbstractPlayer::KILLED:         return "abatido";
      case AbstractPlayer::CRASHED:        return "destruido";
      case AbstractPlayer::DETONATED:      return "detonado";
      case AbstractPlayer::PRE_RELEASE:    return "pre-liberacao";
      case AbstractPlayer::DELETE_REQUEST: return "removendo";
      default:                             return "?";
   }
}

Color modeColor(const int mode)
{
   using mixr::simulation::AbstractPlayer;
   switch (mode) {
      case AbstractPlayer::ACTIVE:      return Color::Green;
      case AbstractPlayer::PRE_RELEASE: return Color::Yellow;
      case AbstractPlayer::INACTIVE:    return Color::GrayDark;
      default:                          return Color::Red;   // dead conditions
   }
}

Color behaviorColor(const std::string& label)
{
   if (label == "--") return Color::GrayDark;
   return kPalette[fnv1a(label) % static_cast<std::uint32_t>(kPaletteSize)];
}

std::string entityRowText(const EntityState& e)
{
   std::ostringstream oss;
   oss << majorTypeGlyph(e.majorType) << " " << std::left << std::setw(kColName) << e.name
       << std::setw(kColType) << e.typeLabel
       << std::setw(kColBehavior) << e.behaviorLabel
       << std::setw(kColThread) << threadTagText(e.threadTag)
       << std::setw(kColAlt) << (formatFixed(e.altitudeM, 0, 5) + "m")
       << std::setw(kColSpd) << (formatFixed(e.speedKts, 0, 4) + "kt");
   if (e.hasFuel) oss << std::setw(kColFuel) << (formatFixed(e.fuelFrac * 100.0, 0, 3) + "%");
   return oss.str();
}

// Cada campo ocupa uma coluna de LARGURA FIXA ('size(WIDTH, EQUAL, N)') --
// e o que garante alinhamento de verdade entre linhas (pedido explicito),
// diferente de so preencher a string com espacos: aqui a largura e do
// ELEMENTO, entao sobrevive a nomes/valores mais compridos ou mais curtos
// sem desalinhar a coluna seguinte.
Element renderEntityRow(const EntityState& e, const bool focused)
{
   Elements row{
      text(" " + majorTypeGlyph(e.majorType) + " ") | bgcolor(sideColor(e.side)) | color(Color::Black)
         | size(WIDTH, EQUAL, kColBadge),
      text(" " + e.name) | (focused ? bold : nothing) | size(WIDTH, EQUAL, kColName),
      text(e.typeLabel) | dim | size(WIDTH, EQUAL, kColType),
      text(" " + e.behaviorLabel + " ") | bgcolor(behaviorColor(e.behaviorLabel)) | color(Color::Black)
         | size(WIDTH, EQUAL, kColBehavior),
      text(threadTagText(e.threadTag)) | dim | size(WIDTH, EQUAL, kColThread),
      text(formatFixed(e.altitudeM, 0, 5) + "m") | size(WIDTH, EQUAL, kColAlt),
      text(formatFixed(e.speedKts, 0, 4) + "kt") | size(WIDTH, EQUAL, kColSpd),
   };
   if (e.hasFuel) {
      row.push_back(text(formatFixed(e.fuelFrac * 100.0, 0, 3) + "%")
                    | color(e.fuelFrac < 0.35 ? Color::Red : Color::GrayDark)
                    | size(WIDTH, EQUAL, kColFuel));
   }
   if (e.mode != static_cast<int>(mixr::simulation::AbstractPlayer::ACTIVE)) {
      row.push_back(text(" [" + modeLabel(e.mode) + "]") | color(modeColor(e.mode)));
   }
   return hbox(std::move(row)) | (focused ? inverted : nothing);
}

Element renderEntityListHeader()
{
   return hbox({
             text("") | size(WIDTH, EQUAL, kColBadge),
             text(" nome") | dim | size(WIDTH, EQUAL, kColName),
             text("tipo") | dim | size(WIDTH, EQUAL, kColType),
             text("bt") | dim | size(WIDTH, EQUAL, kColBehavior),
             text("thread") | dim | size(WIDTH, EQUAL, kColThread),
             text("altitude") | dim | size(WIDTH, EQUAL, kColAlt),
             text("vel(kt)") | dim | size(WIDTH, EQUAL, kColSpd),
             text("combust.") | dim | size(WIDTH, EQUAL, kColFuel),
          });
}

Element renderEntityDetail(const EntityState& e)
{
   const Color badge{behaviorColor(e.behaviorLabel)};

   Elements lines;
   lines.push_back(hbox({
      text(" " + majorTypeGlyph(e.majorType) + " ") | bgcolor(sideColor(e.side)) | color(Color::Black),
      text(" " + e.name) | bold,
      text(" (" + e.typeLabel + ")") | dim,
      filler(),
      text(" " + e.behaviorLabel + " ") | bgcolor(badge) | color(Color::Black) | bold,
   }));
   lines.push_back(separator());

   if (e.mode != static_cast<int>(mixr::simulation::AbstractPlayer::ACTIVE)) {
      lines.push_back(text(modeLabel(e.mode)) | color(modeColor(e.mode)) | bold);
   }

   // A folha atual NAO repete aqui -- o subquadro da arvore, logo abaixo
   // (ver DashboardLoop.cpp::buildDetailPanel), ja mostra ela destacada.
   // Repetir como texto solto era informacao redundante.

   lines.push_back(hbox({
      kv("alt ", formatFixed(e.altitudeM, 0, 5) + "m") | flex,
      kv("agl ", formatFixed(e.altitudeAglM, 0, 5) + "m") | flex,
   }));
   lines.push_back(hbox({
      kv("hdg ", formatFixed(e.headingDeg, 0, 3) + "deg") | flex,
      kv("roll ", formatFixed(e.rollDeg, 0, 4) + "deg") | flex,
   }));
   lines.push_back(hbox({
      kv("spd ", formatFixed(e.speedKts, 0, 3) + "kt") | flex,
      kv("mach ", formatFixed(e.machNum, 2, 4)) | flex,
   }));
   lines.push_back(hbox({
      kv("n ", formatFixed(e.northM, 0, 6) + "m") | flex,
      kv("e ", formatFixed(e.eastM, 0, 6) + "m") | flex,
   }));

   if (e.hasGload || e.hasThrust) {
      lines.push_back(hbox({
         kv("g ", e.hasGload ? formatFixed(e.gLoad, 1, 4) : "--") | flex,
         kv("thrust ", e.hasThrust ? (formatFixed(e.thrustLb, 0, 5) + "lb") : "--") | flex,
      }));
   }

   if (e.hasFuel) {
      lines.push_back(hbox({
         text("fuel ") | dim,
         gauge(static_cast<float>(e.fuelFrac))
            | color(e.fuelFrac < 0.35 ? Color::Red : (e.fuelFrac < 0.5 ? Color::Yellow : Color::Green))
            | flex,
         text(" " + formatFixed(e.fuelFrac * 100.0, 0, 3) + "%"),
      }));
   }

   if (e.hasTrack) {
      std::ostringstream oss;
      oss << "pista=" << e.trackName << "@" << std::fixed << std::setprecision(1)
          << e.trackRangeNm << "NM";
      lines.push_back(text(oss.str()) | color(Color::YellowLight));
   }
   if (e.hasAlert) {
      lines.push_back(text("alerta<-" + e.alertSender + "(" + e.alertContact + ")")
                      | color(Color::RedLight));
   }

   // "decisoes" = quantas decisoes da arvore/UBF ja foram efetivamente
   // ATUADAS nesta entidade (nao candidaturas -- ver shared/xboard/
   // Board.hpp). "thread" saiu do card -- mudou pra coluna da LISTA (F1,
   // renderEntityRow()), com o cabecalho explicando a coluna
   // (renderEntityListHeader()); nao faz sentido repetir aqui.
   lines.push_back(kv("decisoes ", std::to_string(e.decisions)));

   return vbox(std::move(lines)) | border | color(badge);
}

} // namespace app
