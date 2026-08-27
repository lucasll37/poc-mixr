#include "app/StatusReport.hpp"

#include "xnative/AlertDatalink.hpp"
#include "xnative/BehaviorBoard.hpp"
#include "xnative/TrackQuery.hpp"

#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/Autopilot.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace app {

namespace {

const double M2NM{1.0 / 1852.0};

void appendFlightLine(std::ostringstream& oss, const mixr::models::AirVehicle* const air)
{
   oss << "   " << std::setw(8) << std::left
       << (air->getName() != nullptr ? air->getName()->getString() : "?")
       << std::right << std::setprecision(0)
       << " alt=" << std::setw(5) << air->getAltitudeM() << "m"
       << " hdg=" << std::setw(3) << air->getHeadingD() << "deg"
       << " roll=" << std::setw(4) << air->getRollD() << "deg"
       << " spd=" << std::setw(3) << air->getTotalVelocityKts() << "kt";

   // Telemetria do 6-DOF pela interface NATIVA do AirVehicle
   const double fuelMax{air->getFuelWtMax()};
   double thrust[4]{};
   const int nEng{air->getEngThrust(thrust, 4)};
   oss << std::setprecision(0) << " thrust=" << (nEng > 0 ? (thrust[0] + thrust[1]) : 0.0) << "lb"
       << std::setprecision(2) << " mach=" << air->getMach()
       << std::setprecision(1) << " g=" << air->getGload()
       << std::setprecision(0)
       << " fuel=" << ((fuelMax > 0.0) ? (air->getFuelWt() / fuelMax * 100.0) : 0.0) << "%";
}

void appendDecisionLine(std::ostringstream& oss, const mixr::models::AirVehicle* const air)
{
   // Rotulo do comportamento: quadro de status por id (o Aircraft nativo nao
   // tem onde guardar isso) -- ver xnative/BehaviorBoard.hpp
   oss << " bt=" << std::setw(8) << std::left
       << mixr::xnative::getBehaviorLabel(air->getID()) << std::right;

   const auto autopilot = dynamic_cast<const mixr::models::Autopilot*>(air->getPilot());
   if (autopilot != nullptr) {
      oss << std::setprecision(0)
          << " ap(hdg=" << autopilot->getCommandedHeadingD()
          << ",alt=" << autopilot->getCommandedAltitudeFt() << "ft"
          << ",spd=" << autopilot->getCommandedVelocityKts() << ")";
   }
}

void appendSensorLine(std::ostringstream& oss, const mixr::models::AirVehicle* const air)
{
   const mixr::xnative::TrackInfo track{mixr::xnative::nearestHostileTrack(air)};
   if (track.found) {
      oss << " pista=" << track.name << "@" << std::setprecision(1)
          << (track.rangeM * M2NM) << "NM";
   }

   const auto datalink = dynamic_cast<const mixr::xnative::AlertDatalink*>(air->getDatalink());
   if (datalink != nullptr) {
      const auto alert = datalink->getAlert();
      if (alert.valid) {
         oss << " alerta<-" << alert.senderName << "(" << alert.contactName << ")";
      }
   }
}

} // namespace

void printStatus(const Fleet& fleet, const double elapsedSec, const double simSec,
                 const std::string& clockLabel)
{
   std::ostringstream oss;
   oss << std::fixed;

   oss << "[t=" << std::setprecision(0) << elapsedSec << "s"
       << " sim=" << std::setprecision(1) << simSec << "s" << std::setprecision(0)
       << " " << clockLabel << "]" << std::endl;

   for (const auto air : fleet) {
      if (air == nullptr) continue;
      appendFlightLine(oss, air);
      appendDecisionLine(oss, air);
      appendSensorLine(oss, air);
      oss << std::endl;
   }

   std::cout << oss.str();
}

} // namespace app
