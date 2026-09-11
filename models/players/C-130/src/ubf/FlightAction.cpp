#include "ubf/FlightAction.hpp"

#include "xboard/Board.hpp"
#include "xlog/Log.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/system/Autopilot.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/units/distance_utils.hpp"

#include <map>
#include <mutex>
#include <string>

namespace mixr {
namespace models {
namespace xC_130 {

namespace {

// Estado CONTINUO -> evento de BORDA, mesma tecnica de
// models/players/A-4/src/ubf/FlightAction.cpp: sem isso, logar toda transicao
// candidata enche o buffer de xlog (ate 50 Hz por aeronave) e afoga a linha
// que interessa. O mapa e estatico porque so ha uma aeronave neste modelo,
// mas o mutex fica de qualquer forma -- e o mesmo padrao, sem custo extra.
bool changedFor(std::map<int, std::string>& last, const int playerId, const std::string& key)
{
   static std::mutex mutex;
   const std::lock_guard<std::mutex> lock(mutex);
   const auto it = last.find(playerId);
   if (it != last.end() && it->second == key) return false;
   last[playerId] = key;
   return true;
}

} // namespace

IMPLEMENT_SUBCLASS(FlightAction, "C130FlightAction")
EMPTY_SLOTTABLE(FlightAction)
EMPTY_DELETEDATA(FlightAction)

FlightAction::FlightAction()
{
   STANDARD_CONSTRUCTOR()
}

void FlightAction::copyData(const FlightAction& org, const bool)
{
   BaseClass::copyData(org);
   command = org.command;
   label = org.label;
}

//------------------------------------------------------------------------------
// execute() -- atuacao, inteiramente sobre o Autopilot NATIVO, que por sua
// vez fala com o JSBSimModel (ap/heading_hold, ap/altitude_hold,
// ap/airspeed_hold -- ver data/jsbsim/aircraft/C130/c130ap.xml). Nenhuma lei
// de controle propria no caminho.
//
// GOTCHA DE UNIDADE: Autopilot::setCommandedAltitudeFt() e em PES, o resto
// deste modelo trabalha em metros. A conversao acontece aqui, na fronteira.
//
// As DUAS chamadas obrigatorias (xboard::setBehaviorLabel/bumpDecisionCount)
// sao a unica obrigacao do contrato que falha em SILENCIO -- sem elas, o
// dump/status mostram 'bt=--'/'dec=0' para sempre, sem erro em lugar nenhum.
//------------------------------------------------------------------------------
bool FlightAction::execute(base::Component* actor)
{
   const auto player = dynamic_cast<models::Player*>(actor);
   if (player == nullptr) return false;

   const char* const rawName{player->getName()->getString()};
   const std::string playerName{(rawName != nullptr) ? rawName : "?"};

   base::Pair* const pilotPair{player->getPilotByType(typeid(models::Autopilot))};
   const auto autopilot = (pilotPair != nullptr)
                           ? dynamic_cast<models::Autopilot*>(pilotPair->object())
                           : nullptr;
   if (autopilot == nullptr) {
      static std::map<int, std::string> reported;
      if (changedFor(reported, player->getID(), "sem-autopilot")) {
         LOG(ERROR) << "[FlightAction] " << playerName
                    << ": sem Autopilot -- decisao '" << label << "' nao pode ser atuada";
      }
      return false;
   }

   // Estado ANTERIOR do quadro, lido antes de sobrescrever -- permite logar
   // a TRANSICAO (evento raro) em vez do comportamento corrente (ate 50 Hz).
   const xboard::Readout before{xboard::get(player->getID())};

   autopilot->setHeadingHoldMode(true);
   autopilot->setAltitudeHoldMode(true);
   autopilot->setVelocityHoldMode(true);

   autopilot->setCommandedHeadingD(command.headingDeg);
   autopilot->setCommandedAltitudeFt(command.altitudeM * base::distance::M2FT);
   autopilot->setCommandedVelocityKts(command.speedKts);

   xboard::setBehaviorLabel(player->getID(), label);
   xboard::bumpDecisionCount(player->getID());
   xboard::setThreadTag(player->getID(), xboard::threadTag());

   // if (before.label != label) {
   //    LOG(INFO) << "[FlightAction] " << playerName
   //              << ": " << before.label << " -> " << label
   //              << "  (hdg=" << command.headingDeg
   //              << "deg alt=" << command.altitudeM
   //              << "m vel=" << command.speedKts << "kt)";
   // }

   return true;
}

} // namespace xC_130
} // namespace models
} // namespace mixr
