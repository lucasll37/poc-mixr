#include "app/BackgroundPanel.hpp"

#include <iomanip>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;

std::string fmt1(const double v)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(1) << v;
   return oss.str();
}

Element kv(const std::string& k, const std::string& v, const Color valueColor = Color::Default)
{
   return hbox({text(k) | dim, text(v) | color(valueColor)});
}

Element section(const std::string& title, Elements rows)
{
   Elements body{text(title) | bold | color(Color::CyanLight)};
   for (auto& r : rows) body.push_back(std::move(r));
   body.push_back(text(""));
   return vbox(std::move(body));
}

}

Element renderBackgroundPanel(const BackgroundInfo& bg)
{
   Elements parts;

   parts.push_back(text(" Thread de Tempo Nao-Critico ") | bold | bgcolor(Color::Blue)
                   | color(Color::White));
   parts.push_back(separator());

   parts.push_back(paragraphAlignLeft(
      "Este app nunca cria a thread de background NATIVA do MIXR "
      "(Station::createBackgroundProcess()) -- quem faz esse papel e o "
      "proprio laco que atualiza esta interface, chamando "
      "station->updateData(dt) fora do frame de tempo critico. E aqui que "
      "o gravador e drenado para o Tacview, a elevacao de terreno de cada "
      "player e atualizada, a rede DIS seria processada se o cenario "
      "declarasse 'networks:', e onde as outras tres abas sao amostradas."
      ) | dim);
   parts.push_back(text(""));

   parts.push_back(section("Laco de atualizacao", {
      kv("taxa alvo      ", std::to_string(bg.targetHz) + " Hz"),
      kv("taxa medida    ", fmt1(bg.measuredHz) + " Hz",
         (bg.measuredHz < static_cast<double>(bg.targetHz) * 0.8) ? Color::Yellow : Color::Green),
      kv("ultima iteracao", fmt1(bg.lastIterationMs) + " ms"),
      kv("iteracoes      ", std::to_string(bg.iterationCount)),
   }));

   parts.push_back(section("Tacview / gravador", {
      kv("exportacao ", bg.tacviewEnabled ? "ligada" : "desligada",
         bg.tacviewEnabled ? Color::Green : Color::GrayDark),
      kv("varreduras de radar publicadas ", std::to_string(bg.radarScanPushCount)),
   }));

   parts.push_back(section("Terreno", {
      kv("banco de elevacao ", bg.terrainLoaded ? "carregado" : "nao carregado",
         bg.terrainLoaded ? Color::Green : Color::GrayDark),
   }));

   parts.push_back(section("Rede (DIS)", {
      bg.networkHandlerCount > 0
         ? kv("handlers ", std::to_string(bg.networkHandlerCount)
              + " (" + fmt1(bg.networkRateHz) + " Hz)", Color::Yellow)
         : kv("handlers ", "nenhum -- cenario hermetico (sem 'networks:')", Color::GrayDark),
   }));

   return vbox(std::move(parts)) | border;
}

} // namespace app
