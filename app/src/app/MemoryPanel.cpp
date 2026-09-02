#include "app/MemoryPanel.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;
}

std::string classRowText(const ClassStat& s)
{
   std::ostringstream oss;
   oss << (s.fromPlugin ? "[plugin] " : "[host]   ") << std::left << std::setw(kColFactory) << s.factoryName
       << " count=" << s.count << " mc=" << s.mc << " tc=" << s.tc
       << (s.suspectedLeak ? "  CRESCENDO" : "");
   return oss.str();
}

// Mesmo raciocinio de app/FleetPanel.cpp::renderEntityRow -- cada campo em
// 'size(WIDTH, EQUAL, N)', pra alinhar de verdade em coluna, nao so
// preencher a string com espacos.
//
// A barra (no lugar do grafico de linha de antes) usa 'pico' (s.mc) como
// teto -- e o mesmo numero que ja aparece na coluna "pico=", e cresce
// sozinho conforme mais instancias existem ao mesmo tempo (nunca encolhe,
// e o MetaObject nativo quem mantem). E por isso que a escala "varia com o
// tempo": nao ha limite fixo conhecido de antemao, o teto e o proprio
// recorde ja visto.
Element renderClassRow(const ClassStat& s, const bool focused)
{
   const float fraction{s.mc > 0 ? static_cast<float>(s.count) / static_cast<float>(s.mc) : 0.0f};
   const Color barColor{s.suspectedLeak ? Color::Red : Color::Cyan};

   Elements row{
      text(s.fromPlugin ? " P " : " H ") | dim | size(WIDTH, EQUAL, kColSource),
      text(s.factoryName) | (focused ? bold : nothing) | size(WIDTH, EQUAL, kColFactory),
      text("count=" + std::to_string(s.count)) | size(WIDTH, EQUAL, kColCount),
      text("pico=" + std::to_string(s.mc)) | size(WIDTH, EQUAL, kColPeak),
      text("criados=" + std::to_string(s.tc)) | size(WIDTH, EQUAL, kColCreated),
      gauge(fraction) | color(barColor) | size(WIDTH, EQUAL, kColBar),
   };
   if (s.suspectedLeak) {
      row.push_back(text("  CRESCENDO") | color(Color::Red) | bold);
   }
   return hbox(std::move(row)) | (focused ? inverted : nothing);
}

} // namespace app
