#include "app/TelemetryJson.hpp"

#include "xboard/Board.hpp"
#include "xtrack/TrackQuery.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/util/nav_utils.hpp"

namespace app {

namespace {

using mixr::models::Player;

// Duplicado de proposito (nao compartilhado com app/src/app/FleetPanel.cpp):
// os dois subprojetos nao linkam codigo entre si, convencao do repo.
std::string sideLabel(const unsigned int side)
{
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

std::string modeLabel(const int mode)
{
   switch (mode) {
      case Player::INACTIVE:       return "INACTIVE";
      case Player::ACTIVE:         return "ACTIVE";
      case Player::KILLED:         return "KILLED";
      case Player::CRASHED:        return "CRASHED";
      case Player::DETONATED:      return "DETONATED";
      case Player::PRE_RELEASE:    return "PRE_RELEASE";
      case Player::DELETE_REQUEST: return "DELETE_REQUEST";
      default:                     return "?";
   }
}

std::vector<std::string> majorTypeLabels(const unsigned int majorType)
{
   std::vector<std::string> out;
   if (majorType & static_cast<unsigned int>(Player::AIR_VEHICLE))    out.push_back("AIR_VEHICLE");
   if (majorType & static_cast<unsigned int>(Player::GROUND_VEHICLE)) out.push_back("GROUND_VEHICLE");
   if (majorType & static_cast<unsigned int>(Player::WEAPON))         out.push_back("WEAPON");
   if (majorType & static_cast<unsigned int>(Player::SHIP))           out.push_back("SHIP");
   if (majorType & static_cast<unsigned int>(Player::SPACE_VEHICLE))  out.push_back("SPACE_VEHICLE");
   if (majorType & static_cast<unsigned int>(Player::BUILDING))       out.push_back("BUILDING");
   if (majorType & static_cast<unsigned int>(Player::LIFE_FORM))      out.push_back("LIFE_FORM");
   if (out.empty()) out.push_back("GENERIC");
   return out;
}

} // namespace

nlohmann::json playerToJson(mixr::models::Player* const player,
                            const double refLatDeg, const double refLonDeg)
{
   nlohmann::json j;

   j["name"] = (player->getName() != nullptr) ? player->getName()->getString() : "?";
   const mixr::base::String* const type{player->getType()};
   j["type"] = (type != nullptr && type->len() > 0) ? std::string(type->getString()) : std::string("?");
   j["side"] = sideLabel(static_cast<unsigned int>(player->getSide()));
   j["majorType"] = majorTypeLabels(player->getMajorType());
   j["mode"] = modeLabel(player->getMode());

   const mixr::base::Vec3d& pos{player->getPosition()};
   const double northM{pos[Player::INORTH]};
   const double eastM{pos[Player::IEAST]};

   double latDeg{};
   double lonDeg{};
   double llAlt{};
   const bool haveLL{mixr::base::nav::convertPosVec2llE(
      refLatDeg, refLonDeg, mixr::base::Vec3d(northM, eastM, 0.0), &latDeg, &lonDeg, &llAlt)};

   nlohmann::json position;
   position["northM"] = northM;
   position["eastM"] = eastM;
   if (haveLL) {
      position["latDeg"] = latDeg;
      position["lonDeg"] = lonDeg;
   } else {
      position["latDeg"] = nullptr;
      position["lonDeg"] = nullptr;
   }
   j["position"] = position;

   j["altitudeM"] = player->getAltitudeM();
   j["terrainElevationM"] = player->getTerrainElevationM();
   j["altitudeAglM"] = player->getAltitudeAglM();
   j["headingDeg"] = player->getHeadingD();
   j["rollDeg"] = player->getRollD();
   j["pitchDeg"] = player->getPitchD();
   j["speedMps"] = player->getTotalVelocity();
   j["mach"] = player->getMach();

   // Bloco exclusivo de AirVehicle -- combustivel e conceito aerodinamico,
   // sem equivalente generico em Player (mesmo raciocinio de
   // app/src/app/DashboardState.cpp).
   if (const auto* const air = dynamic_cast<const mixr::models::AirVehicle*>(player)) {
      j["fuelWtLbs"] = air->getFuelWt();

      const mixr::xtrack::TrackInfo track{mixr::xtrack::nearestHostileTrack(air)};
      nlohmann::json trackJson;
      trackJson["found"] = track.found;
      if (track.found) {
         trackJson["name"] = track.name;
         trackJson["rangeM"] = track.rangeM;
         trackJson["relBearingDeg"] = track.relBearingDeg;
      }
      j["track"] = trackJson;
   }

   // shared/xboard -- ja agnostico a modelo (ver o cabecalho de Board.hpp):
   // so preenche se ALGUEM escreveu para este id; um player so-Autopilot
   // (sem agente) nunca chama xboard::set*, e o Readout default ("--", 0
   // decisoes) e o valor honesto para "nao decide nada".
   const mixr::xboard::Readout board{mixr::xboard::get(player->getID())};
   j["behaviorLabel"] = board.label;
   j["decisions"] = board.decisions;
   j["threadTag"] = board.threadTag;

   nlohmann::json alert;
   alert["active"] = board.alertValid;
   if (board.alertValid) alert["sender"] = board.alertSender;
   j["alert"] = alert;

   return j;
}

nlohmann::json fleetToJson(const std::vector<mixr::models::Player*>& players,
                           const double refLatDeg, const double refLonDeg)
{
   // '=', NAO '{}': 'nlohmann::json arr{nlohmann::json::array()}' cai no
   // construtor de initializer_list e embrulha o array vazio dentro de OUTRO
   // array (arr vira [[]], nao []) -- armadilha classica da lib, medida
   // rodando (o primeiro elemento de 'players' saia '[]' na resposta).
   nlohmann::json arr = nlohmann::json::array();
   for (const auto player : players) {
      if (player != nullptr) arr.push_back(playerToJson(player, refLatDeg, refLonDeg));
   }
   return arr;
}

} // namespace app
