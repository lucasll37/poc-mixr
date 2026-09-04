#include "xnative/RadarScan.hpp"

#include "mixr/models/player/air/AirVehicle.hpp"
#include "mixr/models/system/Gimbal.hpp"
#include "mixr/models/system/ScanGimbal.hpp"

namespace mixr {
namespace xnative {

namespace {
const char* const RADAR_GIMBAL_NAME{"radar"};
}

RadarScanInfo radarScanOf(const models::AirVehicle* const air)
{
   RadarScanInfo info;
   if (air == nullptr) return info;

   // getGimbalByName() nao e const no framework, mas a consulta e de
   // leitura -- mesmo raciocinio do const_cast em TrackQuery.cpp.
   models::Gimbal* const gimbal{const_cast<models::AirVehicle*>(air)->getGimbalByName(RADAR_GIMBAL_NAME)};
   if (gimbal == nullptr) return info;

   info.found = true;
   info.azimuthDeg = gimbal->getAzimuthD();
   info.elevationDeg = gimbal->getElevationD();
   info.rangeM = gimbal->getMaxRange2PlayersOfInterest();

   if (const auto* const scanGimbal = dynamic_cast<const models::ScanGimbal*>(gimbal)) {
      scanGimbal->getScanVolumeD(&info.horizontalBeamwidthDeg, &info.verticalBeamwidthDeg);
   }

   return info;
}

} // namespace xnative
} // namespace mixr
