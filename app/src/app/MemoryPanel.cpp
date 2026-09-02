#include "app/MemoryPanel.hpp"

#include "ftxui/dom/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;

// Gradiente verde -> amarelo -> vermelho conforme a fracao de 'pico' ja
// ocupada -- muito mais ilustrativo que uma cor so (ciano ou vermelho) o
// tempo inteiro. Suspeita de vazamento forca vermelho, independente da
// fracao (o alerta importa mais que o nivel exato).
Color gradientColor(const float fraction, const bool suspectedLeak)
{
   if (suspectedLeak) return Color::Red;
   const float t{std::clamp(fraction, 0.0f, 1.0f)};
   if (t < 0.5f) {
      const float u{t / 0.5f};
      return Color::RGB(static_cast<std::uint8_t>(u * 220), 200, 0);
   }
   const float u{(t - 0.5f) / 0.5f};
   return Color::RGB(220, static_cast<std::uint8_t>((1.0f - u) * 200), 0);
}

// A barra em si -- um Canvas (nao ftxui::gauge()) porque da pra desenhar
// DUAS coisas na mesma faixa: o nivel ATUAL (preenchimento em gradiente) E
// uma marca de TENDENCIA (onde 'count' estava no inicio da janela
// deslizante, ~3s atras) -- um trecinho sobe ou desce em relacao a marca
// mostra a direcao sem precisar de um grafico separado.
Element renderMemoryBar(const ClassStat& s)
{
   const int widthPx{kColBar * 2};
   auto c = Canvas(widthPx, 4);

   const float fraction{s.mc > 0
      ? std::clamp(static_cast<float>(s.count) / static_cast<float>(s.mc), 0.0f, 1.0f)
      : 0.0f};
   const int filledPx{static_cast<int>(std::lround(fraction * (widthPx - 1)))};
   const Color barColor{gradientColor(fraction, s.suspectedLeak)};

   if (filledPx > 0) {
      c.DrawBlockLine(0, 0, filledPx, 0, barColor);
      c.DrawBlockLine(0, 2, filledPx, 2, barColor);
   }

   if (!s.countHistory.empty() && s.mc > 0) {
      const float oldFraction{std::clamp(
         static_cast<float>(s.countHistory.front()) / static_cast<float>(s.mc), 0.0f, 1.0f)};
      const int oldPx{static_cast<int>(std::lround(oldFraction * (widthPx - 1)))};
      c.DrawBlockLine(oldPx, 0, oldPx, 3, Color::White);
   }

   return canvas(std::move(c)) | size(WIDTH, EQUAL, kColBar);
}
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
   Elements row{
      text(s.fromPlugin ? " P " : " H ") | dim | size(WIDTH, EQUAL, kColSource),
      text(s.factoryName) | (focused ? bold : nothing) | size(WIDTH, EQUAL, kColFactory),
      text("count=" + std::to_string(s.count)) | size(WIDTH, EQUAL, kColCount),
      text("pico=" + std::to_string(s.mc)) | size(WIDTH, EQUAL, kColPeak),
      text("criados=" + std::to_string(s.tc)) | size(WIDTH, EQUAL, kColCreated),
      renderMemoryBar(s),
   };
   if (s.suspectedLeak) {
      row.push_back(text("  CRESCENDO") | color(Color::Red) | bold);
   }
   return hbox(std::move(row)) | (focused ? inverted : nothing);
}

} // namespace app
