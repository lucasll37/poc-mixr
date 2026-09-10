#include "app/GrootMonitorCheck.hpp"

#include "app/Fleet.hpp"

#include "mixr/base/String.hpp"
#include "mixr/models/player/Player.hpp"

#include "xlog/Log.hpp"

#include <cstdlib>
#include <string>

namespace app {

void checkGrootMonitorTarget(mixr::models::WorldModel* const wm)
{
   const char* const target{std::getenv("MIXR_GROOT_MONITOR")};
   if (target == nullptr || target[0] == '\0') return;
   if (wm == nullptr) return;

   // discoverPlayers() e' a MESMA varredura que a aba "Players" do TUI usa --
   // generica sobre mixr::models::Player, sem lista de nomes e sem supor
   // AirVehicle (ver app/Fleet.hpp). Reusar aqui garante que a lista impressa
   // e' exatamente a que o cenario tem.
   std::string nomes;
   bool achou{false};
   for (const auto* const p : app::discoverPlayers(wm)) {
      if (p == nullptr) continue;
      const auto* const nm = p->getName();
      const std::string nome{(nm != nullptr) ? nm->getString() : ""};
      if (nome.empty()) continue;
      if (nome == target) achou = true;
      if (!nomes.empty()) nomes += " ";
      nomes += nome;
   }

   if (!achou) {
      LOG(WARNING) << "MIXR_GROOT_MONITOR=\"" << target << "\" nao casa com player NENHUM deste"
                   << " cenario -- o monitor do Groot nao vai ligar e nenhuma porta sera aberta."
                   << " Players do cenario: " << nomes;
      return;
   }

   // Medido: esta linha sai DEPOIS da do modelo -- o frame de aquecimento de
   // primeStation() ja constroi a arvore (e liga o monitor) antes daqui. Dai o
   // texto falar do LOG inteiro, e nao de "logo abaixo".
   LOG(INFO) << "MIXR_GROOT_MONITOR=\"" << target << "\": player encontrado no cenario. Se a linha"
             << " \"[BtBehavior] monitor do Groot ligado\" nao aparecer em lugar nenhum deste log, o MODELO desse"
             << " player nao implementa o hook do monitor (hoje so' o A-4/flight implementa).";
}

}  // namespace app
