#include "app/LogPanel.hpp"

#include <iomanip>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;
using mixr::xlog::Level;

// Cor por nivel -- a mesma escala de gravidade que o resto do painel ja
// usa (cinza = ruido, amarelo = atencao, vermelho = erro), pra nao inventar
// vocabulario visual novo so aqui.
Color levelColor(const Level level)
{
   switch (level) {
      case Level::DEBUG:   return Color::GrayDark;
      case Level::INFO:    return Color::Cyan;
      case Level::WARNING: return Color::Yellow;
      case Level::ERROR:   return Color::Red;
   }
   return Color::White;
}

std::string seqText(const std::uint64_t seq)
{
   std::ostringstream oss;
   oss << "#" << seq;
   return oss.str();
}

} // namespace

bool passesLevelFilter(const Level level, const Level minLevel)
{
   return static_cast<int>(level) >= static_cast<int>(minLevel);
}

Level nextLevelFilter(const Level minLevel)
{
   switch (minLevel) {
      case Level::DEBUG:   return Level::INFO;
      case Level::INFO:    return Level::WARNING;
      case Level::WARNING: return Level::ERROR;
      case Level::ERROR:   return Level::DEBUG;
   }
   return Level::DEBUG;
}

std::string logRowText(const mixr::xlog::Entry& e)
{
   return seqText(e.seq) + " " + e.time + " " + mixr::xlog::levelName(e.level) + " " + e.text;
}

Element renderLogRow(const mixr::xlog::Entry& e, const bool focused)
{
   return hbox({
             text(seqText(e.seq)) | dim | size(WIDTH, EQUAL, kColLogSeq),
             text(e.time) | dim | size(WIDTH, EQUAL, kColLogTime),
             // O nivel e um badge de fundo colorido (nao so texto colorido)
             // pela mesma razao do badge de comportamento na aba Players:
             // e o campo que se procura ao varrer a lista com o olho.
             text(" " + std::string(mixr::xlog::levelName(e.level)) + " ")
                | bgcolor(levelColor(e.level)) | color(Color::Black)
                | size(WIDTH, EQUAL, kColLogLevel),
             text(" " + e.text) | (focused ? bold : nothing),
          })
          | (focused ? inverted : nothing);
}

Element renderLogListHeader()
{
   return hbox({
             text("seq") | dim | size(WIDTH, EQUAL, kColLogSeq),
             text("hora") | dim | size(WIDTH, EQUAL, kColLogTime),
             text("nivel") | dim | size(WIDTH, EQUAL, kColLogLevel),
             text(" mensagem") | dim,
          });
}

} // namespace app
