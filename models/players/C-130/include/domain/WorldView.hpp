#pragma once

#include <string>

namespace mixr {
namespace models {
namespace xC_130 {
namespace domain {

//------------------------------------------------------------------------------
// WorldView -- a visao do mundo de UM frame, em numeros crus.
//
// Versao ENXUTA do domain::WorldView de models/players/A-4: este modelo so
// navega (nao ha combate, nao ha sensor proprio, nao ha datalink, nao ha
// arma) -- entao nao ha contato/alerta/weaponReady aqui. So o necessario
// para ubf::FlightState perceber o ator e para bt::NavigateAction seguir a
// Route/Steerpoint nativa.
//
// Struct de dados, sem comportamento: a fronteira entre "ler o mundo" e
// "decidir" e justamente nao deixar tipo de framework atravessar.
//------------------------------------------------------------------------------
struct WorldView
{
   bool valid{};

   std::string ownerName;   // nome EDL do ator
   double northM{};
   double eastM{};
   double altitudeM{};
   double headingDeg{};
   double speedKts{};

   // navegacao NATIVA (Navigation/Route/Steerpoint) -- so leitura. Quem
   // calcula estes numeros e mixr::models::Navigation::updateNavSteering()
   // (chamado todo frame de background, independente do navMode do
   // Autopilot) -- o MESMO dado que Autopilot::processModeNavigation()
   // consultaria se navMode estivesse ligado. Aqui eles so atravessam a
   // fronteira do framework, sem tipo do MIXR.
   bool hasNavSteering{};    // Navigation::isNavSteeringValid()
   double navTrueBrgDeg{};   // rumo verdadeiro ate o steerpoint "to"
   bool hasNavCmdAlt{};      // Steerpoint::isCmdAltValid()
   double navCmdAltM{};      // Steerpoint::getCmdAltitudeM()
   bool hasNavCmdSpeed{};    // Steerpoint::isCmdAirspeedValid()
   double navCmdSpeedKts{};  // Steerpoint::getCmdAirspeedKts()
};

} // namespace domain
} // namespace xC_130
} // namespace models
} // namespace mixr
