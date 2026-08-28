#include "app/DeterministicDump.hpp"

#include "xnative/AlertDatalink.hpp"
#include "xnative/BehaviorBoard.hpp"
#include "xnative/FlightAgentTC.hpp"
#include "xnative/TrackQuery.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace app {

void printDeterministicDump(const Fleet& fleet, const long frame)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(9);

   for (const auto air : fleet) {
      if (air == nullptr) continue;

      const mixr::base::Vec3d& pos{air->getPosition()};
      const auto datalink = dynamic_cast<const mixr::xnative::AlertDatalink*>(air->getDatalink());
      const mixr::xnative::TrackInfo track{mixr::xnative::nearestHostileTrack(air)};
      const mixr::xnative::FlightAgentTC* const agent{mixr::xnative::findFlightAgent(air)};

      oss << "frame=" << frame
          << " player=" << (air->getName() != nullptr ? air->getName()->getString() : "?")
          << " n=" << pos[mixr::models::Player::INORTH]
          << " e=" << pos[mixr::models::Player::IEAST]
          << " alt=" << air->getAltitudeM()
          // Elevacao e AGL sao estado da simulacao (consulta ao banco de
          // elevacao, imutavel depois de carregado) -- entram no dump pelo
          // mesmo criterio do resto: e por eles que se prova que a consulta
          // ao terreno da o MESMO resultado com 1, 2 e 4 threads T/C.
          << " elev=" << air->getTerrainElevationM()
          << " agl=" << air->getAltitudeAglM()
          << " hdg=" << air->getHeadingD()
          << " roll=" << air->getRollD()
          << " pitch=" << air->getPitchD()
          << " spd=" << air->getTotalVelocity()
          << " mach=" << air->getMach()
          << " fuel=" << air->getFuelWt()
          << " bt=" << mixr::xnative::getBehaviorLabel(air->getID())
          << " track=" << (track.found ? track.name : std::string("none"))
          << " trackRange=" << track.rangeM
          << " alert=" << ((datalink != nullptr && datalink->hasAlert())
                            ? datalink->getAlert().senderName : std::string("none"))
          << " sent=" << (datalink != nullptr ? datalink->getSentCount() : 0L)
          << " recv=" << (datalink != nullptr ? datalink->getReceivedCount() : 0L)
          // Contagem de decisoes: entra no dump de proposito. Se a decisao
          // esta mesmo amarrada ao frame, este numero tem que ser identico
          // com 1, 2 ou 4 threads -- e igual ao numero de frames. O indice
          // da thread NAO entra: depende do escalonador.
          << " dec=" << (agent != nullptr ? agent->getDecisionCount() : 0L)
          << std::endl;
   }

   std::cout << oss.str();
}

} // namespace app
