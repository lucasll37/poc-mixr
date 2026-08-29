#ifndef __xnative_RadarScan_H__
#define __xnative_RadarScan_H__

namespace mixr {
namespace models { class AirVehicle; }

namespace xnative {

//------------------------------------------------------------------------------
// Consulta ao radar NATIVO: para onde a antena esta apontando AGORA.
//
// Diferente de TrackQuery (que le o CONTATO detectado), isto le o proprio
// Gimbal -- ja vivo e configurado em EDL (antennas: ( Gimbal components: {
// radar: ( Antenna ... ) } )) -- para alimentar a varredura de radar do
// Tacview (RadarAzimuth/RadarElevation/RadarRange/RadarHorizontalBeamwidth/
// RadarVerticalBeamwidth). Nao ha nada para simular: e leitura direta de
// Gimbal::getAzimuthD()/getElevationD()/getMaxRange2PlayersOfInterest() e
// ScanGimbal::getScanVolumeD().
//
// O caminho e sempre o mesmo do framework:
//    AirVehicle -> Gimbal (nome "radar", ver scenario.epp.in)
//------------------------------------------------------------------------------
struct RadarScanInfo
{
   bool found{};
   double azimuthDeg{};              // relativo ao proprio player (nariz = 0, direita positivo)
   double elevationDeg{};
   double rangeM{};                  // alcance configurado do radar (Gimbal::getMaxRange2PlayersOfInterest)
   double horizontalBeamwidthDeg{};  // abertura do feixe (ScanGimbal::getScanVolumeD)
   double verticalBeamwidthDeg{};
};

RadarScanInfo radarScanOf(const models::AirVehicle* air);

} // namespace xnative
} // namespace mixr

#endif
