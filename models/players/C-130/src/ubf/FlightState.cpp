#include "ubf/FlightState.hpp"

#include "mixr/models/navigation/Navigation.hpp"
#include "mixr/models/navigation/Route.hpp"
#include "mixr/models/navigation/Steerpoint.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

IMPLEMENT_SUBCLASS(FlightState, "C130FlightState")
EMPTY_SLOTTABLE(FlightState)
EMPTY_DELETEDATA(FlightState)

FlightState::FlightState()
{
   STANDARD_CONSTRUCTOR()
}

// Copiada campo a campo, nao EMPTY_COPYDATA -- 'snap' e a ultima leitura,
// nao so cache descartavel; um clone() deste objeto tem de preservar o que
// ja foi percebido.
void FlightState::copyData(const FlightState& org, const bool)
{
   BaseClass::copyData(org);
   snap = org.snap;
}

//------------------------------------------------------------------------------
// updateState() -- percepcao, lendo SO acessores nativos. Sem contato/
// alerta/arma/radar: este modelo so navega.
//------------------------------------------------------------------------------
void FlightState::updateState(const base::Component* const actor)
{
   BaseClass::updateState(actor);

   const auto air = dynamic_cast<const models::AirVehicle*>(actor);
   if (air == nullptr) {
      snap = Snapshot{};
      return;
   }

   Snapshot s;
   s.valid = true;

   // 'actor' chega aqui resolvido pelo Agent nativo (FlightAgentTC, aninhado
   // no proprio player) -- e o unico jeito confiavel de saber "quem decide".
   const char* const rawOwnerName =
      (air->getName() != nullptr) ? air->getName()->getString() : nullptr;
   s.ownerName = (rawOwnerName != nullptr) ? rawOwnerName : "";

   const base::Vec3d& pos{air->getPosition()};
   s.northM = pos[models::Player::INORTH];
   s.eastM = pos[models::Player::IEAST];
   s.altitudeM = air->getAltitudeM();
   s.headingDeg = air->getHeadingD();
   s.speedKts = air->getTotalVelocityKts();

   // --- navegacao NATIVA: Route/Steerpoint, so leitura ---
   //
   // mixr::models::Route::updateData() (chamado todo frame de BACKGROUND,
   // incondicionalmente) atualiza os dados de guiagem de CADA steerpoint e
   // sequencia a rota por DISTANCIA, independente do navMode do Autopilot.
   // Navigation::updateNavSteering() copia o rumo/alcance do steerpoint "to"
   // para os proprios campos da Navigation -- e o MESMO dado que
   // Autopilot::processModeNavigation() consultaria se navMode estivesse
   // ligado. Aqui a leitura e identica, so que por FORA do Autopilot -- quem
   // decide o que fazer com ela e a arvore (bt::NavigateAction).
   const auto nav = air->getNavigation();
   if (nav != nullptr) {
      s.hasNavSteering = nav->isNavSteeringValid();
      s.navTrueBrgDeg = nav->getTrueBrgDeg();
      const auto route = nav->getPriRoute();
      const auto steerpoint = (route != nullptr) ? route->getSteerpoint() : nullptr;
      if (steerpoint != nullptr) {
         s.hasNavCmdAlt = steerpoint->isCmdAltValid();
         s.navCmdAltM = steerpoint->getCmdAltitudeM();
         s.hasNavCmdSpeed = steerpoint->isCmdAirspeedValid();
         s.navCmdSpeedKts = steerpoint->getCmdAirspeedKts();
      }
   }

   snap = s;
}

} // namespace xC_130
} // namespace models
} // namespace mixr
