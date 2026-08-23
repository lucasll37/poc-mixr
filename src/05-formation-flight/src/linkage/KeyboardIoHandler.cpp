#include "linkage/KeyboardIoHandler.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/system/Autopilot.hpp"
#include "mixr/terrain/Terrain.hpp"
#include "mixr/base/concepts/linkage/AbstractIoData.hpp"
#include "mixr/base/units/Distances.hpp"

#include "configs/channel_map.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace keyboard {

IMPLEMENT_SUBCLASS(KeyboardIoHandler, "KeyboardIoHandler")
EMPTY_SLOTTABLE(KeyboardIoHandler)
EMPTY_DELETEDATA(KeyboardIoHandler)

KeyboardIoHandler::KeyboardIoHandler()
{
   STANDARD_CONSTRUCTOR()
}

void KeyboardIoHandler::copyData(const KeyboardIoHandler& org, const bool)
{
   BaseClass::copyData(org);
   // formationState e pendingEvent sao estado de execucao, nao configuracao
   // -- nao ha nada relevante pra copiar aqui.
}

std::string KeyboardIoHandler::consumePendingEvent()
{
   std::string ev;
   ev.swap(pendingEvent);
   return ev;
}

void KeyboardIoHandler::inputDevicesImpl(const double dt)
{
   readDeviceInputs(dt);

   const mixr::base::AbstractIoData* const inData{getInputData()};
   if (inData == nullptr) return;

   const auto station = static_cast<mixr::simulation::Station*>(
      findContainerByType(typeid(mixr::simulation::Station)));
   if (station == nullptr) return;

   const auto lead = dynamic_cast<mixr::models::Player*>(station->getOwnship());
   if (lead == nullptr) return;

   mixr::models::Autopilot* ap{};
   {
      mixr::base::Pair* p{lead->getPilotByType(typeid(mixr::models::Autopilot))};
      if (p != nullptr) ap = static_cast<mixr::models::Autopilot*>(p->object());
   }
   if (ap == nullptr) return;

   if (!initialized) {
      // primeiro tick: parte do estado atual do lider, nao de zero
      cmdHeadingDeg = lead->getHeadingD();
      cmdAltitudeFt = lead->getAltitudeFt();
      cmdVelocityKts = lead->getTotalVelocityKts();
      ap->setHeadingHoldMode(true);
      ap->setAltitudeHoldMode(true);
      ap->setVelocityHoldMode(true);
      initialized = true;
   }

   bool v{};

   if (inData->getDiscreteInput(HDG_LEFT, &v) && v)  cmdHeadingDeg -= kHeadingRateDps * dt;
   if (inData->getDiscreteInput(HDG_RIGHT, &v) && v) cmdHeadingDeg += kHeadingRateDps * dt;
   if (cmdHeadingDeg < 0.0) cmdHeadingDeg += 360.0;
   if (cmdHeadingDeg >= 360.0) cmdHeadingDeg -= 360.0;

   if (inData->getDiscreteInput(ALT_UP, &v) && v)   cmdAltitudeFt += (kAltitudeRateFpm / 60.0) * dt;
   if (inData->getDiscreteInput(ALT_DOWN, &v) && v) cmdAltitudeFt = std::max(0.0, cmdAltitudeFt - (kAltitudeRateFpm / 60.0) * dt);

   if (inData->getDiscreteInput(SPD_UP, &v) && v)   cmdVelocityKts += kVelocityRateKtps * dt;
   if (inData->getDiscreteInput(SPD_DOWN, &v) && v) cmdVelocityKts = std::max(0.0, cmdVelocityKts - kVelocityRateKtps * dt);

   // rede de seguranca: nunca comandar abaixo do terreno + margem no ponto atual
   if (terrain != nullptr) {
      double elevM{};
      if (terrain->getElevation(&elevM, lead->getLatitude(), lead->getLongitude(), true)) {
         const double minAltFt{(elevM + 150.0) * mixr::base::distance::M2FT};
         if (cmdAltitudeFt < minAltFt) cmdAltitudeFt = minAltFt;
      }
   }

   ap->setCommandedHeadingD(cmdHeadingDeg);
   ap->setCommandedAltitudeFt(cmdAltitudeFt);
   ap->setCommandedVelocityKts(cmdVelocityKts);

   if (formationState != nullptr) {
      formations::Formation newFormation{formationState->current};
      if (inData->getDiscreteInput(FORM_1, &v) && v) newFormation = formations::Formation::TRAIL;
      if (inData->getDiscreteInput(FORM_2, &v) && v) newFormation = formations::Formation::WEDGE;
      if (inData->getDiscreteInput(FORM_3, &v) && v) newFormation = formations::Formation::LINE;
      if (inData->getDiscreteInput(FORM_4, &v) && v) newFormation = formations::Formation::VIC;

      if (newFormation != formationState->current) {
         formationState->current = newFormation;
         std::ostringstream oss;
         oss << "Formation changed to " << formations::formationName(newFormation);
         queueEvent(oss.str());
         std::cout << "[keyboard] " << oss.str() << std::endl;
      }

      if (!formationState->rtbEngaged && inData->getDiscreteInput(RTB, &v) && v) {
         formationState->rtbEngaged = true;
         ap->setNavMode(true);
         queueEvent("RTB engaged");
         std::cout << "[keyboard] RTB engaged - lead switching to nav mode" << std::endl;
      }
   }
}

} // namespace keyboard
